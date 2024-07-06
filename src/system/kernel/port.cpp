/*
** Copyright 2004, Bill Hayden. All rights reserved.
 * Copyright 2011, Ingo Weinhold, ingo_weinhold@gmx.de.
 * Copyright 2002-2010, Axel Dörfler, axeld@pinc-software.de.
 * Distributed under the terms of the MIT License.
 *
 * Copyright 2001, Mark-Jan Bastian. All rights reserved.
 * Distributed under the terms of the NewOS License.
 */


/*!	Ports for IPC */

#include <OS.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define kprintf printf
#define panic printf

//#define TRACE_PORTS
#ifdef TRACE_PORTS
#	define TRACE(x) printf x
#else
#	define TRACE(x)
#endif

#define DEBUG

#define PORT_MAX_MESSAGE_SIZE 65536

typedef struct port_message {
	int32		code;
	char		buffer_chain[PORT_MAX_MESSAGE_SIZE];
	size_t		size;
} port_message;

typedef struct Port {
	port_id				id;
	team_id				owner;
	int32		 		capacity;
	int32		original_capacity;
	sem_id		lock;
	char		name[B_OS_NAME_LENGTH];
	sem_id		read_sem;
	sem_id		write_sem;
	int32				total_count;
		// messages read from port since creation
	int		queue_shm;
	int32		head;
	int32		tail;
} Port;

// hidden API
static void _dump_port_info(struct Port *port);


#define MAX_QUEUE_LENGTH 256

// sMaxPorts must be power of 2
int32 sMaxPorts = 256;
static int32 sUsedPorts = 0;
static area_id sPortArea = -1;
static void *sPortMemory = NULL;
static sem_id sPortSem = -1;

static Port *sPorts = NULL;
static port_id* sNextPort = NULL;

static bool sPortsActive = false;
static int32 sFirstFreeSlot = 1;

#define GRAB_PORT_LIST_LOCK() do {} while(acquire_sem(sPortSem) == B_INTERRUPTED)
#define RELEASE_PORT_LIST_LOCK() release_sem(sPortSem);
#define GRAB_PORT_LOCK(s) if ((s).lock != -1) do {} while(acquire_sem((s).lock) == B_INTERRUPTED)
#define RELEASE_PORT_LOCK(s) if ((s).lock != -1) release_sem((s).lock)

static status_t port_init(void);
static void teardown_ports(void);
static int delete_owned_ports(team_id owner);

//	#pragma mark -


static int
dump_port_list(int argc, char** argv)
{
	const char* name = NULL;
	team_id owner = -1;

	if (argc > 2) {
		if (!strcmp(argv[1], "team") || !strcmp(argv[1], "owner"))
			owner = strtoul(argv[2], NULL, 0);
		else if (!strcmp(argv[1], "name"))
			name = argv[2];
	} else if (argc > 1)
		owner = strtoul(argv[1], NULL, 0);
	kprintf("port             id  cap  r-sem  r-cnt  w-sem  w-cnt    total   team  name\n");

	for (int i = 0; i < sMaxPorts; i++) {
		Port *port = &sPorts[i];
		if (port->id < 0
			|| (owner != -1 && port->owner != owner)
			|| (name != NULL && strstr(port->name, name) == NULL))
			continue;

		int32 readCount, writeCount;
		get_sem_count(port->read_sem, &readCount);
		get_sem_count(port->write_sem, &writeCount);
		kprintf("%p %8" B_PRId32 " %4" B_PRId32 " %6ld %6ld %6ld %6ld %8ld %6ld  %s\n", port,
			port->id, port->capacity, port->read_sem, readCount,
			port->write_sem, writeCount, port->total_count, port->owner,
			port->name);
	}

	return 0;
}


static void
_dump_port_info(Port* port)
{
	int32 count;

	kprintf("PORT: %p\n", port);
	kprintf(" id:              %" B_PRId32 "\n", port->id);
	kprintf(" name:            \"%s\"\n", port->name);
	kprintf(" owner:           %" B_PRId32 "\n", port->owner);
	kprintf(" capacity:        %" B_PRId32 "\n", port->capacity);
	kprintf(" read_sem:        %" B_PRId32 "\n", port->read_sem);
	kprintf(" write_sem:       %" B_PRId32 "\n", port->write_sem);
 	get_sem_count(port->read_sem, &count);
 	kprintf(" read_sem count:  %" B_PRId32 "\n", count);
 	get_sem_count(port->write_sem, &count);
	kprintf(" write_sem count: %" B_PRId32 "\n", count);
	kprintf(" total count:     %" B_PRId32 "\n", port->total_count);
}


static int
dump_port_info(int argc, char** argv)
{
	const char *name = NULL;
	sem_id sem = -1;
	int i;
	
	port_init();
	 
	if (!sPortsActive)
	{
		kprintf("No Cosmoe ports in use.\n");
		return 0;
	}

	if (argc < 2) {
		dump_port_list(argc, argv);
		return 0;
	}

	if (argc > 2) {
		if (!strcmp(argv[1], "address")) {
			_dump_port_info((Port *)strtoul(argv[2], NULL, 0));
			return 0;
		} else if (!strcmp(argv[1], "sem"))
			sem = strtoul(argv[2], NULL, 0);
		else if (!strcmp(argv[1], "name"))
			name = argv[2];
	} else if (isdigit(argv[1][0])) {
		// if the argument looks like a number, treat it as such
		uint32 num = strtoul(argv[1], NULL, 0);
		uint32 slot = num % sMaxPorts;
		if (sPorts[slot].id != (int)num) {
			kprintf("port %" B_PRId32 " (%#" B_PRIx32 ") doesn't exist!\n",
				num, num);
			return 0;
		}
		_dump_port_info(&sPorts[slot]);
		return 0;
	} else
		name = argv[1];

	// walk through the ports list, trying to match name
	for (i = 0; i < sMaxPorts; i++) {
		if ((name != NULL && sPorts[i].name != NULL
				&& !strcmp(name, sPorts[i].name))
			|| (sem != -1 && (sPorts[i].read_sem == sem
				|| sPorts[i].write_sem == sem))) {
			_dump_port_info(&sPorts[i]);
			return 0;
		}
	}

	return 0;
}


// #pragma mark - internal helper functions


static void
put_port_msg(port_message *msg)
{
	msg->buffer_chain[0] = '\0';
	msg->code = 0;
	msg->size = 0;
}


/*!	You need to own the port's lock when calling this function */
static inline bool
is_port_closed(int32 slot)
{
	return sPorts[slot].capacity == 0;
}


/*!	Fills the port_info structure with information from the specified
	port.
	The port's lock must be held when called.
*/
static void
fill_port_info(Port* port, port_info* info, size_t size)
{
	info->port = port->id;
	info->team = port->owner;
	info->capacity = port->capacity;

	int32 count;
	get_sem_count(port->read_sem, &count);
	if (count < 0)
		count = 0;

	info->queue_count = count;
	info->total_count = port->total_count;

	strlcpy(info->name, port->name, B_OS_NAME_LENGTH);
}


//	#pragma mark - private kernel API


/*! This function deletes all the ports that are owned by the passed team.
*/
int
delete_owned_ports(team_id owner)
{
	int count = 0;

	TRACE(("delete_owned_ports(owner = %ld)\n", owner));

	if (!sPortsActive)
		return B_BAD_PORT_ID;

	GRAB_PORT_LIST_LOCK();

	for (int i = 0; i < sMaxPorts; i++) {
		if (sPorts[i].id != -1 && sPorts[i].owner == owner) {
			port_id id = sPorts[i].id;

			RELEASE_PORT_LIST_LOCK();

			delete_port(id);
			count++;

			GRAB_PORT_LIST_LOCK();
		}
	}

	RELEASE_PORT_LIST_LOCK();

	return count;
}


int32
port_max_ports(void)
{
	return sMaxPorts;
}


int32
port_used_ports(void)
{
	return sUsedPorts;
}


status_t
port_init(void)
{
	if (sPorts)
		return B_OK;

	size_t size = sizeof(sem_id) + sizeof(port_id) + (sizeof(struct Port) * sMaxPorts);
	key_t table_key;
	bool created = true;

	/* grab a (hopefully) unique key for our table */
	table_key = ftok("/usr/local/bin/appserver", (int)'P');
	TRACE(("Using key %x for the port table\n", (int)table_key));
	TRACE(("The size of the port table is %ld bytes\n", (long)size));

	// create and initialize ports table
	sPortArea = shmget(table_key, size, IPC_CREAT | IPC_EXCL | 0700);
	if (sPortArea == -1 && errno == EEXIST)
	{
		/* get existing semaphore table in shared memory */
		sPortArea = shmget(table_key, size, IPC_CREAT | 0700);
		TRACE(("Using pre-existing master ports table\n"));
		created = false;
	}

	if (sPortArea < 0) {
		TRACE(("FATAL: Couldn't setup port table due to "
			"error %d (%s)\n", errno, strerror(errno)));
		return B_ERROR;
	}

	/* point our local table at the master table */
	sPortMemory = shmat(sPortArea, NULL, 0);
	if (sPortMemory == (void *) -1)
	{
		TRACE(("FATAL: Couldn't attach port table: %s\n", strerror (errno)));
		return B_ERROR;
	}

	sNextPort = (port_id *)sPortMemory + sizeof(sem_id);
	sPorts = (Port*)sNextPort + sizeof(port_id);

	if (created)
	{
		int i;
		memset(sNextPort, 0, size);
		for (i = 0; i < sMaxPorts; i++)
		{
			sPorts[i].id = -1;
			sPorts[i].lock = -1;
		}

		sPortSem = create_sem(1, "master port lock");
		*((sem_id *)sPortMemory) = sPortSem;
	}
	else
		sPortSem = *((sem_id *)sPortMemory);

	atexit(teardown_ports);

	TRACE(("port_init: exit\n"));

	sPortsActive = true;
	return B_OK;
}


//	#pragma mark - public kernel API


port_id
create_port(int32 queueLength, const char* name)
{
	sem_id readSem, writeSem;
	sem_id portSem;
	port_id returnValue;
	status_t status;
	team_id	owner;

	TRACE(("create_port(queueLength = %ld, name = \"%s\")\n", queueLength,
		name));

	if (!sPortsActive)
		port_init();
	if (!sPortsActive) {
		return B_BAD_PORT_ID;
	}
	if (queueLength < 1 || queueLength > MAX_QUEUE_LENGTH)
		return B_BAD_VALUE;

	// check early on if there are any free port slots to use
	if (atomic_add(&sUsedPorts, 1) >= sMaxPorts) {
		status = B_NO_MORE_PORTS;
		goto err1;
	}

	// check & dup name
	if (name == NULL)
		name = "unnamed port";

	// create read sem with owner set to -1
	// ToDo: should be B_SYSTEM_TEAM
	portSem = create_sem_etc(1, name, -1);
	if (portSem < B_OK) {
		// cleanup
		return portSem;
	}

	// create read sem with owner set to -1
	// ToDo: should be B_SYSTEM_TEAM
	readSem = create_sem_etc(0, name, -1);
	if (readSem < B_OK) {
		// cleanup
		delete_sem(portSem);
		return readSem;
	}

	// create write sem
	writeSem = create_sem_etc(queueLength, name, -1);
	if (writeSem < B_OK) {
		// cleanup
		delete_sem(readSem);
		delete_sem(portSem);
		return writeSem;
	}

	owner = team_get_current_team_id();

	GRAB_PORT_LIST_LOCK();

	// find the first empty spot
	for (int32 slot = 0; slot < sMaxPorts; slot++) {
		int32 i = (slot + sFirstFreeSlot) % sMaxPorts;

		if (sPorts[i].id == -1) {
			key_t  port_shm_key;
			const size_t size = sizeof(port_message) * queueLength;
			void* msg_queue;

			// make the port_id be a multiple of the slot it's in
			if (i >= *sNextPort % sMaxPorts)
				*sNextPort += i - *sNextPort % sMaxPorts;
			else
				*sNextPort += sMaxPorts - (*sNextPort % sMaxPorts - i);
			sFirstFreeSlot = slot + 1;

			if (sPorts[i].lock == -1)
				sPorts[i].lock = create_sem(1, "port lock");
			GRAB_PORT_LOCK(sPorts[i]);
			sPorts[i].id = (*sNextPort)++;
			RELEASE_PORT_LIST_LOCK();

			strncpy(sPorts[i].name, name, B_OS_NAME_LENGTH);
			sPorts[i].capacity = queueLength;
			sPorts[i].original_capacity = queueLength;
			sPorts[i].owner = owner;
			sPorts[i].name[B_OS_NAME_LENGTH - 1] = '\0';

			sPorts[i].read_sem	= readSem;
			sPorts[i].write_sem	= writeSem;
			sPorts[i].lock = portSem;

			sPorts[i].total_count = 0;

			sPorts[i].head		= 0;
			sPorts[i].tail		= 0;

			/* grab a (hopefully) unique key for our port */
			char path[B_PATH_NAME_LENGTH];
			static int static_port_count = 0;
			sprintf(path, "/proc/%d/exe", getpid());
			port_shm_key = ftok(path, ++static_port_count);	/* HACK ALERT */

			TRACE(("create_port: generated port queue key %d from %s + %d.\n", port_shm_key, path, static_port_count));
			/* create and initialize a new semaphore table in shared memory */
			sPorts[i].queue_shm = shmget(port_shm_key, size, IPC_CREAT | IPC_EXCL | 0700);
			if (sPorts[i].queue_shm == -1 && errno == EEXIST)
			{
				/* TODO: this should be FATAL */
				/* TODO: we don't know if it is large enough */
				sPorts[i].queue_shm = shmget(port_shm_key, size, IPC_CREAT | 0700);
				TRACE(("WARNING: Using pre-existing port queue.\n"));
			}

			if (sPorts[i].queue_shm < 0)
			{
				TRACE(("FATAL: Couldn't setup port queue with key %d: %s\n",
						port_shm_key,
						strerror(errno)));
				returnValue = B_NO_MEMORY;
				sPorts[i].id = -1;
				goto cleanup;
			}

			TRACE(("Port %d named %s is using shm key %x\n", i, name, port_shm_key));

			/* point our local table at the master table */
			msg_queue = shmat(sPorts[i].queue_shm, NULL, 0);
			if (msg_queue == (void *) -1)
			{
				printf("Couldn't attach port queue: %s\n", strerror(errno));
				returnValue = B_NO_MEMORY;
				sPorts[i].id = -1;
				goto cleanup;
			}

			TRACE(("Port %d is now attached successfully\n", i));

			port_message* p = (port_message*)msg_queue;
			for (int j = 0; j < queueLength; j++)
			{
				p[j].buffer_chain[0] = '\0';
				p[j].code = 0;
				p[j].size = 0;
			}

			shmdt(msg_queue);

			returnValue = sPorts[i].id;

			RELEASE_PORT_LOCK(sPorts[i]);

			TRACE(("create_port() done: port created %ld\n", id));

			return returnValue;
		}
	}

	// not enough ports...

	// TODO: due to sUsedPorts, this cannot happen anymore - as
	//		long as sMaxPorts stays constant over the kernel run
	//		time (which it should be). IOW we could simply panic()
	//		here.

	RELEASE_PORT_LIST_LOCK();

	status = B_NO_MORE_PORTS;

	TRACE(("create_port(): B_NO_MORE_PORTS\n"));

	// cleanup
cleanup:
	delete_sem(writeSem);
	delete_sem(readSem);
	delete_sem(portSem);
	// Cosmoe has static allocation, so nothing to free
err1:
	atomic_add(&sUsedPorts, -1);

	return status;
}


status_t
close_port(port_id id)
{
	sem_id readSem, writeSem;
	int32 slot;

	TRACE(("close_port(id = %ld)\n", id));

	if (!sPortsActive)
		port_init();
	if (!sPortsActive || id < 0)
		return B_BAD_PORT_ID;

	slot = id % sMaxPorts;

	// walk through the sem list, trying to match name
	GRAB_PORT_LOCK(sPorts[slot]);

	if (sPorts[slot].id != id) {
		RELEASE_PORT_LOCK(sPorts[slot]);
		TRACE(("close_port: invalid port_id %ld\n", id));
		return B_BAD_PORT_ID;
	}

	// mark port to disable writing - deleting the semaphores will
	// wake up waiting read/writes
	sPorts[slot].capacity = 0;
	readSem = sPorts[slot].read_sem;
	writeSem = sPorts[slot].write_sem;

	RELEASE_PORT_LOCK(sPorts[slot]);

	delete_sem(readSem);
	delete_sem(writeSem);

	return B_NO_ERROR;
}


status_t
delete_port(port_id id)
{
	TRACE(("delete_port(id = %ld)\n", id));

	if (!sPortsActive)
		port_init();
	if (!sPortsActive || id < 0)
		return B_BAD_PORT_ID;

	int32 slot = id % sMaxPorts;

	GRAB_PORT_LOCK(sPorts[slot]);

	if (sPorts[slot].id != id) {
		RELEASE_PORT_LOCK(sPorts[slot]);

		TRACE(("delete_port: invalid port_id %ld\n", id));
		return B_BAD_PORT_ID;
	}

	/* mark port as invalid */
	sPorts[slot].id	= -1;
	sem_id readSem = sPorts[slot].read_sem;
	sem_id writeSem = sPorts[slot].write_sem;
	sPorts[slot].name[0] = '\0';
	sem_id portSem = sPorts[slot].lock;

	RELEASE_PORT_LOCK(sPorts[slot]);

	// update the first free slot hint in the array
	GRAB_PORT_LIST_LOCK();
	if (slot < sFirstFreeSlot)
		sFirstFreeSlot = slot;
	RELEASE_PORT_LIST_LOCK();

	atomic_add(&sUsedPorts, -1);

	sPorts[slot].lock = -1;

	// free the queue
	// Not necessary on Cosmoe since allocation is static

	// release the threads that were blocking on this port by deleting the sem
	// read_port() will see the B_BAD_SEM_ID acq_sem() return value, and act accordingly
	delete_sem(portSem);
	delete_sem(readSem);
	delete_sem(writeSem);

	/* schedule our port's shared memory segment for deletion */
	shmctl(sPorts[slot].queue_shm, IPC_RMID, NULL);

	return B_OK;
}


port_id
find_port(const char* name)
{
	TRACE(("find_port(name = \"%s\")\n", name));

	if (!sPortsActive)
		port_init();
	if (!sPortsActive) {
		return B_NAME_NOT_FOUND;
	}
	if (name == NULL)
		return B_BAD_VALUE;

	port_id portFound = B_NAME_NOT_FOUND;
	int32 i;

	// Since we have to check every single port, and we don't
	// care if it goes away at any point, we're only grabbing
	// the port lock in question, not the port list lock

	// loop over list
	TRACE(("find_port(): Looking for port named \"%s\"\n", name));
	for (i = 0; i < sMaxPorts && portFound < B_OK; i++) {
		// lock every individual port before comparing
		GRAB_PORT_LOCK(sPorts[i]);

		if (sPorts[i].id >= 0 && !strcmp(name, sPorts[i].name))
				portFound = sPorts[i].id;

		RELEASE_PORT_LOCK(sPorts[i]);
	}
	
	if (portFound >= 0)
	{
		TRACE(("find_port(): Port %ld matches search\n", portFound));
	}
	else
	{
		TRACE(("find_port(): Couldn't find port named \"%s\"\n", name));
	}
	
	return portFound;
}


status_t
_get_port_info(port_id id, port_info* info, size_t size)
{
	TRACE(("get_port_info(id = %ld)\n", id));

	if (info == NULL || size != sizeof(port_info))
		return B_BAD_VALUE;
	if (!sPortsActive)
		port_init();
	if (!sPortsActive || id < 0)
		return B_BAD_PORT_ID;

	int slot = id % sMaxPorts;

	GRAB_PORT_LOCK(sPorts[slot]);

	if (sPorts[slot].id != id || sPorts[slot].capacity == 0) {
		RELEASE_PORT_LOCK(sPorts[slot]);
		TRACE(("get_port_info: invalid port_id %ld\n", id));
		return B_BAD_PORT_ID;
	}

	// fill a port_info struct with info
	fill_port_info(&sPorts[slot], info, size);

	RELEASE_PORT_LOCK(sPorts[slot]);

	return B_OK;
}


status_t
_get_next_port_info(team_id teamID, int32* _cookie, struct port_info* info,
	size_t size)
{
	TRACE(("get_next_port_info(team = %ld)\n", teamID));

	if (info == NULL || size != sizeof(port_info) || _cookie == NULL
		|| teamID < 0) {
		return B_BAD_VALUE;
	}
	if (!sPortsActive)
		port_init();
	if (!sPortsActive)
		return B_BAD_PORT_ID;

	int slot = *_cookie;
	if (slot >= sMaxPorts)
		return B_BAD_PORT_ID;

	if (teamID == B_CURRENT_TEAM)
		teamID = team_get_current_team_id();

	info->port = -1; // used as found flag

	GRAB_PORT_LIST_LOCK();

	while (slot < sMaxPorts) {
		GRAB_PORT_LOCK(sPorts[slot]);
		if (sPorts[slot].id != -1 && sPorts[slot].capacity != 0 && sPorts[slot].owner == teamID) {
			// found one!
			fill_port_info(&sPorts[slot], info, size);

			RELEASE_PORT_LOCK(sPorts[slot]);
			slot++;
			break;
		}
		RELEASE_PORT_LOCK(sPorts[slot]);
		slot++;
	}
	RELEASE_PORT_LIST_LOCK();

	if (info->port == -1)
		return B_BAD_PORT_ID;

	*_cookie = slot;
	return B_OK;
}


ssize_t
port_buffer_size(port_id id)
{
	return port_buffer_size_etc(id, 0, 0);
}


ssize_t
port_buffer_size_etc(port_id id, uint32 flags, bigtime_t timeout)
{
	port_message_info info;
	info.size = 0;
	status_t error = get_port_message_info_etc(id, &info, flags, timeout);
	return error != B_OK ? error : info.size;
}


status_t
_get_port_message_info_etc(port_id id, port_message_info* info,
	size_t infoSize, uint32 flags, bigtime_t timeout)
{
	if (info == NULL || infoSize != sizeof(port_message_info))
		return B_BAD_VALUE;

	sem_id cachedSem;
	status_t status;
	port_message *msg;
	ssize_t size;
	int32 slot;
	int tail;
	void* msg_queue;

	TRACE(("_get_port_message_info_etc(%ld): enter\n", (long)id));

	if (!sPortsActive)
		port_init();

	if (!sPortsActive || id < 0)
		return B_BAD_PORT_ID;

	slot = id % sMaxPorts;

	GRAB_PORT_LOCK(sPorts[slot]);

	if (sPorts[slot].id != id
		|| (is_port_closed(slot))) {
		RELEASE_PORT_LOCK(sPorts[slot]);
		TRACE(("port_buffer_size_etc(): %s port %ld\n",
			sPorts[slot].id == id ? "closed" : "invalid", id));
		return B_BAD_PORT_ID;
	}

	cachedSem = sPorts[slot].read_sem;

	RELEASE_PORT_LOCK(sPorts[slot]);

	// block if no message, or, if B_TIMEOUT flag set, block with timeout

	status = acquire_sem_etc(cachedSem, 1, flags, timeout);

	if (status != B_OK && status != B_BAD_SEM_ID)
		return status;

	// in case of B_BAD_SEM_ID, the port might have been closed but not yet
	// deleted, ie. there could still be messages waiting for us

	GRAB_PORT_LOCK(sPorts[slot]);

	if (sPorts[slot].id != id) {
		// the port is no longer there
		RELEASE_PORT_LOCK(sPorts[slot]);
		return B_BAD_PORT_ID;
	}

	// determine tail & get the length of the message
	tail = sPorts[slot].tail;
	if (tail < 0)
		panic("port %ld: tail < 0", sPorts[slot].id);
	if (tail > sPorts[slot].original_capacity)
		panic("port %ld: tail > cap %ld", sPorts[slot].id, sPorts[slot].original_capacity);

	msg_queue = shmat(sPorts[slot].queue_shm, NULL, 0);
	if (msg_queue == (void *) -1) {
		panic("port %ld: missing queue - shmat returned %d\n", sPorts[slot].id, errno);
		return B_ERROR;
	}

	msg = msg_queue + (sizeof(port_message) * tail);
	if (msg == NULL)
		panic("port %ld: no messages found\n", sPorts[slot].id);

	size = msg->size;

	shmdt(msg_queue);

	RELEASE_PORT_LOCK(sPorts[slot]);

	// restore read_sem, as we haven't read from the port
	release_sem(cachedSem);

	// return length of item at end of queue
	return size;
}


ssize_t
port_count(port_id id)
{
	if (!sPortsActive == false)
		port_init();
	if (!sPortsActive || id < 0)
		return B_BAD_PORT_ID;

	int32 slot = id % sMaxPorts;

	GRAB_PORT_LOCK(sPorts[slot]);

	if (sPorts[slot].id != id) {
		RELEASE_PORT_LOCK(sPorts[slot]);
		TRACE(("port_count: invalid port_id %ld\n", id));
		return B_BAD_PORT_ID;
	}

	int32 count = 0;
	get_sem_count(sPorts[slot].read_sem, &count);
	// do not return negative numbers 
	if (count < 0)
		count = 0;

	RELEASE_PORT_LOCK(sPorts[slot]);

	// return count of messages
	return count;
}


ssize_t
read_port(port_id port, int32* msgCode, void* buffer, size_t bufferSize)
{
	return read_port_etc(port, msgCode, buffer, bufferSize, 0, 0);
}


ssize_t
read_port_etc(port_id id, int32* _code, void* buffer, size_t bufferSize,
	uint32 flags, bigtime_t timeout)
{
	sem_id cachedSem;
	status_t status;
	port_message *msg;
	size_t size;
	int slot;
	int tail;
	void* msg_queue;

	if (!sPortsActive)
		port_init();

	if (!sPortsActive || id < 0)
		return B_BAD_PORT_ID;
	if ((buffer == NULL && bufferSize > 0) || timeout < 0)
		return B_BAD_VALUE;

	flags &= B_CAN_INTERRUPT | B_KILL_CAN_INTERRUPT | B_RELATIVE_TIMEOUT
		| B_ABSOLUTE_TIMEOUT;

	slot = id % sMaxPorts;

	GRAB_PORT_LOCK(sPorts[slot]);

	if (sPorts[slot].id != id
		|| (is_port_closed(slot))) {
		RELEASE_PORT_LOCK(sPorts[slot]);
		TRACE(("read_port_etc(): %s port %ld\n",
			sPorts[slot].id == id ? "closed" : "invalid", id));
		return B_BAD_PORT_ID;
	}
	// store sem_id in local variable
	cachedSem = sPorts[slot].read_sem;

	// unlock port && enable ints/
	RELEASE_PORT_LOCK(sPorts[slot]);
	TRACE(("read_port_etc: about to acquire read sem\n"));

	status = acquire_sem_etc(cachedSem, 1, flags, timeout);
		// get 1 entry from the queue, block if needed

	if (status != B_OK && status != B_BAD_SEM_ID)
		return status;

	// in case of B_BAD_SEM_ID, the port might have been closed but not yet
	// deleted, ie. there could still be messages waiting for us

	GRAB_PORT_LOCK(sPorts[slot]);

	// first, let's check if the port is still alive
	if (sPorts[slot].id == -1) {
		// the port has been deleted in the meantime
		RELEASE_PORT_LOCK(sPorts[slot]);
		return B_BAD_PORT_ID;
	}

	tail = sPorts[slot].tail;
	if (tail < 0)
		panic("port %ld: tail < 0", sPorts[slot].id);
	if (tail > sPorts[slot].original_capacity)
		panic("port %ld: tail > cap %ld", sPorts[slot].id, sPorts[slot].original_capacity);

	sPorts[slot].tail = (sPorts[slot].tail + 1) % sPorts[slot].original_capacity;

	msg_queue = shmat(sPorts[slot].queue_shm, NULL, 0);
	if (msg_queue == (void *) -1) {
		panic("port %ld: missing queue - shmat returned %d\n", sPorts[slot].id, errno);
		return B_ERROR;
	}

	msg = msg_queue + (sizeof(port_message) * tail);
	if (msg == NULL)
		panic("port %ld: no messages found", sPorts[slot].id);

	sPorts[slot].total_count++;

	cachedSem = sPorts[slot].write_sem;

	RELEASE_PORT_LOCK(sPorts[slot]);

	// check output buffer size
	size = min_c(bufferSize, msg->size);

	// copy message
	if (_code != NULL)
		*_code = msg->code;
	if (size > 0) {
		if (buffer)
			memcpy(buffer, msg->buffer_chain, size);
	}
	put_port_msg(msg);

	shmdt(msg_queue);

	// make one spot in queue available again for write
	release_sem(cachedSem);
		// ToDo: we might think about setting B_NO_RESCHEDULE here
		//	from time to time (always?)

	TRACE(("read_port_etc(): read %ld bytes from port %ld queue position %d.\n", (long)size, id, tail));
	return size;
}


status_t
write_port(port_id id, int32 msgCode, const void* buffer, size_t bufferSize)
{
	return write_port_etc(id, msgCode, buffer, bufferSize, 0, 0);
}


status_t
write_port_etc(port_id id, int32 msgCode, const void* buffer,
	size_t bufferSize, uint32 flags, bigtime_t timeout)
{
	sem_id cachedSem;
	int head;
	int slot;
	void* msg_queue;

	if (!sPortsActive)
		port_init();

	if (!sPortsActive || id < 0)
		return B_BAD_PORT_ID;
	if (bufferSize > PORT_MAX_MESSAGE_SIZE)
		return B_BAD_VALUE;

	// mask irrelevant flags (for acquire_sem() usage)
	flags &= B_CAN_INTERRUPT | B_KILL_CAN_INTERRUPT | B_RELATIVE_TIMEOUT
		| B_ABSOLUTE_TIMEOUT;
	slot = id % sMaxPorts;
	status_t status;
	port_message* message;


	GRAB_PORT_LOCK(sPorts[slot]);

	if (sPorts[slot].id != id) {
		RELEASE_PORT_LOCK(sPorts[slot]);
		TRACE(("write_port_etc: invalid port_id %ld\n", id));
		return B_BAD_PORT_ID;
	}

	if (is_port_closed(slot)) {
		RELEASE_PORT_LOCK(sPorts[slot]);
		TRACE(("write_port_etc: port %ld closed\n", id));
		return B_BAD_PORT_ID;
	}

	// store sem_id in local variable 
	cachedSem = sPorts[slot].write_sem;

	RELEASE_PORT_LOCK(sPorts[slot]);

	status = acquire_sem_etc(cachedSem, 1, flags, timeout);
		// get 1 entry from the queue, block if needed

	if (status == B_BAD_SEM_ID) {
		// somebody deleted or closed the port
		return B_BAD_PORT_ID;
	}

	if (status != B_OK)
		return status;

	if (status != B_NO_ERROR) {
		TRACE(("write_port_etc: unknown error %ld\n", status));
		return status;
	}

	// Find and sanity-check the head of the queue
	head = sPorts[slot].head;
	if (head < 0)
		panic("port %ld: head < 0", sPorts[slot].id);
	if (head >= sPorts[slot].capacity)
		panic("port %ld: head > cap %ld", sPorts[slot].id, sPorts[slot].capacity);

	msg_queue = shmat(sPorts[slot].queue_shm, NULL, 0);
	if (msg_queue == (void *) -1)
		panic("port %ld: missing queue", sPorts[slot].id);

	message = msg_queue + (sizeof(port_message) * head);

	message->code = msgCode;
	message->size = bufferSize;
	memcpy(message->buffer_chain, buffer, bufferSize);
	sPorts[slot].head = (sPorts[slot].head + 1) % sPorts[slot].capacity;

	shmdt(msg_queue);

	// attach message to queue
	GRAB_PORT_LOCK(sPorts[slot]);

	// first, let's check if the port is still alive
	if (sPorts[slot].id == -1) {
		// the port has been deleted in the meantime
		RELEASE_PORT_LOCK(sPorts[slot]);

		//put_port_msg(message);
		return B_BAD_PORT_ID;
	}

	// list_add_item not necessary, already done in Cosmoe

	// store sem_id in local variable 
	cachedSem = sPorts[slot].read_sem;

	RELEASE_PORT_LOCK(sPorts[slot]);

	// release sem, allowing read (might reschedule)
	release_sem(cachedSem);

	TRACE(("write_port_etc(): wrote %ld bytes to port %d queue position %d.\n", (long)bufferSize, slot, head));
	return B_NO_ERROR;
}


status_t
set_port_owner(port_id id, team_id newTeamID)
{
	TRACE(("set_port_owner(id = %ld, team = %ld)\n", id, newTeamID));

	if (!sPortsActive)
		port_init();

	if (!sPortsActive || id < 0)
		return B_BAD_PORT_ID;

	int slot = id % sMaxPorts;

	GRAB_PORT_LOCK(sPorts[slot]);

	if (sPorts[slot].id != id) {
		RELEASE_PORT_LOCK(sPorts[slot]);
		TRACE(("set_port_owner: invalid port_id %ld\n", id));
		return B_BAD_PORT_ID;
	}

	// transfer ownership to other team
	sPorts[slot].owner = newTeamID;

	// unlock port
	RELEASE_PORT_LOCK(sPorts[slot]);

	return B_OK;
}

//	#pragma mark -
/* 
 *	private functions
 */

void teardown_ports(void)
{
	if (!sPorts)
	{
		printf("teardown_ports(): no ports to delete\n");
		return;
	}

	/* remove all sems owned by our team */
	int num_deleted = delete_owned_ports(getpid());

	printf("teardown_ports(): %d ports deleted\n", num_deleted);
}

