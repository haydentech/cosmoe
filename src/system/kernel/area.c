/*------------------------------------------------------------------------------
//	Copyright (c) 2003 Tom Marshall, 2003-2025 Bill Hayden
//
//	Permission is hereby granted, free of charge, to any person obtaining a
//	copy of this software and associated documentation files (the "Software"),
//	to deal in the Software without restriction, including without limitation
//	the rights to use, copy, modify, merge, publish, distribute, sublicense,
//	and/or sell copies of the Software, and to permit persons to whom the
//	Software is furnished to do so, subject to the following conditions:
//
//	The above copyright notice and this permission notice shall be included in
//	all copies or substantial portions of the Software.
//
//	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//	FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//	DEALINGS IN THE SOFTWARE.
//
//	File Name:		area.c
//	Authors:		Tom Marshall (tommy@tig-grr.com)
//					Bill Hayden (hayden@haydentech.com)
//----------------------------------------------------------------------------*/


// Platform-specific system headers
#ifdef _WIN32
	#include <process.h>  // For getpid() on Windows
#else
	#include <unistd.h>   // For getpid() on POSIX
#endif

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <string.h>

#include "../../config.h"

#include <SupportDefs.h>
#include <StorageDefs.h>	/* Just because BeOS apps expect this here */
#include <OS.h>

#define TRACE_AREA 0
#if TRACE_AREA
#	define TRACE(x) dprintf x
#	define TRACE_BLOCK(x) dprintf x
#else
#	define TRACE(x) ;
#	define TRACE_BLOCK(x) ;
#endif

#define dprintf printf

  // Area IDs
#define AREA_ID_MAX  96
#define AREA_ID_FREE -1


area_info* sAreaMap = NULL;
// Reference counts for shared memory - tracks how many areas point to the same memory
int32* sAreaRefCounts = NULL;
// Mutex to protect access to area map
static pthread_mutex_t sAreaMapLock = PTHREAD_MUTEX_INITIALIZER;



void init_area_map(void)
{
	pthread_mutex_lock(&sAreaMapLock);
	
	// Check again after acquiring lock (double-checked locking)
	if (sAreaMap != NULL) {
		pthread_mutex_unlock(&sAreaMapLock);
		return;
	}
	
	/* create and initialize a new area table in shared memory */
	sAreaMap = (area_info*)malloc(sizeof(area_info) * AREA_ID_MAX);
	if (sAreaMap == NULL)
	{
		printf( "FATAL: init_area_map() failed: %s\n", strerror(errno) );
		pthread_mutex_unlock(&sAreaMapLock);
		return;
	}

	sAreaRefCounts = (int32*)calloc(AREA_ID_MAX, sizeof(int32));
	if (sAreaRefCounts == NULL)
	{
		printf( "FATAL: init_area_map() refcount allocation failed: %s\n", strerror(errno) );
		free(sAreaMap);
		sAreaMap = NULL;
		pthread_mutex_unlock(&sAreaMapLock);
		return;
	}

	for(area_id n = 0; n < AREA_ID_MAX; n++)
	{
		sAreaMap[n].area = AREA_ID_FREE;
	}
	
	pthread_mutex_unlock(&sAreaMapLock);
}


area_id create_area(const char* name, void** start_addr, uint32 addr_spec, size_t size, uint32 lock, uint32 protection)
{
	(void)addr_spec;  // Unused - address specification not implemented yet
	
	if (sAreaMap == NULL)
		init_area_map();
	
	pthread_mutex_lock(&sAreaMapLock);

	for (area_id n = 0; n < AREA_ID_MAX; n++)
	{
		if (sAreaMap[n].area == AREA_ID_FREE)
		{		
			sAreaMap[n].address = malloc(size);
			if (sAreaMap[n].address == NULL)
			{
				printf("create_area() failed: %s\n", strerror(errno));
				pthread_mutex_unlock(&sAreaMapLock);
				return B_NO_MEMORY;
			}
			
			if (start_addr != NULL)
			{
				*start_addr = sAreaMap[n].address;
			}

			strncpy(sAreaMap[n].name, name, B_OS_NAME_LENGTH - 1);
			sAreaMap[n].name[B_OS_NAME_LENGTH - 1] = '\0';
			sAreaMap[n].area = n;
			sAreaMap[n].size = size;
			sAreaMap[n].lock = lock;
			sAreaMap[n].protection = protection;
			sAreaMap[n].team = getpid();
			sAreaMap[n].ram_size = size;
			sAreaRefCounts[n] = 1;  // Initial reference count
			pthread_mutex_unlock(&sAreaMapLock);
			return n;
		}
	}
	pthread_mutex_unlock(&sAreaMapLock);
	return B_NO_MEMORY;
}


area_id clone_area(const char* name, void** dest_addr, uint32 addr_spec, uint32 protection, area_id source)
{
	(void)addr_spec;  // Unused - address specification not implemented yet
	(void)protection; // Unused - inherits protection from source
	
	if (sAreaMap == NULL)
		init_area_map();
	
	pthread_mutex_lock(&sAreaMapLock);

	if (source < 0 || source >= AREA_ID_MAX || sAreaMap == NULL || sAreaMap[source].area == AREA_ID_FREE)
	{
		printf( "clone_area(): AREA IS FREE\n" );
		pthread_mutex_unlock(&sAreaMapLock);
		return B_ERROR;
	}

	size_t nSize = sAreaMap[source].size;
	uint32 nProtection = sAreaMap[source].protection;
	uint32 lock = sAreaMap[source].lock;

	for (area_id n = 0; n < AREA_ID_MAX; n++)
	{
		if (sAreaMap[n].area == AREA_ID_FREE)
		{
			// Share the same physical memory as the source area
			sAreaMap[n].address = sAreaMap[source].address;

			if (sAreaMap[n].address == NULL)
			{
				printf( "clone_area() failed: source has NULL address\n" );
				pthread_mutex_unlock(&sAreaMapLock);
				return B_NO_MEMORY;
			}

			// Increment reference count for the shared memory
			sAreaRefCounts[source]++;

			if (dest_addr != NULL)
			{
				*dest_addr = sAreaMap[n].address;
			}

			strncpy(sAreaMap[n].name, name, B_OS_NAME_LENGTH - 1);
			sAreaMap[n].name[B_OS_NAME_LENGTH - 1] = '\0';
			sAreaMap[n].area = n;
			sAreaMap[n].size = nSize;
			sAreaMap[n].lock = lock;
			sAreaMap[n].protection = nProtection;
			sAreaMap[n].team = getpid();
			sAreaMap[n].ram_size = nSize;
			// Clone shares the source's reference count (both use sAreaRefCounts[source])
			pthread_mutex_unlock(&sAreaMapLock);
			return n;
		}
	}

	pthread_mutex_unlock(&sAreaMapLock);
	return B_NO_MEMORY;
}


area_id
find_area(const char *name)
{
	if (sAreaMap == NULL)
		init_area_map();
	
	pthread_mutex_lock(&sAreaMapLock);

	for (area_id n = 0; n < AREA_ID_MAX; n++)
	{
		if(sAreaMap[n].area != AREA_ID_FREE)
		{
			if(strcmp(name, sAreaMap[n].name) == 0)
			{
				pthread_mutex_unlock(&sAreaMapLock);
				return n;
			}
		}
	}

	pthread_mutex_unlock(&sAreaMapLock);
	return B_NAME_NOT_FOUND;
}


area_id
area_for(void *address)
{
	if (sAreaMap == NULL)
		init_area_map();
	
	pthread_mutex_lock(&sAreaMapLock);

	for (area_id n = 0; n < AREA_ID_MAX; n++)
	{
		if (sAreaMap[n].area != AREA_ID_FREE)
		{
			char *area_start = (char *)sAreaMap[n].address;
			char *area_end = area_start + sAreaMap[n].size;
			if ((char *)address >= area_start && (char *)address < area_end)
			{
				pthread_mutex_unlock(&sAreaMapLock);
				return n;
			}
		}
	}

	pthread_mutex_unlock(&sAreaMapLock);
	return B_ERROR;
}


status_t delete_area( area_id hArea )
{
	if( hArea < 0 || hArea >= AREA_ID_MAX || sAreaMap == NULL ||
		sAreaMap[hArea].area == AREA_ID_FREE )
	{
		return B_ERROR;
	}
	
	pthread_mutex_lock(&sAreaMapLock);

	// Find the original area that owns this memory (could be self or source)
	area_id owner = hArea;
	void* addr = sAreaMap[hArea].address;
	
	// Find which area originally allocated this memory
	for (area_id i = 0; i < AREA_ID_MAX; i++) {
		if (sAreaMap[i].area != AREA_ID_FREE && 
		    sAreaMap[i].address == addr &&
		    sAreaRefCounts[i] > 0) {
			owner = i;
			break;
		}
	}
	
	// Decrement reference count
	if (sAreaRefCounts[owner] > 0) {
		sAreaRefCounts[owner]--;
		
		// Only free memory when no more references exist
		if (sAreaRefCounts[owner] == 0) {
			free(sAreaMap[hArea].address);
		}
	}
	
	sAreaMap[hArea].address = NULL;
	sAreaMap[hArea].area = AREA_ID_FREE;

	pthread_mutex_unlock(&sAreaMapLock);
	return B_OK;
}


status_t _get_area_info( area_id hArea, area_info* psInfo, size_t size )
{
	(void)size;  // Unused - for future compatibility
	
	if( hArea < 0 || hArea >= AREA_ID_MAX || sAreaMap == NULL ||
		sAreaMap[hArea].area == AREA_ID_FREE || psInfo == NULL )
	{
		return B_BAD_VALUE;
	}
	
	pthread_mutex_lock(&sAreaMapLock);
	*psInfo = sAreaMap[hArea];
	pthread_mutex_unlock(&sAreaMapLock);
	return B_OK;
}


status_t resize_area(area_id id, size_t new_size)
{
	if (id < 0 || id >= AREA_ID_MAX || sAreaMap == NULL || sAreaMap[id].area == AREA_ID_FREE)
	{
		return B_BAD_VALUE;
	}
	
	pthread_mutex_lock(&sAreaMapLock);

	if (sAreaMap[id].size != new_size)
	{
		void *new_address = realloc(sAreaMap[id].address, new_size);
		if (new_address == NULL)
		{
			printf("resize_area() failed: %s\n", strerror(errno));
			pthread_mutex_unlock(&sAreaMapLock);
			return B_NO_MEMORY;
		}

		sAreaMap[id].address = new_address;
		sAreaMap[id].size = new_size;
		sAreaMap[id].ram_size = new_size;
	}

	pthread_mutex_unlock(&sAreaMapLock);
	return B_OK;
}


// private os function to set the owning team of an area
status_t _kern_transfer_area(area_id id, void **_address, uint32 addressSpec, team_id target)
{
	(void)_address;    // Unused - for future compatibility
	(void)addressSpec; // Unused - for future compatibility
	
	if (id < 0 || id >= AREA_ID_MAX || sAreaMap == NULL || sAreaMap[id].area == AREA_ID_FREE)
		return B_BAD_VALUE;
	
	pthread_mutex_lock(&sAreaMapLock);
	sAreaMap[id].team = target;
	pthread_mutex_unlock(&sAreaMapLock);

	return B_NO_ERROR;
}


status_t set_area_protection(area_id id, uint32 newProtection)
{
	if (id < 0 || id >= AREA_ID_MAX || sAreaMap == NULL || sAreaMap[id].area == AREA_ID_FREE)
		return B_BAD_VALUE;
	
	pthread_mutex_lock(&sAreaMapLock);
	sAreaMap[id].protection = newProtection;
	pthread_mutex_unlock(&sAreaMapLock);
	
	return B_NO_ERROR;
}


int32 _area_count_for_team(team_id id)
{
	if (sAreaMap == NULL)
		return 0;
	
	pthread_mutex_lock(&sAreaMapLock);
	
	int32 areaCount = 0;

	for (area_id n = 0; n < AREA_ID_MAX; n++)
	{
		if (sAreaMap[n].area != AREA_ID_FREE && sAreaMap[n].team == id)
				areaCount++;
	}

	pthread_mutex_unlock(&sAreaMapLock);
	return areaCount;
}