/*
 * Copyright 2025, Cosmoe Project
 * Distributed under the terms of the MIT License.
 *
 * Windows-specific thread implementation
 * Maps BeOS thread API to Windows threading primitives
 */

#include <windows.h>
#include <process.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <OS.h>

//#define TRACE_THREAD
#ifdef TRACE_THREAD
#	define TRACE(x) printf x
#else
#	define TRACE(x) ;
#endif

// Maximum number of threads we can track
#define MAX_THREADS 128

// Thread table entry
typedef struct {
	thread_id id;           // BeOS thread ID (matches index)
	HANDLE handle;          // Windows thread handle
	DWORD win_thread_id;    // Windows thread ID
	thread_func entry;      // Entry point function
	void* data;             // User data
	char name[B_OS_NAME_LENGTH];
	int32 priority;
	team_id team;
	status_t exit_status;
	bool in_use;
	bool exited;
	CRITICAL_SECTION lock;
} thread_entry_t;

// Global thread table
static thread_entry_t thread_table[MAX_THREADS];
static CRITICAL_SECTION table_lock;
static BOOL table_initialized = FALSE;

// Thread-local storage for current thread ID
static DWORD tls_thread_id = TLS_OUT_OF_INDEXES;


// Initialize the threading system
static void
init_threading(void)
{
	if (table_initialized)
		return;

	InitializeCriticalSection(&table_lock);
	
	// Initialize all slots
	for (int i = 0; i < MAX_THREADS; i++) {
		thread_table[i].id = i;
		thread_table[i].handle = NULL;
		thread_table[i].win_thread_id = 0;
		thread_table[i].entry = NULL;
		thread_table[i].data = NULL;
		thread_table[i].name[0] = '\0';
		thread_table[i].priority = B_NORMAL_PRIORITY;
		thread_table[i].team = _getpid();
		thread_table[i].exit_status = 0;
		thread_table[i].in_use = false;
		thread_table[i].exited = false;
		InitializeCriticalSection(&thread_table[i].lock);
	}

	// Allocate TLS for thread ID
	tls_thread_id = TlsAlloc();
	if (tls_thread_id == TLS_OUT_OF_INDEXES) {
		fprintf(stderr, "FATAL: Failed to allocate TLS for thread ID\n");
	}

	table_initialized = TRUE;
	TRACE(("Windows threading initialized\n"));
}


// Find a free slot in the thread table
static thread_id
allocate_thread_slot(void)
{
	EnterCriticalSection(&table_lock);
	
	for (int i = 0; i < MAX_THREADS; i++) {
		if (!thread_table[i].in_use) {
			thread_table[i].in_use = true;
			thread_table[i].exited = false;
			thread_table[i].exit_status = 0;
			LeaveCriticalSection(&table_lock);
			return i;
		}
	}
	
	LeaveCriticalSection(&table_lock);
	return B_NO_MORE_THREADS;
}


// Free a thread slot
static void
free_thread_slot(thread_id id)
{
	if (id < 0 || id >= MAX_THREADS)
		return;

	EnterCriticalSection(&table_lock);
	
	thread_entry_t* entry = &thread_table[id];
	
	if (entry->handle) {
		CloseHandle(entry->handle);
		entry->handle = NULL;
	}
	
	entry->in_use = false;
	entry->win_thread_id = 0;
	entry->entry = NULL;
	entry->data = NULL;
	entry->name[0] = '\0';
	
	LeaveCriticalSection(&table_lock);
}


// Convert BeOS priority to Windows priority
static int
beos_to_windows_priority(int32 beos_priority)
{
	if (beos_priority >= B_REAL_TIME_PRIORITY)
		return THREAD_PRIORITY_TIME_CRITICAL;
	else if (beos_priority >= B_URGENT_PRIORITY)
		return THREAD_PRIORITY_HIGHEST;
	else if (beos_priority >= B_DISPLAY_PRIORITY)
		return THREAD_PRIORITY_ABOVE_NORMAL;
	else if (beos_priority >= B_NORMAL_PRIORITY)
		return THREAD_PRIORITY_NORMAL;
	else if (beos_priority >= B_LOW_PRIORITY)
		return THREAD_PRIORITY_BELOW_NORMAL;
	else
		return THREAD_PRIORITY_LOWEST;
}


// Thread wrapper function
static DWORD WINAPI
thread_wrapper(LPVOID arg)
{
	thread_id id = (thread_id)(uintptr_t)arg;
	
	if (id < 0 || id >= MAX_THREADS) {
		fprintf(stderr, "thread_wrapper: Invalid thread ID %d\n", (int)id);
		return B_ERROR;
	}

	thread_entry_t* entry = &thread_table[id];
	
	// Store thread ID in TLS
	TlsSetValue(tls_thread_id, (LPVOID)(uintptr_t)id);
	
	TRACE(("thread_wrapper: Starting thread %d '%s'\n", (int)id, entry->name));
	
	// Call the actual thread function
	thread_func func = entry->entry;
	void* data = entry->data;
	
	if (func) {
		entry->exit_status = func(data);
	} else {
		entry->exit_status = B_ERROR;
	}
	
	// Mark as exited
	EnterCriticalSection(&entry->lock);
	entry->exited = true;
	LeaveCriticalSection(&entry->lock);
	
	TRACE(("thread_wrapper: Thread %d exiting with status %d\n", 
	       (int)id, (int)entry->exit_status));
	
	return (DWORD)entry->exit_status;
}


// Spawn a new thread
thread_id
spawn_thread(thread_func entry_func, const char* name, int32 priority, void* data)
{
	if (!table_initialized)
		init_threading();

	if (!entry_func)
		return B_BAD_VALUE;

	thread_id id = allocate_thread_slot();
	if (id < 0) {
		fprintf(stderr, "spawn_thread: No free thread slots\n");
		return B_NO_MORE_THREADS;
	}

	thread_entry_t* entry = &thread_table[id];
	
	// Fill in thread info
	entry->entry = entry_func;
	entry->data = data;
	entry->priority = priority;
	entry->team = _getpid();
	
	if (name) {
		strncpy(entry->name, name, B_OS_NAME_LENGTH - 1);
		entry->name[B_OS_NAME_LENGTH - 1] = '\0';
	} else {
		snprintf(entry->name, B_OS_NAME_LENGTH, "thread_%d", (int)id);
	}

	// Create the Windows thread in suspended state
	entry->handle = CreateThread(
		NULL,                                    // Default security
		0,                                       // Default stack size
		thread_wrapper,                          // Thread function
		(LPVOID)(uintptr_t)id,                  // Thread parameter
		CREATE_SUSPENDED,                        // Creation flags
		&entry->win_thread_id                   // Thread ID
	);

	if (!entry->handle) {
		fprintf(stderr, "spawn_thread: CreateThread failed for '%s': error %lu\n",
		        name ? name : "(unnamed)", GetLastError());
		free_thread_slot(id);
		return B_ERROR;
	}

	// Set thread priority
	int win_priority = beos_to_windows_priority(priority);
	SetThreadPriority(entry->handle, win_priority);

	TRACE(("spawn_thread: Created thread %d '%s' (Windows ID %lu)\n",
	       (int)id, entry->name, entry->win_thread_id));

	return id;
}


// Resume a suspended thread
status_t
resume_thread(thread_id thread)
{
	if (!table_initialized)
		init_threading();

	if (thread < 0 || thread >= MAX_THREADS)
		return B_BAD_THREAD_ID;

	thread_entry_t* entry = &thread_table[thread];
	
	if (!entry->in_use || !entry->handle)
		return B_BAD_THREAD_ID;

	DWORD result = ResumeThread(entry->handle);
	if (result == (DWORD)-1) {
		fprintf(stderr, "resume_thread: ResumeThread failed: error %lu\n", GetLastError());
		return B_ERROR;
	}

	TRACE(("resume_thread: Resumed thread %d\n", (int)thread));
	return B_OK;
}


// Suspend a thread
status_t
suspend_thread(thread_id thread)
{
	if (!table_initialized)
		init_threading();

	if (thread < 0 || thread >= MAX_THREADS)
		return B_BAD_THREAD_ID;

	thread_entry_t* entry = &thread_table[thread];
	
	if (!entry->in_use || !entry->handle)
		return B_BAD_THREAD_ID;

	DWORD result = SuspendThread(entry->handle);
	if (result == (DWORD)-1) {
		fprintf(stderr, "suspend_thread: SuspendThread failed: error %lu\n", GetLastError());
		return B_ERROR;
	}

	TRACE(("suspend_thread: Suspended thread %d\n", (int)thread));
	return B_OK;
}


// Kill a thread
status_t
kill_thread(thread_id thread)
{
	if (!table_initialized)
		init_threading();

	if (thread < 0 || thread >= MAX_THREADS)
		return B_BAD_THREAD_ID;

	thread_entry_t* entry = &thread_table[thread];
	
	if (!entry->in_use || !entry->handle)
		return B_BAD_THREAD_ID;

	// Warning: TerminateThread is dangerous and should be avoided
	// It doesn't allow proper cleanup
	BOOL result = TerminateThread(entry->handle, B_INTERRUPTED);
	if (!result) {
		fprintf(stderr, "kill_thread: TerminateThread failed: error %lu\n", GetLastError());
		return B_ERROR;
	}

	// Mark as exited
	EnterCriticalSection(&entry->lock);
	entry->exited = true;
	entry->exit_status = B_INTERRUPTED;
	LeaveCriticalSection(&entry->lock);

	TRACE(("kill_thread: Killed thread %d\n", (int)thread));
	return B_OK;
}


// Wait for a thread to exit
status_t
wait_for_thread(thread_id thread, status_t* returnValue)
{
	if (!table_initialized)
		init_threading();

	if (thread < 0 || thread >= MAX_THREADS)
		return B_BAD_THREAD_ID;

	thread_entry_t* entry = &thread_table[thread];
	
	if (!entry->in_use || !entry->handle)
		return B_BAD_THREAD_ID;

	// Wait for the thread to finish
	DWORD wait_result = WaitForSingleObject(entry->handle, INFINITE);
	if (wait_result != WAIT_OBJECT_0) {
		fprintf(stderr, "wait_for_thread: WaitForSingleObject failed: error %lu\n", 
		        GetLastError());
		return B_ERROR;
	}

	// Get exit code
	DWORD exit_code;
	if (GetExitCodeThread(entry->handle, &exit_code)) {
		entry->exit_status = (status_t)exit_code;
	}

	if (returnValue)
		*returnValue = entry->exit_status;

	TRACE(("wait_for_thread: Thread %d exited with status %d\n",
	       (int)thread, (int)entry->exit_status));

	// Clean up
	free_thread_slot(thread);

	return B_OK;
}


// Find the current thread
thread_id
find_thread(const char* name)
{
	if (!table_initialized)
		init_threading();

	// If name is NULL, return current thread
	if (!name) {
		// Get from TLS
		thread_id id = (thread_id)(uintptr_t)TlsGetValue(tls_thread_id);
		if (id >= 0 && id < MAX_THREADS)
			return id;
		
		// Fallback: search by Windows thread ID
		DWORD current_tid = GetCurrentThreadId();
		
		EnterCriticalSection(&table_lock);
		for (int i = 0; i < MAX_THREADS; i++) {
			if (thread_table[i].in_use && 
			    thread_table[i].win_thread_id == current_tid) {
				LeaveCriticalSection(&table_lock);
				return i;
			}
		}
		LeaveCriticalSection(&table_lock);
		
		return B_NAME_NOT_FOUND;
	}

	// Search by name
	EnterCriticalSection(&table_lock);
	for (int i = 0; i < MAX_THREADS; i++) {
		if (thread_table[i].in_use && 
		    strcmp(thread_table[i].name, name) == 0) {
			LeaveCriticalSection(&table_lock);
			return i;
		}
	}
	LeaveCriticalSection(&table_lock);

	return B_NAME_NOT_FOUND;
}


// Rename a thread
status_t
rename_thread(thread_id thread, const char* newName)
{
	if (!table_initialized)
		init_threading();

	if (!newName)
		return B_BAD_VALUE;

	if (thread < 0 || thread >= MAX_THREADS)
		return B_BAD_THREAD_ID;

	thread_entry_t* entry = &thread_table[thread];
	
	if (!entry->in_use)
		return B_BAD_THREAD_ID;

	EnterCriticalSection(&entry->lock);
	strncpy(entry->name, newName, B_OS_NAME_LENGTH - 1);
	entry->name[B_OS_NAME_LENGTH - 1] = '\0';
	LeaveCriticalSection(&entry->lock);

	TRACE(("rename_thread: Renamed thread %d to '%s'\n", (int)thread, newName));
	return B_OK;
}


// Set thread priority
status_t
set_thread_priority(thread_id thread, int32 newPriority)
{
	if (!table_initialized)
		init_threading();

	if (thread < 0 || thread >= MAX_THREADS)
		return B_BAD_THREAD_ID;

	thread_entry_t* entry = &thread_table[thread];
	
	if (!entry->in_use || !entry->handle)
		return B_BAD_THREAD_ID;

	int win_priority = beos_to_windows_priority(newPriority);
	if (!SetThreadPriority(entry->handle, win_priority)) {
		fprintf(stderr, "set_thread_priority: SetThreadPriority failed: error %lu\n",
		        GetLastError());
		return B_ERROR;
	}

	entry->priority = newPriority;
	TRACE(("set_thread_priority: Set thread %d priority to %d\n", 
	       (int)thread, (int)newPriority));
	
	return B_OK;
}


// Exit the current thread
void
exit_thread(status_t status)
{
	if (!table_initialized)
		init_threading();

	thread_id id = find_thread(NULL);
	if (id >= 0 && id < MAX_THREADS) {
		thread_entry_t* entry = &thread_table[id];
		EnterCriticalSection(&entry->lock);
		entry->exited = true;
		entry->exit_status = status;
		LeaveCriticalSection(&entry->lock);
	}

	TRACE(("exit_thread: Thread %d exiting with status %d\n", (int)id, (int)status));
	ExitThread((DWORD)status);
}


// Sleep for a specified number of microseconds
status_t
snooze(bigtime_t microseconds)
{
	if (microseconds <= 0)
		return B_OK;

	// Convert microseconds to milliseconds (Windows Sleep uses ms)
	// Round up to ensure we sleep at least as long as requested
	DWORD milliseconds = (DWORD)((microseconds + 999) / 1000);
	
	Sleep(milliseconds);
	return B_OK;
}


// Sleep until a specific time
status_t
snooze_until(bigtime_t time, int timeBase)
{
	// Get current time
	bigtime_t now = system_time();
	
	// Calculate how long to sleep
	bigtime_t duration = time - now;
	
	if (duration <= 0)
		return B_OK;

	return snooze(duration);
}


// Get thread info
status_t
_get_thread_info(thread_id thread, thread_info* info, size_t size)
{
	if (!table_initialized)
		init_threading();

	if (!info || size != sizeof(thread_info))
		return B_BAD_VALUE;

	if (thread < 0 || thread >= MAX_THREADS)
		return B_BAD_THREAD_ID;

	thread_entry_t* entry = &thread_table[thread];
	
	if (!entry->in_use)
		return B_BAD_THREAD_ID;

	EnterCriticalSection(&entry->lock);
	
	info->thread = thread;
	info->team = entry->team;
	strncpy(info->name, entry->name, B_OS_NAME_LENGTH);
	info->name[B_OS_NAME_LENGTH - 1] = '\0';
	info->priority = entry->priority;
	
	// Get thread state
	if (entry->exited) {
		info->state = B_THREAD_ASLEEP;  // No exact "dead" state
	} else if (entry->handle) {
		DWORD suspend_count = SuspendThread(entry->handle);
		if (suspend_count == 0) {
			info->state = B_THREAD_RUNNING;
		} else {
			info->state = B_THREAD_SUSPENDED;
			// Resume since we only suspended to check state
			ResumeThread(entry->handle);
		}
		ResumeThread(entry->handle);  // Undo our suspend
	} else {
		info->state = B_THREAD_SUSPENDED;
	}
	
	// Stack info not easily available on Windows
	info->stack_base = NULL;
	info->stack_end = NULL;
	
	LeaveCriticalSection(&entry->lock);
	return B_OK;
}


// Get next thread info (for iteration)
status_t
_get_next_thread_info(team_id team, int32* cookie, thread_info* info, size_t size)
{
	if (!table_initialized)
		init_threading();

	if (!cookie || !info || size != sizeof(thread_info))
		return B_BAD_VALUE;

	team_id current_team = (team == B_CURRENT_TEAM) ? _getpid() : team;

	EnterCriticalSection(&table_lock);
	
	// Start from cookie index
	for (int i = *cookie; i < MAX_THREADS; i++) {
		if (thread_table[i].in_use && thread_table[i].team == current_team) {
			*cookie = i + 1;  // Next iteration starts here
			LeaveCriticalSection(&table_lock);
			return _get_thread_info(i, info, size);
		}
	}
	
	LeaveCriticalSection(&table_lock);
	return B_ENTRY_NOT_FOUND;
}


// Get pthread thread ID (not applicable on Windows, but provide stub)
thread_id
get_pthread_thread_id(pthread_t thread)
{
	// Not implemented on Windows
	return B_ERROR;
}
