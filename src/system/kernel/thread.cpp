/*
 * Copyright 2025, Bill Hayden, hayden@haydentech.com
 * Distributed under the terms of the MIT License.
 */


/*! Threading routines */


// Platform-specific system headers
#ifdef _WIN32
	#include <windows.h>
	#include <process.h>  // For getpid() on Windows
	// Define missing POSIX signals on Windows
	#ifndef SIGSTOP
	#define SIGSTOP 19
	#endif
	#ifndef SIGCONT
	#define SIGCONT 18
	#endif
	#ifndef SIGKILL
	#define SIGKILL 9
	#endif
	// Windows sleep wrapper
	static inline int usleep(unsigned int usec) {
		Sleep(usec / 1000);
		return 0;
	}
	// Stub kill() for Windows (not implemented)
	static inline int kill(int pid, int sig) {
		return -1;
	}
#else
	#include <unistd.h>   // For getpid(), fork(), etc. on POSIX
#endif

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <OS.h>
#ifndef _WIN32
#include <sys/mman.h>
#endif

//#define TRACE_THREAD
#ifdef TRACE_THREAD
#	define TRACE(x) dprintf x
#else
#	define TRACE(x) ;
#endif


// This thread implementation has a 1-to-1 relationship between thread_id and the index
// into the thread table, but none of the functions assume that, in case that might
// change in the future.

/* Todo: Compute based on the amount of available memory. */
#define MAX_THREADS 128

const thread_id FREE_SLOT = -1;

typedef void* (*pthread_entry) (void*);

// Shared memory structure for cross-process thread synchronization
struct thread_sync_data {
	int32 initialized;  // 0=uninitialized, 1=initializing, 2=ready
	pthread_mutex_t mutex;
	pthread_cond_t cond;
};

thread_info *thread_table = NULL;
static struct thread_sync_data *thread_sync = NULL;

static status_t init_thread(void);
static void teardown_threads(void);
static void* thread_wrapper(void* arg);
static void remove_thread_table_entry(thread_id id);
static void atfork_prepare_handler(void);
static void atfork_parent_handler(void);
static void atfork_child_handler(void);

// Thread table access is protected by thread_sync->mutex

static status_t
init_thread(void)
{
	// Quick check if already initialized
	if (thread_table && thread_sync && 
	    __atomic_load_n(&thread_sync->initialized, __ATOMIC_ACQUIRE) == 2)
		return B_OK;

	// Allocate shared memory for thread table
	if (!thread_table) {
#ifdef _WIN32
		// On Windows, use regular malloc (no cross-process support for now)
		thread_table = (thread_info*)calloc(MAX_THREADS, sizeof(thread_info));
		if (!thread_table) {
			printf("FATAL: Couldn't create thread table: %s\n", strerror(errno));
			return B_ERROR;
		}
#else
		thread_table = (thread_info*)mmap(NULL, sizeof(thread_info) * MAX_THREADS,
										  PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		if (thread_table == MAP_FAILED) {
			printf("FATAL: Couldn't create thread table: %s\n", strerror(errno));
			return B_ERROR;
		}
#endif
	}

	// Allocate shared memory for synchronization
	if (!thread_sync) {
#ifdef _WIN32
		// On Windows, use regular malloc (no cross-process support for now)
		thread_sync = (struct thread_sync_data*)calloc(1, sizeof(struct thread_sync_data));
		if (!thread_sync) {
			printf("FATAL: Couldn't create thread sync data: %s\n", strerror(errno));
			return B_ERROR;
		}
#else
		thread_sync = (struct thread_sync_data*)mmap(NULL, sizeof(struct thread_sync_data),
													  PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		if (thread_sync == MAP_FAILED) {
			printf("FATAL: Couldn't create thread sync data: %s\n", strerror(errno));
			return B_ERROR;
		}
#endif
	}

	// Atomically claim initialization (0->1)
	int32 expected = 0;
	if (__atomic_compare_exchange_n(&thread_sync->initialized, &expected, 1,
									false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
		// We won - initialize the structures
		pthread_mutexattr_t mattr;
		pthread_mutexattr_init(&mattr);
		pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
		pthread_mutex_init(&thread_sync->mutex, &mattr);
		pthread_mutexattr_destroy(&mattr);

		pthread_condattr_t cattr;
		pthread_condattr_init(&cattr);
		pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED);
		pthread_cond_init(&thread_sync->cond, &cattr);
		pthread_condattr_destroy(&cattr);

		// Initialize thread table
		for (thread_id i = 0; i < MAX_THREADS; i++)
			thread_table[i].thread = FREE_SLOT;

		// Mark as ready (1->2)
		__atomic_store_n(&thread_sync->initialized, 2, __ATOMIC_RELEASE);
	} else {
		// Wait for initialization to complete
		while (__atomic_load_n(&thread_sync->initialized, __ATOMIC_ACQUIRE) != 2)
			usleep(100);
	}

	static bool handlers_registered = false;
	if (!handlers_registered) {
#ifndef _WIN32
		pthread_atfork(atfork_prepare_handler, atfork_parent_handler,
			atfork_child_handler);
#endif
		atexit(teardown_threads);
		handlers_registered = true;
	}

	return B_OK;
}


static void
remove_thread_table_entry(thread_id id)
{
	// All sanity checks should be done by the caller
	if (!thread_sync)
		return;

	pthread_mutex_lock(&thread_sync->mutex);
	thread_table[id].thread = FREE_SLOT;
	thread_table[id].team = 0;
	thread_table[id].buffer_allocation = 0;
	thread_table[id].buffer[0] = '\0';
	thread_table[id].sender = 0;
	pthread_mutex_unlock(&thread_sync->mutex);
}


// Wrapper function that ensures thread cleanup happens on normal exit
static void*
thread_wrapper(void* arg)
{
	thread_id id = (thread_id)(uintptr_t)arg;
	
	// Get the actual function and data from the thread table
	pthread_mutex_lock(&thread_sync->mutex);
	thread_func func = thread_table[id].func;
	void* data = thread_table[id].data;
	pthread_mutex_unlock(&thread_sync->mutex);
	
	// Call the user's thread function
	status_t result = func(data);
	
	// NOTE: Do NOT remove thread table entry here!
	// It will be removed by wait_for_thread after pthread_join completes.
	// Removing it here would cause wait_for_thread to fail.
	
	return (void*)(uintptr_t)result;
}


thread_id
spawn_thread(thread_func func, const char *name, int32 priority, void *data)
{
	init_thread();

	pthread_mutex_lock(&thread_sync->mutex);
	for (thread_id i = 0; i < MAX_THREADS; i++)
	{
		if (thread_table[i].thread == FREE_SLOT)
		{
			if (!name)
				name = "no-name thread";

			thread_table[i].pth = 0; //not in the POSIX system yet
			thread_table[i].thread = i;
			thread_table[i].team = getpid();
			thread_table[i].priority = priority;
			thread_table[i].state = B_THREAD_SPAWNED;
			strncpy(thread_table[i].name, name, B_OS_NAME_LENGTH);
			thread_table[i].name[B_OS_NAME_LENGTH - 1] = '\0';
			thread_table[i].func = func;
			thread_table[i].data = data;
			thread_table[i].code = 0;
			thread_table[i].sender = 0;
			thread_table[i].buffer[0] = '\0';
			thread_table[i].buffer_allocation = 0;

			pthread_mutex_unlock(&thread_sync->mutex);
			return i;
		}
	}
	pthread_mutex_unlock(&thread_sync->mutex);

	return B_ERROR;
}


status_t
kill_thread(thread_id thread)
{
	init_thread();

	if (thread < 0)
		return B_BAD_THREAD_ID;

	pthread_mutex_lock(&thread_sync->mutex);
	for (thread_id i = 0; i < MAX_THREADS; i++)
	{
		if (thread_table[i].thread == thread)
		{
			pthread_t pth = thread_table[i].pth;
			pthread_mutex_unlock(&thread_sync->mutex);
			if (pthread_cancel(pth) == 0)
			{
				// Wait for the thread to actually terminate
				pthread_join(pth, NULL);
				remove_thread_table_entry(i);
				return B_OK;
			}
			return B_BAD_THREAD_ID;
		}
	}
	pthread_mutex_unlock(&thread_sync->mutex);

	return B_BAD_THREAD_ID;
}


status_t
rename_thread(thread_id thread, const char *newName)
{
	init_thread();

	if (thread < 0)
		return B_BAD_THREAD_ID;

	pthread_mutex_lock(&thread_sync->mutex);
	for (thread_id i = 0; i < MAX_THREADS; i++)
	{
		if (thread_table[i].thread == thread)
		{
			strncpy(thread_table[i].name, newName, B_OS_NAME_LENGTH);
			thread_table[i].name[B_OS_NAME_LENGTH - 1] = '\0';

			pthread_mutex_unlock(&thread_sync->mutex);
			return B_OK;
		}
	}
	pthread_mutex_unlock(&thread_sync->mutex);

	return B_BAD_THREAD_ID;
}


void
exit_thread(status_t status)
{
	pthread_t this_thread = pthread_self();
	
	init_thread();
	
	pthread_mutex_lock(&thread_sync->mutex);
	for (thread_id i = 0; i < MAX_THREADS; i++)
	{
		if (pthread_equal(thread_table[i].pth, this_thread))
		{
			pthread_mutex_unlock(&thread_sync->mutex);
			remove_thread_table_entry(i);
			break;
		}
	}
	pthread_mutex_unlock(&thread_sync->mutex);
	
	pthread_exit((void *) &status);
}


status_t
on_exit_thread(void (*callback)(void *), void *data)
{
	return B_NO_MEMORY;
}


status_t
send_data(thread_id thread, int32 code, const void *buffer, size_t buffer_size)
{
	init_thread();

	if (thread < 0)
		return B_BAD_THREAD_ID;

	if (buffer_size > THREAD_BUFFER_SIZE)
		return B_NO_MEMORY;

	//printf("send_data(to thread %d, code %d, size %ld)\n", thread, code, buffer_size);

	thread_id this_thread = find_thread(NULL);

	pthread_mutex_lock(&thread_sync->mutex);
	for (thread_id i = 0; i < MAX_THREADS; i++)
	{
		if (thread_table[i].thread == thread)
		{
			//printf("send_data: sending now, potentially blocking\n");

			// Wait until previous message is consumed
			while (thread_table[i].buffer_allocation || thread_table[i].code) {
				pthread_cond_wait(&thread_sync->cond, &thread_sync->mutex);
			}

			//printf("send_data: sending, past block\n");

			thread_table[i].code = code;
			thread_table[i].sender = this_thread;

			if (buffer)
			{
				memcpy(thread_table[i].buffer, buffer, buffer_size);
				thread_table[i].buffer_allocation = buffer_size;
			}

			// Signal waiting receiver
			pthread_cond_broadcast(&thread_sync->cond);
			pthread_mutex_unlock(&thread_sync->mutex);
			return B_OK;
		}
	}
	pthread_mutex_unlock(&thread_sync->mutex);

	return B_BAD_THREAD_ID;
}


void teardown_threads()
{
	int count = 0;
	pid_t this_process = getpid();
	
	/* Free thread table entries created by our process */
	for (thread_id i = 0; i < MAX_THREADS; i++)
	{
		if (thread_table[i].team == this_process)
		{
			thread_table[i].thread = FREE_SLOT;
			thread_table[i].team = 0;
			//if (thread_table[i].buffer)
			//	free(thread_table[i].buffer);
			thread_table[i].buffer[0] = '\0';
			count++;
		}
	}
	
	//printf("teardown_threads(): %d threads deleted\n", count);
}


static void
atfork_prepare_handler(void)
{
	if (thread_sync
		&& __atomic_load_n(&thread_sync->initialized, __ATOMIC_ACQUIRE) == 2) {
		pthread_mutex_lock(&thread_sync->mutex);
	}
}


static void
atfork_parent_handler(void)
{
	if (thread_sync
		&& __atomic_load_n(&thread_sync->initialized, __ATOMIC_ACQUIRE) == 2) {
		pthread_mutex_unlock(&thread_sync->mutex);
	}
}


static void
atfork_child_handler(void)
{
	// After fork(), the child process has a new PID and new pthread_t for main thread.
	// We need to register the child's main thread in a new slot in the shared table.
	// The parent's thread entries remain unchanged so messaging can work cross-process.
	
	if (thread_table && thread_sync &&
	    __atomic_load_n(&thread_sync->initialized, __ATOMIC_ACQUIRE) == 2) {
		
		pid_t child_pid = getpid();
		pthread_t child_pthread = pthread_self();
		
		// The mutex is already locked by atfork_prepare_handler() in the forking
		// thread. Keep it locked while updating shared table state in child.
		for (thread_id i = 0; i < MAX_THREADS; i++) {
			if (thread_table[i].thread == FREE_SLOT) {
				// Register child's main thread
				thread_table[i].pth = child_pthread;
				thread_table[i].thread = i;
				thread_table[i].team = child_pid;
				thread_table[i].priority = B_NORMAL_PRIORITY;
				thread_table[i].state = B_THREAD_RUNNING;
				strncpy(thread_table[i].name, "main", B_OS_NAME_LENGTH);
				thread_table[i].name[B_OS_NAME_LENGTH - 1] = '\0';
				thread_table[i].func = NULL;
				thread_table[i].data = NULL;
				thread_table[i].code = 0;
				thread_table[i].sender = 0;
				thread_table[i].buffer[0] = '\0';
				thread_table[i].buffer_allocation = 0;
				pthread_mutex_unlock(&thread_sync->mutex);
				return;
			}
		}
		pthread_mutex_unlock(&thread_sync->mutex);
	}
}


status_t
receive_data(thread_id *sender, void *buffer, size_t bufferSize)
{
	init_thread();

	//printf("receive_data()\n");

	thread_id this_thread = find_thread(NULL);

	pthread_mutex_lock(&thread_sync->mutex);
	for (thread_id i = 0; i < MAX_THREADS; i++)
	{
		if (thread_table[i].thread == this_thread)
		{
			//printf("receive_data: found data in thread %d, potentially blocking\n", i);

			// Wait until data is available
			while (thread_table[i].buffer_allocation == 0 && thread_table[i].code == 0) {
				pthread_cond_wait(&thread_sync->cond, &thread_sync->mutex);
			}

			//printf("receive_data: found data in thread %d, past block\n", i);
			if (sender)
				*sender = thread_table[i].sender;

			int32 code = thread_table[i].code;
			size_t receiveSize = min_c(bufferSize, thread_table[i].buffer_allocation);
			if (receiveSize > 0)
				memcpy(buffer, thread_table[i].buffer, receiveSize);
			
			// Clear the inline buffer
			thread_table[i].buffer[0] = '\0';
			thread_table[i].buffer_allocation = 0;
			thread_table[i].code = 0;
			thread_table[i].sender = 0;
			
			// Signal waiting sender that buffer is now free
			pthread_cond_broadcast(&thread_sync->cond);
			pthread_mutex_unlock(&thread_sync->mutex);
			return code;
		}
	}
	pthread_mutex_unlock(&thread_sync->mutex);


	return B_BAD_THREAD_ID;
}


bool
has_data(thread_id thread)
{
	init_thread();

	if (thread < 0)
		return false;

	pthread_mutex_lock(&thread_sync->mutex);
	for (thread_id count = 0; count < MAX_THREADS; count++)
	{
		if (thread_table[count].thread == thread) {
			bool result = (thread_table[count].buffer_allocation > 0 || thread_table[count].code != 0);
			pthread_mutex_unlock(&thread_sync->mutex);
			return result;
		}
	}
	pthread_mutex_unlock(&thread_sync->mutex);

	return false;
}


status_t
_get_thread_info(thread_id id, thread_info *info, size_t size)
{
	init_thread();

	//printf("get_thread_info(%d)\n", id);

	if (info == NULL || size != sizeof(thread_info) || id < B_OK)
		return B_BAD_VALUE;

	pthread_mutex_lock(&thread_sync->mutex);
	for (thread_id i = 0; i < MAX_THREADS; i++)
	{
		if (thread_table[i].thread == id)
		{
			info->thread = id;
			strlcpy (info->name, thread_table[i].name, B_OS_NAME_LENGTH);
			info->name[B_OS_NAME_LENGTH - 1] = '\0';
			info->state = thread_table[i].state;
			info->priority = thread_table[i].priority;
			info->team = thread_table[i].team;
			pthread_mutex_unlock(&thread_sync->mutex);
			return B_OK;
		}
	}
	pthread_mutex_unlock(&thread_sync->mutex);

	return B_BAD_VALUE;
}


status_t
_get_next_thread_info(team_id teamID, int32 *_cookie, thread_info *info,
	size_t size)
{
	if (info == NULL || size != sizeof(thread_info) || teamID < 0)
		return B_BAD_VALUE;

	init_thread();

	for (thread_id i = *_cookie; i < MAX_THREADS; i++)
	{
		if (thread_table[i].team == teamID)
		{
			*_cookie = i + 1;

			return _get_thread_info(thread_table[i].thread, info, size);
		}
	}

	return B_BAD_VALUE;
}


thread_id
find_thread(const char* name)
{
	return _find_thread(name, B_ANY_TEAM);
}


thread_id
_find_thread(const char* name, team_id team)
{
	init_thread();

	pthread_t pth = name ? 0 : pthread_self();
	pid_t my_pid = getpid();

	pthread_mutex_lock(&thread_sync->mutex);
	for (thread_id i = 0; i < MAX_THREADS; i++)
	{
		if (thread_table[i].thread != FREE_SLOT)
		{
			// If we wanted a specific team, and this isn't it, continue
			if (team != B_ANY_TEAM && thread_table[i].team != team)
				continue;

			if (name)
			{
				if (strcmp(thread_table[i].name, name) == 0) {
					pthread_mutex_unlock(&thread_sync->mutex);
					return i;
				}
			}
			else
			{
				// When finding current thread by pthread_t, also check team
				// to distinguish parent from child after fork()
				if (pthread_equal(thread_table[i].pth, pth)) {
					// If team was specified (not B_ANY_TEAM), it was already checked above
					// If team is B_ANY_TEAM, still prefer our own process's thread
					if (team == B_ANY_TEAM && thread_table[i].team != my_pid)
						continue;
					pthread_mutex_unlock(&thread_sync->mutex);
					return i;
				}
			}
		}
	}
	pthread_mutex_unlock(&thread_sync->mutex);

	return B_NAME_NOT_FOUND;
}


status_t
set_thread_priority(thread_id id, int32 priority)
{
	init_thread();

	if (id < 0)
		return B_BAD_THREAD_ID;

	pthread_mutex_lock(&thread_sync->mutex);
	for (thread_id i = 0; i < MAX_THREADS; i++)
	{
		if (thread_table[i].thread == id)
		{
			int32 old_priority = thread_table[i].priority;
			thread_table[i].priority = priority;
			pthread_mutex_unlock(&thread_sync->mutex);
			return old_priority;
		}
	}
	pthread_mutex_unlock(&thread_sync->mutex);

	return B_BAD_THREAD_ID;
}


// Internal helper: performs the actual sleep with thread state management
static status_t
_do_snooze(bigtime_t duration, bool canInterrupt)
{
	init_thread();

	pthread_t pth = pthread_self();

	for (thread_id i = 0; i < MAX_THREADS; i++)
	{
		if (pthread_equal(thread_table[i].pth, pth))
		{
			thread_table[i].state = B_THREAD_ASLEEP;

			int err = usleep((unsigned long)duration);

			thread_table[i].state = B_THREAD_RUNNING;

			// Handle interruption based on canInterrupt flag
			if (err < 0 && errno == EINTR)
			{
				if (canInterrupt)
					return B_INTERRUPTED;
				// If interruption not allowed, ignore and return success
			}

			return B_OK;
		}
	}
	
	return B_OK;
}


status_t
snooze(bigtime_t timeout)
{
	return _do_snooze(timeout, true);
}


status_t
snooze_etc(bigtime_t amount, int timeBase, uint32 flags)
{
	bool canInterrupt = (flags & B_CAN_INTERRUPT) != 0;

	// Handle absolute vs relative timeout
	if (timeBase & B_ABSOLUTE_TIMEOUT)
	{
		// Use snooze_until for absolute timeouts
		clockid_t clock_id;
		
		// Check if we should use real-time clock or system time
		if (timeBase & B_TIMEOUT_REAL_TIME_BASE)
			clock_id = CLOCK_REALTIME;
		else
			clock_id = CLOCK_MONOTONIC;
		
		// Get current time
		struct timespec now;
		if (clock_gettime(clock_id, &now) != 0)
			return B_ERROR;

		// Convert current time to microseconds
		bigtime_t now_usecs = (bigtime_t)now.tv_sec * 1000000LL + 
		                      (bigtime_t)now.tv_nsec / 1000LL;

		// Calculate sleep duration
		bigtime_t duration = amount - now_usecs;

		// If the target time is in the past, return timeout
		if (duration <= 0)
			return B_TIMED_OUT;

		// Perform the sleep
		return _do_snooze(duration, canInterrupt);
	}
	else
	{
		// Relative timeout (default)
		return _do_snooze(amount, canInterrupt);
	}
}


status_t
snooze_until(bigtime_t time, int timeBase)
{
	init_thread();

	pthread_t pth = pthread_self();

	for (thread_id i = 0; i < MAX_THREADS; i++)
	{
		if (pthread_equal(thread_table[i].pth, pth))
		{
			thread_table[i].state = B_THREAD_ASLEEP;

			// Get the current time based on the specified time base
			struct timespec now;
			// B_SYSTEM_TIMEBASE (0) should use CLOCK_MONOTONIC to match system_time()
			// Other values are passed directly as clockid_t
			clockid_t clock_id = (timeBase == B_SYSTEM_TIMEBASE) ? CLOCK_MONOTONIC : (clockid_t)timeBase;
			
			if (clock_gettime(clock_id, &now) != 0)
			{
				thread_table[i].state = B_THREAD_RUNNING;
				return B_ERROR;
			}

			// Convert current time to microseconds
			bigtime_t now_usecs = (bigtime_t)now.tv_sec * 1000000LL + 
			                      (bigtime_t)now.tv_nsec / 1000LL;

			// Calculate sleep duration
			bigtime_t duration = time - now_usecs;

			// If the target time is in the past or now, don't sleep
			if (duration <= 0)
			{
				thread_table[i].state = B_THREAD_RUNNING;
				return B_OK;
			}

			// Sleep for the calculated duration
			int err = usleep((unsigned long)duration);

			thread_table[i].state = B_THREAD_RUNNING;

			if (err < 0 && errno == EINTR)
				return B_INTERRUPTED;

			break;
		}
	}
	
	return B_OK;
}


status_t
wait_for_thread(thread_id id, status_t *_returnCode)
{
	init_thread();

	if (id < 0)
		return B_BAD_THREAD_ID;

	pthread_mutex_lock(&thread_sync->mutex);
	for (thread_id i = 0; i < MAX_THREADS; i++)
	{
		if (thread_table[i].thread == id)
		{
			void *returnValue = NULL;
			pthread_t pth = thread_table[i].pth;
			pthread_mutex_unlock(&thread_sync->mutex);

			if (pthread_join(pth, &returnValue) == 0) {
				if (_returnCode)
					*_returnCode = static_cast<status_t>(reinterpret_cast<uintptr_t>(returnValue));
				
				// Clean up the thread table entry after successful join
				remove_thread_table_entry(id);
				
				return B_OK;
			}
			return B_BAD_THREAD_ID;
		}
	}
	pthread_mutex_unlock(&thread_sync->mutex);

	return B_BAD_THREAD_ID;
}


status_t
suspend_thread(thread_id id)
{
	if (id < 0)
		return B_BAD_THREAD_ID;

	return send_signal(id, SIGSTOP);
}


status_t
resume_thread(thread_id id)
{
	init_thread();

	if (id < 0)
		return B_BAD_THREAD_ID;

	pthread_mutex_lock(&thread_sync->mutex);
	for (thread_id i = 0; i < MAX_THREADS; i++)
	{
		if (thread_table[i].thread == id)
		{
			switch (thread_table[i].state)
			{
				case B_THREAD_SPAWNED:
				{
					pthread_t tid;

					if (pthread_create(&tid, NULL, thread_wrapper,
										(void*)(uintptr_t)id) == 0)
					{
						thread_table[i].pth = tid;
						thread_table[i].state = B_THREAD_RUNNING;
						pthread_mutex_unlock(&thread_sync->mutex);
						return B_OK;
					}

					pthread_mutex_unlock(&thread_sync->mutex);
					return B_ERROR;
				}

				case B_THREAD_SUSPENDED:
					pthread_kill(thread_table[i].pth, SIGCONT);
					thread_table[i].state = B_THREAD_RUNNING;
					pthread_mutex_unlock(&thread_sync->mutex);
					return B_OK;

				default:
					pthread_mutex_unlock(&thread_sync->mutex);
					return B_BAD_THREAD_STATE;
			}
		}
	}
	pthread_mutex_unlock(&thread_sync->mutex);

	return B_BAD_THREAD_ID;
}


status_t kill_team(team_id team)
{
	status_t ret = B_OK;
	int err;
	 
	err = kill((pid_t)team, SIGKILL);
	if (err < 0 && errno == ESRCH)
		ret = B_BAD_TEAM_ID;

	return ret;
}


int32 _thread_count_for_team(team_id id)
{
	init_thread();
	int32 threadCount = 0;

	for (thread_id i = 0; i < MAX_THREADS; i++)
	{
		if (thread_table[i].thread == id)
			threadCount++;
	}

	return threadCount;
}


team_id	_get_next_team(team_id id)
{
	init_thread();
	team_id nextTeam = INT_MAX;

	for (thread_id i = 0; i < MAX_THREADS; i++)
	{
		if (thread_table[i].team > id && thread_table[i].team < nextTeam)
			nextTeam = thread_table[i].team;
	}

	return (nextTeam != INT_MAX) ? nextTeam : B_NO_MORE_TEAMS;
}


thread_id _main_thread_for_team(team_id id)
{
	init_thread();

	for (thread_id i = 0; i < MAX_THREADS; i++)
	{
		if (thread_table[i].team == id && strcmp(thread_table[i].name, "main") == 0)
		{
			return thread_table[i].thread;
		}
	}

	return B_ERROR;
}


int send_signal(thread_id id, unsigned int signal)
{
	init_thread();

	for (thread_id i = 0; i < MAX_THREADS; i++)
	{
		if (thread_table[i].thread == id)
		{
			pthread_kill(thread_table[i].pth, signal);
			if (signal == SIGSTOP)
				thread_table[i].state = B_THREAD_SUSPENDED;

			return B_OK;
		}
	}

	return B_BAD_THREAD_ID;
}


// Insert a reference to our team's main thread into our thread table,
// which otherwise would not be represented there.  This allow send_data
// and receive_data to work in the main thread.
status_t
_register_main_thread()
{
	if (init_thread() != B_OK)
		return B_ERROR;

	const char* name = "main";

	for (thread_id i = 0; i < MAX_THREADS; i++)
	{
		if (thread_table[i].thread == FREE_SLOT)
		{
			thread_table[i].pth = pthread_self();
			thread_table[i].thread = i;
			thread_table[i].team = getpid();
			thread_table[i].priority = B_NORMAL_PRIORITY;
			thread_table[i].state = B_THREAD_RUNNING;
			strncpy(thread_table[i].name, name, B_OS_NAME_LENGTH);
			thread_table[i].name[B_OS_NAME_LENGTH - 1] = '\0';
			thread_table[i].func = NULL;
			thread_table[i].data = NULL;
			thread_table[i].code = 0;
			thread_table[i].sender = 0;
			thread_table[i].buffer[0] = '\0';
			thread_table[i].buffer_allocation = 0;

			return B_OK;
		}
	}

	return B_ERROR;
}
