/*
 * Copyright 2004-2025, Bill Hayden
 * Copyright 2008-2011, Ingo Weinhold, ingo_weinhold@gmx.de.
 * Copyright 2002-2010, Axel Dörfler, axeld@pinc-software.de.
 * Distributed under the terms of the MIT License.
 *
 * Copyright 2001, Travis Geiselbrecht. All rights reserved.
 * Distributed under the terms of the NewOS License.
 */


/*
 
Implementation using POSIX unnamed semaphores for thread-only use.
Each semaphore is allocated from a simple table with direct indexing.
No kernel IPC objects are used, making cleanup automatic and overhead minimal.

*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include <OS.h>

#include <errno.h>
#include <sys/time.h>

// Platform-specific system headers
#ifdef _WIN32
	#include <process.h>  // For getpid() on Windows
#else
	#include <unistd.h>   // For getpid() on POSIX
#endif

#include <semaphore.h>

// macOS doesn't have sem_timedwait, provide a fallback
#ifdef __APPLE__
static int sem_timedwait(sem_t *sem, const struct timespec *abs_timeout) {
	while (1) {
		if (sem_trywait(sem) == 0)
			return 0;
		if (errno != EAGAIN)
			return -1;
		
		struct timeval now;
		gettimeofday(&now, NULL);
		
		if (now.tv_sec > abs_timeout->tv_sec ||
			(now.tv_sec == abs_timeout->tv_sec && now.tv_usec * 1000 >= abs_timeout->tv_nsec)) {
			errno = ETIMEDOUT;
			return -1;
		}
		
		usleep(1000); // Sleep for 1ms
	}
}
#endif

//#define TRACE_SEM
#ifdef TRACE_SEM
#	define TRACE(x) printf x
#else
#	define TRACE(x) ;
#endif

#define MAX_SEMS 4096

// Internal semaphore structure
struct cosmoe_sem {
	sem_t posix_sem;
	char name[B_OS_NAME_LENGTH];
	team_id owner;
	bool in_use;
	int32 waiter_count;  // Number of threads currently waiting
};

static struct cosmoe_sem sem_table[MAX_SEMS];
static pthread_mutex_t sem_table_lock = PTHREAD_MUTEX_INITIALIZER;

static void construct_sem_timeout(struct timespec* tmout,
								  uint32 flags,
								  bigtime_t raw_timeout);


sem_id create_sem_etc(int32 count,
					  const char *name,
					  team_id owner)
{
	int err;
	
	TRACE(("create_sem_etc: enter\n"));
	
	if (count < 0)
		return B_BAD_VALUE;
	
	pthread_mutex_lock(&sem_table_lock);
	
	// Find a free slot
	sem_id id = -1;
	for (int i = 0; i < MAX_SEMS; i++) {
		if (!sem_table[i].in_use) {
			id = i;
			break;
		}
	}
	
	if (id == -1) {
		pthread_mutex_unlock(&sem_table_lock);
		TRACE(("create_sem_etc(): no free semaphore slots\n"));
		return B_NO_MORE_SEMS;
	}
	
	// Initialize the POSIX semaphore (0 = not shared between processes)
	err = sem_init(&sem_table[id].posix_sem, 0, count);
	if (err == -1) {
		pthread_mutex_unlock(&sem_table_lock);
		TRACE(("create_sem_etc(): sem_init failed with errno %d\n", errno));
		return B_NO_MORE_SEMS;
	}
	
	// Set up metadata
	sem_table[id].in_use = true;
	sem_table[id].owner = owner;
	sem_table[id].waiter_count = 0;
	if (name) {
		strncpy(sem_table[id].name, name, B_OS_NAME_LENGTH - 1);
		sem_table[id].name[B_OS_NAME_LENGTH - 1] = '\0';
	} else {
		strcpy(sem_table[id].name, "unnamed sem");
	}
	
	pthread_mutex_unlock(&sem_table_lock);
	
	TRACE(("create_sem_etc(): created sem %d\n", id));
	return id;
}


sem_id create_sem(int32 count,
				  const char *name)
{
	return create_sem_etc(count, name, getpid());
}


status_t delete_sem(sem_id id)
{
	return delete_sem_etc(id, 0, false);
}


status_t delete_sem_etc(sem_id id,
						status_t return_code,
						bool interrupted)
{
	int err;
	int32 waiters;

	TRACE(("delete_sem_etc(%ld): enter\n", id));
	
	if (id < 0 || id >= MAX_SEMS)
		return B_BAD_SEM_ID;
	
	pthread_mutex_lock(&sem_table_lock);
	
	if (!sem_table[id].in_use) {
		pthread_mutex_unlock(&sem_table_lock);
		return B_BAD_SEM_ID;
	}
	
	// Mark as not in use first, so any new acquire attempts will fail
	sem_table[id].in_use = false;
	
	// Get the number of waiting threads so we know how many to wake
	waiters = sem_table[id].waiter_count;
	
	pthread_mutex_unlock(&sem_table_lock);
	
	// Wake up all waiting threads by posting exactly the number needed
	// The waiting threads will see in_use=false and return B_BAD_SEM_ID
	if (waiters > 0) {
		TRACE(("delete_sem_etc(): waking %d waiting threads\n", waiters));
		for (int32 i = 0; i < waiters; i++) {
			sem_post(&sem_table[id].posix_sem);
		}
		// Give threads a moment to wake up and exit their wait
		usleep(10000);  // 10ms
	}
	
	// Destroy the POSIX semaphore
	// After waking waiters and the delay, they should have exited their wait
	err = sem_destroy(&sem_table[id].posix_sem);
	if (err == -1 && errno == EBUSY) {
		// If still busy, wait a bit longer and try again
		TRACE(("delete_sem_etc(): still busy after waking waiters, retrying\n"));
		usleep(50000);  // 50ms more
		err = sem_destroy(&sem_table[id].posix_sem);
	}
	
	if (err == -1) {
		TRACE(("delete_sem_etc(): sem_destroy failed with errno %d\n", errno));
		// Semaphore is already marked as not in use, so just return success
		// The POSIX semaphore will leak, but the ID is freed for reuse
		return B_OK;
	}

	return B_OK;
}


status_t acquire_sem(sem_id id)
{
	return acquire_sem_etc(id, 1, 0, 0);
}


status_t acquire_sem_etc(sem_id id,
						 int32 count,
						 uint32 flags,
						 bigtime_t timeout)
{
	struct timespec tmout;
	int err;

	TRACE(("acquire_sem_etc(%ld): enter\n", id));

	if (id < 0 || id >= MAX_SEMS)
		return B_BAD_SEM_ID;
	
	pthread_mutex_lock(&sem_table_lock);
	
	if (!sem_table[id].in_use) {
		pthread_mutex_unlock(&sem_table_lock);
		return B_BAD_SEM_ID;
	}
	
	pthread_mutex_unlock(&sem_table_lock);
	
	if (count <= 0)
		return B_BAD_VALUE;
	
	// Check if timeout is requested and not infinite
	bool has_timeout = (flags & (B_RELATIVE_TIMEOUT | B_ABSOLUTE_TIMEOUT)) 
	                   && (timeout != B_INFINITE_TIMEOUT);
	
	// Acquire 'count' times
	for (int32 i = 0; i < count; i++) {
		// Increment waiter count before potentially blocking
		pthread_mutex_lock(&sem_table_lock);
		if (!sem_table[id].in_use) {
			pthread_mutex_unlock(&sem_table_lock);
			// Release any we already acquired
			for (int32 j = 0; j < i; j++) {
				sem_post(&sem_table[id].posix_sem);
			}
			return B_BAD_SEM_ID;
		}
		sem_table[id].waiter_count++;
		pthread_mutex_unlock(&sem_table_lock);
		
		if (has_timeout && (timeout <= 0) && (flags & B_RELATIVE_TIMEOUT)) {
			// Try without blocking (relative timeout of 0)
			err = sem_trywait(&sem_table[id].posix_sem);
		} else if (has_timeout) {
			// Wait with timeout
			construct_sem_timeout(&tmout, flags, timeout);
			err = sem_timedwait(&sem_table[id].posix_sem, &tmout);
		} else {
			// Wait indefinitely
			err = sem_wait(&sem_table[id].posix_sem);
		}
		
		// Decrement waiter count after acquiring (or failing to acquire)
		pthread_mutex_lock(&sem_table_lock);
		sem_table[id].waiter_count--;
		bool still_valid = sem_table[id].in_use;
		pthread_mutex_unlock(&sem_table_lock);
		
		if (err == -1) {
			// Need to release any we already acquired
			for (int32 j = 0; j < i; j++) {
				sem_post(&sem_table[id].posix_sem);
			}
			
			if (!still_valid) {
				return B_BAD_SEM_ID;
			}
			
			if (errno == ETIMEDOUT || errno == EAGAIN)
				return ((flags & B_TIMEOUT) && (timeout <= 0)) ? B_WOULD_BLOCK : B_TIMED_OUT;
			else if (errno == EINTR)
				return B_INTERRUPTED;
			else {
				TRACE(("acquire_sem_etc(): error %d, errno is %d\n", err, errno));
				return B_ERROR;
			}
		}
		
		// Even if wait succeeded, check if semaphore was deleted while we were waiting
		if (!still_valid) {
			// Release any we already acquired
			for (int32 j = 0; j <= i; j++) {
				sem_post(&sem_table[id].posix_sem);
			}
			return B_BAD_SEM_ID;
		}
	}
	
	return B_OK;
}


status_t
release_sem(sem_id id)
{
	return release_sem_etc(id, 1, 0);
}


status_t
release_sem_etc(sem_id id, int32 count, uint32 flags)
{
	TRACE(("release_sem_etc(%ld): enter\n", id));

	if (id < 0 || id >= MAX_SEMS)
		return B_BAD_SEM_ID;
	
	pthread_mutex_lock(&sem_table_lock);
	
	if (!sem_table[id].in_use) {
		pthread_mutex_unlock(&sem_table_lock);
		return B_BAD_SEM_ID;
	}
	
	pthread_mutex_unlock(&sem_table_lock);

	if (count <= 0)
		return B_BAD_VALUE;

	// Release 'count' times
	for (int32 i = 0; i < count; i++) {
		if (sem_post(&sem_table[id].posix_sem) == -1) {
			TRACE(("release_sem_etc(): sem_post failed with errno %d\n", errno));
			return B_BAD_SEM_ID;
		}
	}

	return B_OK;
}


status_t
get_sem_count(sem_id id, int32 *_count)
{
	int semcount;
	
	TRACE(("get_sem_count(%ld): enter\n", id));

	if (id < 0 || id >= MAX_SEMS)
		return B_BAD_SEM_ID;

	if (_count == NULL)
		return B_BAD_VALUE;
	
	pthread_mutex_lock(&sem_table_lock);
	
	if (!sem_table[id].in_use) {
		pthread_mutex_unlock(&sem_table_lock);
		return B_BAD_SEM_ID;
	}
		
	// Get semaphore value
	if (sem_getvalue(&sem_table[id].posix_sem, &semcount) == -1) {
		pthread_mutex_unlock(&sem_table_lock);
		return B_BAD_SEM_ID;
	}

	// If there are waiters and the semaphore count is 0, return negative count
	if (sem_table[id].waiter_count > 0 && semcount == 0) {
		*_count = -sem_table[id].waiter_count;
	} else {
		*_count = semcount;
	}
	
	pthread_mutex_unlock(&sem_table_lock);

	return B_OK;
}


/*!	Called by the get_sem_info() macro. */
status_t
_get_sem_info(sem_id id, struct sem_info *info, size_t size)
{
	int semcount;
	
	TRACE(("_get_sem_info(%ld): enter\n", id));

	if (id < 0 || id >= MAX_SEMS)
		return B_BAD_SEM_ID;
	
	pthread_mutex_lock(&sem_table_lock);
	
	if (!sem_table[id].in_use) {
		pthread_mutex_unlock(&sem_table_lock);
		return B_BAD_SEM_ID;
	}
	
	if (info == NULL || size != sizeof(sem_info)) {
		pthread_mutex_unlock(&sem_table_lock);
		return B_BAD_VALUE;
	}

	info->sem = id;
	info->team = sem_table[id].owner;
	strncpy(info->name, sem_table[id].name, B_OS_NAME_LENGTH - 1);
	info->name[B_OS_NAME_LENGTH - 1] = '\0';
	
	if (sem_getvalue(&sem_table[id].posix_sem, &semcount) == -1) {
		pthread_mutex_unlock(&sem_table_lock);
		return B_BAD_SEM_ID;
	}
	
	// If there are waiters and the semaphore count is 0, return negative count
	if (sem_table[id].waiter_count > 0 && semcount == 0) {
		semcount = -sem_table[id].waiter_count;
	}
	info->count = semcount;
	info->latest_holder	= 0;
	
	pthread_mutex_unlock(&sem_table_lock);
	
	return B_OK;
}


/*!	Called by the get_next_sem_info() macro. */
status_t
_get_next_sem_info(team_id teamID, int32 *_cookie, struct sem_info *info,
	size_t size)
{
	int32 slot = *_cookie;
	
	TRACE(("_get_next_sem_info(): enter, cookie=%d\n", slot));
	
	// Validate team_id (must be non-negative, 0 means current team)
	if (teamID < 0)
		return B_BAD_TEAM_ID;
	
	if (slot < 0)
		slot = 0;
	
	pthread_mutex_lock(&sem_table_lock);
	
	// Find the next valid semaphore
	while (slot < MAX_SEMS) {
		if (sem_table[slot].in_use) {
			// Found one, fill in the info directly (can't call _get_sem_info - would deadlock)
			if (info != NULL && size == sizeof(sem_info)) {
				int semcount;
				info->sem = slot;
				info->team = sem_table[slot].owner;
				strncpy(info->name, sem_table[slot].name, B_OS_NAME_LENGTH - 1);
				info->name[B_OS_NAME_LENGTH - 1] = '\0';
				
				if (sem_getvalue(&sem_table[slot].posix_sem, &semcount) == 0) {
					// If there are waiters and the semaphore count is 0, return negative count
					if (sem_table[slot].waiter_count > 0 && semcount == 0) {
						info->count = -sem_table[slot].waiter_count;
					} else {
						info->count = semcount;
					}
				} else {
					info->count = 0;
				}
				info->latest_holder = 0;
				
				*_cookie = slot + 1;  // Move to next slot for next call
				pthread_mutex_unlock(&sem_table_lock);
				return B_OK;
			}
		}
		slot++;
	}
	
	pthread_mutex_unlock(&sem_table_lock);
	
	return B_BAD_VALUE;  // No more semaphores
}


/* kinda useless in Cosmoe on Wayland, but kept for compatibility */
status_t
set_sem_owner(sem_id id, team_id newTeamID)
{
	TRACE(("set_sem_owner(%ld): enter\n", id));

	if (id < 0 || id >= MAX_SEMS)
		return B_BAD_SEM_ID;
	
	// Validate team_id (must be positive, or B_SYSTEM_TEAM which is 1)
	if (newTeamID < 0)
		return B_BAD_TEAM_ID;
	
	pthread_mutex_lock(&sem_table_lock);
	
	if (!sem_table[id].in_use) {
		pthread_mutex_unlock(&sem_table_lock);
		return B_BAD_SEM_ID;
	}
	
	sem_table[id].owner = newTeamID;
	
	pthread_mutex_unlock(&sem_table_lock);
	
	return B_OK;
}


void construct_sem_timeout(struct timespec* ts, uint32 flags, bigtime_t timeout)
{
	if (timeout == B_INFINITE_TIMEOUT) {
		// Set to the maximum possible time without overflow
		ts->tv_sec = 0x7FFFFFFF;
		ts->tv_nsec = 999999999L;
		return;
	}
	
	if (flags & B_ABSOLUTE_TIMEOUT)
	{
		// Convert absolute BeOS time (microseconds since boot) to timespec
		// BeOS absolute time is system_time(), which is microseconds since boot
		ts->tv_sec = timeout / 1000000LL;
		ts->tv_nsec = (timeout % 1000000LL) * 1000L;
	}
	else /* B_RELATIVE_TIMEOUT */
	{
		// Convert relative timeout to absolute CLOCK_REALTIME time
		// sem_timedwait requires absolute time based on CLOCK_REALTIME
		struct timespec now;
		clock_gettime(CLOCK_REALTIME, &now);
		
		// Add the relative timeout (in microseconds) to current time
		int64 total_nsec = (now.tv_sec * 1000000000LL) + now.tv_nsec + (timeout * 1000LL);
		
		ts->tv_sec = total_nsec / 1000000000LL;
		ts->tv_nsec = total_nsec % 1000000000LL;
	}

	/* If we ended up with an overflow in tv_nsec, spill it into tv_sec */
	while (ts->tv_nsec >= 1000000000L)
	{
		ts->tv_sec++;
		ts->tv_nsec -= 1000000000L;
	}
}


static int dump_sem_list(void)
{
	int count = 0;

	pthread_mutex_lock(&sem_table_lock);
	
	for (int id = 0; id < MAX_SEMS; id++) 
	{
		if (sem_table[id].in_use)
		{
			int semval;
			if (sem_getvalue(&sem_table[id].posix_sem, &semval) == 0)
				printf("id: %d\t\tcount: %d\t\tname: %s\n", id, semval, sem_table[id].name);
			count++;
		}
	}
	
	pthread_mutex_unlock(&sem_table_lock);
	
	return count;
}


static void dump_sem(int id)
{
	if (id < 0 || id >= MAX_SEMS)
	{
		printf("A semaphore with that ID has never existed.\n");
		return;
	}
	
	pthread_mutex_lock(&sem_table_lock);
	
	if (sem_table[id].in_use)
	{
		int semval;
		if (sem_getvalue(&sem_table[id].posix_sem, &semval) == 0)
			printf("id: %d\t\tcount: %d\t\tname: %s\n", id, semval, sem_table[id].name);
		else
			printf("Error reading semaphore value.\n");
	}
	else
	{
		printf("There is no active semaphore with that ID.\n");
	}
	
	pthread_mutex_unlock(&sem_table_lock);
}

int dump_sem_info(int argc, char **argv)
{
	if (argc < 2)
		dump_sem_list();
	else
		dump_sem(atoi(argv[1]));

	return 0;
}
