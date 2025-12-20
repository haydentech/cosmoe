/*
 * Copyright 2024-2025, Cosmoe Project
 * Distributed under the terms of the MIT License.
 *
 * macOS-specific semaphore implementation using Grand Central Dispatch
 */

#include <OS.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <dispatch/dispatch.h>
#include <pthread.h>

//#define TRACE_SEM
#ifdef TRACE_SEM
#	define TRACE(x) printf x
#else
#	define TRACE(x) ;
#endif

#define MAX_SEMS 4096

typedef struct {
	dispatch_semaphore_t dsem;
	int32 count;
	char name[B_OS_NAME_LENGTH];
	team_id owner;
	bool in_use;
} macos_sem_entry;

static macos_sem_entry sem_table[MAX_SEMS];
static pthread_mutex_t sem_table_lock = PTHREAD_MUTEX_INITIALIZER;
static bool sem_system_initialized = false;

static void init_sem_system(void) {
	if (sem_system_initialized)
		return;
	
	for (int i = 0; i < MAX_SEMS; i++) {
		sem_table[i].dsem = NULL;
		sem_table[i].count = 0;
		sem_table[i].name[0] = '\0';
		sem_table[i].owner = 0;
		sem_table[i].in_use = false;
	}
	
	sem_system_initialized = true;
	TRACE(("sem system initialized\n"));
}

sem_id create_sem_etc(int32 count, const char *name, team_id owner) {
	if (!sem_system_initialized)
		init_sem_system();
	
	if (count < 0)
		return B_BAD_VALUE;
	
	pthread_mutex_lock(&sem_table_lock);
	
	// Find free slot
	sem_id id = -1;
	for (int i = 0; i < MAX_SEMS; i++) {
		if (!sem_table[i].in_use) {
			id = i;
			break;
		}
	}
	
	if (id == -1) {
		pthread_mutex_unlock(&sem_table_lock);
		TRACE(("create_sem_etc: no free slots\n"));
		return B_NO_MORE_SEMS;
	}
	
	// Create dispatch semaphore
	dispatch_semaphore_t dsem = dispatch_semaphore_create(count);
	if (!dsem) {
		pthread_mutex_unlock(&sem_table_lock);
		TRACE(("create_sem_etc: dispatch_semaphore_create failed\n"));
		return B_NO_MORE_SEMS;
	}
	
	// Set up metadata
	sem_table[id].dsem = dsem;
	sem_table[id].count = count;
	sem_table[id].owner = owner;
	sem_table[id].in_use = true;
	
	if (name) {
		strncpy(sem_table[id].name, name, B_OS_NAME_LENGTH - 1);
		sem_table[id].name[B_OS_NAME_LENGTH - 1] = '\0';
	} else {
		strcpy(sem_table[id].name, "unnamed sem");
	}
	
	pthread_mutex_unlock(&sem_table_lock);
	
	TRACE(("create_sem_etc: created sem %d (%s) with count %d\n", id, sem_table[id].name, count));
	return id;
}

sem_id create_sem(int32 count, const char *name) {
	return create_sem_etc(count, name, getpid());
}

status_t delete_sem(sem_id id) {
	return delete_sem_etc(id, 0, false);
}

status_t delete_sem_etc(sem_id id, status_t return_code, bool interrupted) {
	if (id < 0 || id >= MAX_SEMS)
		return B_BAD_SEM_ID;
	
	pthread_mutex_lock(&sem_table_lock);
	
	if (!sem_table[id].in_use) {
		pthread_mutex_unlock(&sem_table_lock);
		TRACE(("delete_sem_etc: sem %d not in use\n", id));
		return B_BAD_SEM_ID;
	}
	
	dispatch_semaphore_t dsem = sem_table[id].dsem;
	int32 count = sem_table[id].count;
	sem_table[id].dsem = NULL;
	sem_table[id].in_use = false;
	
	pthread_mutex_unlock(&sem_table_lock);
	
	// Wake any waiters by signaling the semaphore
	// Note: We don't call dispatch_release because GCD will crash if the
	// semaphore count is not at its initial value. This leaks the dispatch
	// semaphore object, but prevents crashes.
	if (dsem) {
		while (count < 0) {
			dispatch_semaphore_signal(dsem);
			count++;
		}
	}
	
	TRACE(("delete_sem_etc: deleted sem %d\n", id));
	return B_OK;
}

status_t acquire_sem(sem_id id) {
	return acquire_sem_etc(id, 1, 0, 0);
}

status_t acquire_sem_etc(sem_id id, int32 count, uint32 flags, bigtime_t timeout) {
	if (id < 0 || id >= MAX_SEMS)
		return B_BAD_SEM_ID;
	
	if (count <= 0)
		return B_BAD_VALUE;
	
	pthread_mutex_lock(&sem_table_lock);
	
	if (!sem_table[id].in_use) {
		pthread_mutex_unlock(&sem_table_lock);
		TRACE(("acquire_sem_etc: sem %d not in use\n", id));
		return B_BAD_SEM_ID;
	}
	
	dispatch_semaphore_t dsem = sem_table[id].dsem;
	pthread_mutex_unlock(&sem_table_lock);
	
	// Handle timeout
	dispatch_time_t dispatch_timeout;
	
	if (flags & B_RELATIVE_TIMEOUT) {
		if (timeout == 0) {
			// Non-blocking
			dispatch_timeout = DISPATCH_TIME_NOW;
		} else if (timeout == B_INFINITE_TIMEOUT) {
			dispatch_timeout = DISPATCH_TIME_FOREVER;
		} else {
			// Convert microseconds to nanoseconds
			dispatch_timeout = dispatch_time(DISPATCH_TIME_NOW, timeout * 1000);
		}
	} else if (flags & B_ABSOLUTE_TIMEOUT) {
		// Convert absolute time to relative
		struct timeval now;
		gettimeofday(&now, NULL);
		bigtime_t now_us = (bigtime_t)now.tv_sec * 1000000LL + now.tv_usec;
		bigtime_t relative = timeout - now_us;
		
		if (relative <= 0) {
			dispatch_timeout = DISPATCH_TIME_NOW;
		} else {
			dispatch_timeout = dispatch_time(DISPATCH_TIME_NOW, relative * 1000);
		}
	} else {
		// Default: infinite wait
		dispatch_timeout = DISPATCH_TIME_FOREVER;
	}
	
	// Update tracked count BEFORE waiting so get_sem_count shows negative if blocked
	pthread_mutex_lock(&sem_table_lock);
	if (sem_table[id].in_use) {
		sem_table[id].count -= count;
	}
	pthread_mutex_unlock(&sem_table_lock);
	
	// Acquire the semaphore count times
	for (int32 i = 0; i < count; i++) {
		long result = dispatch_semaphore_wait(dsem, dispatch_timeout);
		
		if (result != 0) {
			// Timeout occurred, release any we already acquired
			for (int32 j = 0; j < i; j++) {
				dispatch_semaphore_signal(dsem);
			}
			// Restore the count since we didn't actually acquire
			pthread_mutex_lock(&sem_table_lock);
			if (sem_table[id].in_use) {
				sem_table[id].count += count;
			}
			pthread_mutex_unlock(&sem_table_lock);
			TRACE(("acquire_sem_etc: sem %d timed out\n", id));
			// Return B_WOULD_BLOCK for zero timeout, B_TIMED_OUT otherwise
			if ((flags & B_RELATIVE_TIMEOUT) && timeout == 0) {
				return B_WOULD_BLOCK;
			}
			return B_TIMED_OUT;
		}
	}
	pthread_mutex_unlock(&sem_table_lock);
	
	TRACE(("acquire_sem_etc: acquired sem %d (count %d)\n", id, count));
	return B_OK;
}

status_t release_sem(sem_id id) {
	return release_sem_etc(id, 1, 0);
}

status_t release_sem_etc(sem_id id, int32 count, uint32 flags) {
	if (id < 0 || id >= MAX_SEMS)
		return B_BAD_SEM_ID;
	
	if (count <= 0)
		return B_BAD_VALUE;
	
	pthread_mutex_lock(&sem_table_lock);
	
	if (!sem_table[id].in_use) {
		pthread_mutex_unlock(&sem_table_lock);
		TRACE(("release_sem_etc: sem %d not in use\n", id));
		return B_BAD_SEM_ID;
	}
	
	dispatch_semaphore_t dsem = sem_table[id].dsem;
	
	// Update tracked count
	sem_table[id].count += count;
	
	pthread_mutex_unlock(&sem_table_lock);
	
	// Signal the semaphore count times
	for (int32 i = 0; i < count; i++) {
		dispatch_semaphore_signal(dsem);
	}
	
	TRACE(("release_sem_etc: released sem %d (count %d)\n", id, count));
	return B_OK;
}

status_t get_sem_count(sem_id id, int32 *thread_count) {
	if (id < 0 || id >= MAX_SEMS)
		return B_BAD_SEM_ID;
	
	if (!thread_count)
		return B_BAD_VALUE;
	
	pthread_mutex_lock(&sem_table_lock);
	
	if (!sem_table[id].in_use) {
		pthread_mutex_unlock(&sem_table_lock);
		return B_BAD_SEM_ID;
	}
	
	// Return the tracked count (updated on acquire/release)
	*thread_count = sem_table[id].count;
	
	pthread_mutex_unlock(&sem_table_lock);
	
	return B_OK;
}

status_t _get_sem_info(sem_id id, sem_info *info, size_t infoSize) {
	if (id < 0 || id >= MAX_SEMS)
		return B_BAD_SEM_ID;
	
	if (!info || infoSize < sizeof(sem_info))
		return B_BAD_VALUE;
	
	pthread_mutex_lock(&sem_table_lock);
	
	if (!sem_table[id].in_use) {
		pthread_mutex_unlock(&sem_table_lock);
		return B_BAD_SEM_ID;
	}
	
	info->sem = id;
	info->team = sem_table[id].owner;
	strncpy(info->name, sem_table[id].name, B_OS_NAME_LENGTH - 1);
	info->name[B_OS_NAME_LENGTH - 1] = '\0';
	info->count = sem_table[id].count;
	info->latest_holder = -1; // Not tracked in this implementation
	
	pthread_mutex_unlock(&sem_table_lock);
	
	return B_OK;
}

status_t _get_next_sem_info(team_id team, int32 *cookie, sem_info *info, size_t infoSize) {
	if (!cookie || !info || infoSize < sizeof(sem_info))
		return B_BAD_VALUE;
	
	if (team < 0)
		return B_BAD_TEAM_ID;
	
	pthread_mutex_lock(&sem_table_lock);
	
	int32 start = *cookie;
	if (start < 0)
		start = 0;
	
	// Find next semaphore owned by this team
	for (int i = start; i < MAX_SEMS; i++) {
		if (sem_table[i].in_use && (team == 0 || sem_table[i].owner == team)) {
			info->sem = i;
			info->team = sem_table[i].owner;
			strncpy(info->name, sem_table[i].name, B_OS_NAME_LENGTH - 1);
			info->name[B_OS_NAME_LENGTH - 1] = '\0';
			info->count = sem_table[i].count;
			info->latest_holder = -1;
			
			*cookie = i + 1;
			pthread_mutex_unlock(&sem_table_lock);
			return B_OK;
		}
	}
	
	pthread_mutex_unlock(&sem_table_lock);
	return B_BAD_VALUE;
}

status_t set_sem_owner(sem_id id, team_id team) {
	if (id < 0 || id >= MAX_SEMS)
		return B_BAD_SEM_ID;
	
	pthread_mutex_lock(&sem_table_lock);
	
	if (!sem_table[id].in_use) {
		pthread_mutex_unlock(&sem_table_lock);
		return B_BAD_SEM_ID;
	}
	
	sem_table[id].owner = team;
	
	pthread_mutex_unlock(&sem_table_lock);
	
	return B_OK;
}
