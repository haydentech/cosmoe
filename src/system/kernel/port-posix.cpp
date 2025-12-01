/*
** Copyright 2024, Bill Hayden. All rights reserved.
 */


/*!	Ports for IPC */


#include <unistd.h>
#include <sys/types.h>
#include <mqueue.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
//#include <glob.h>
#include <sys/resource.h>

#include <string>
#include <map>
#include <algorithm>

using namespace std;

#include <OS.h>
#include <sys/time.h>

#define panic printf

#define TRACE_PORTS
#ifdef TRACE_PORTS
#	define TRACE(x) printf x
#else
#	define TRACE(x)
#endif

#define DEBUG

#define PORT_MAX_MESSAGE_SIZE 65536

#define COSMOE_RESEND_HACK_FLAG 0x40

typedef struct port_msg {
	size_t		size;
	int32		code;
	char		message[1];
} port_msg;

// struct port_entry {
// 	port_id				id;
// 	team_id				owner;
// 	int32		 		capacity;
// 	int32				original_capacity;
// 	sem_id				lock;
// 	char				name[B_OS_NAME_LENGTH];
// 	sem_id				read_sem;
// 	sem_id				write_sem;
// 	int32				total_count;
// 		// messages read from port since creation
// 	int				queue_shm;
// 	int32				head;
// 	int32				tail;
// };

// hidden API
static int dump_port_list(int argc, char** argv);
//static void _dump_port_info(struct port_entry *port);


#define MAX_QUEUE_LENGTH 300

static bool sPortsActive = false;

static map<port_id, string> portMap;

static status_t port_init(void);
static void teardown_ports(void);
static int delete_owned_ports(team_id owner);

const char* portFilenameBase = "/Cosmoe|";
const char* portFilenameSearchBase = "/Cosmoe|*";

//	#pragma mark -


static int
dump_port_list(int argc, char** argv)
{
	const char* name = NULL;
	team_id owner = -1;
	//int32 i;

	if (argc > 2) {
		if (!strcmp(argv[1], "team") || !strcmp(argv[1], "owner"))
			owner = strtoul(argv[2], NULL, 0);
		else if (!strcmp(argv[1], "name"))
			name = argv[2];
	} else if (argc > 1)
		owner = strtoul(argv[1], NULL, 0);
	printf("port             id  cap  r-sem  r-cnt  w-sem  w-cnt    total   team  name\n");

	// FIXME iterate /dev/mqueue
	// 	if (port->id < 0
	// 		|| (owner != -1 && port->owner != owner)
	// 		|| (name != NULL && strstr(port->name, name) == NULL))
	// 		continue;

	// 	int32 readCount, writeCount;
	// 	get_sem_count(port->read_sem, &readCount);
	// 	get_sem_count(port->write_sem, &writeCount);
	// 	kprintf("%p %8ld %4ld %6ld %6ld %6ld %6ld %8ld %6ld  %s\n", port,
	// 		port->id, port->capacity, port->read_sem, readCount,
	// 		port->write_sem, writeCount, port->total_count, port->owner,
	// 		port->name);
	// }

	return 0;
}


// static void
// _dump_port_info(struct port_entry* port)
// {
// 	int32 count;

// 	printf("PORT: %p\n", port);
// 	printf(" id:              %ld\n", port->id);
// 	printf(" name:            \"%s\"\n", port->name);
// 	printf(" owner:           %ld\n", port->owner);
// 	printf(" capacity:        %ld\n", port->capacity);
// 	printf(" read_sem:        %ld\n", port->read_sem);
// 	printf(" write_sem:       %ld\n", port->write_sem);
//  	get_sem_count(port->read_sem, &count);
//  	printf(" read_sem count:  %ld\n", count);
//  	get_sem_count(port->write_sem, &count);
// 	printf(" write_sem count: %ld\n", count);
// 	printf(" total count:     %ld\n", port->total_count);
// }


static int
dump_port_info(int argc, char** argv)
{
	const char* name = NULL;
	sem_id sem = -1;
	
	port_init();
	 
	if (!sPortsActive)
	{
		printf("No Cosmoe ports in use.\n");
		return 0;
	}

	if (argc < 2) {
		dump_port_list(argc, argv);
		return 0;
	}

	if (argc > 2) {
		if (!strcmp(argv[1], "address")) {
			//_dump_port_info((struct port_entry *)strtoul(argv[2], NULL, 0));
			return 0;
		} else if (!strcmp(argv[1], "sem"))
			sem = strtoul(argv[2], NULL, 0);
		else if (!strcmp(argv[1], "name"))
			name = argv[2];
	} else if (isdigit(argv[1][0])) {
		// if the argument looks like a number, treat it as such
		// uint32 num = strtoul(argv[1], NULL, 0);
		// uint32 slot = num % sMaxPorts;
		// if (sPorts[slot].id != (int)num) {
		// 	printf("port %ld (%#lx) doesn't exist!\n", num, num);
		// 	return 0;
		// }
		// _dump_port_info(&sPorts[slot]);
		return 0;
	} else
		name = argv[1];

	// walk through the ports list, trying to match name
	// for (int32 i = 0; i < sMaxPorts; i++) {
	// 	if ((name != NULL && sPorts[i].name != NULL
	// 			&& !strcmp(name, sPorts[i].name))
	// 		|| (sem != -1 && (sPorts[i].read_sem == sem
	// 			|| sPorts[i].write_sem == sem))) {
	// 		_dump_port_info(&sPorts[i]);
	// 		return 0;
	// 	}
	// }

	return 0;
}


// This is for ports created by this process only
bool portid_to_filename(port_id id, char* filename, bool includeBase)
{
	// for (const auto& kv : portMap) {
	// 	printf("#### port %ld is '%s'\n", kv.first, kv.second);
	// }

	auto result = std::find_if(
          portMap.begin(),
          portMap.end(),
          [id](const auto& mo) {return mo.first == id; });

	if (result != portMap.end()) {
		string file(includeBase ? portFilenameBase : "");
		file.append(result->second);
		strncpy(filename, file.c_str(), B_OS_NAME_LENGTH);
		printf("found it!\n");
		return true;
	}

	printf("didn't find it!\n");

	return false;


	// glob_t glob_results;
	// std::string portbase;

	// portbase += "/Cosmoe|" + std::to_string(id) + "|*";

	// if (glob(portbase.c_str(), GLOB_NOCHECK, 0, &glob_results) != 0)
	// 	return false;

	// int results = glob_results.gl_pathc;
	// if (results > 0)
	// 	strncpy(filename, glob_results.gl_pathv[0], B_OS_NAME_LENGTH);

	// globfree(&glob_results);
	// return (results > 0);
}


void construct_posix_timespec(struct timespec* ts, uint32 flags, bigtime_t timeout)
{
	if (timeout == B_INFINITE_TIMEOUT) {
		// Set to maximum timespec without overflow
		ts->tv_sec = 0x7FFFFFFF;
		ts->tv_nsec = 999999999L;
		return;
	}

	// POSIX wants an absolute timeout value for pselect/mq_timedreceive/mq_timedsend
	if (flags & B_RELATIVE_TIMEOUT)
	{
		// Convert relative timeout to absolute time
		struct timeval now;
		gettimeofday(&now, NULL);
		
		// Add relative timeout (in microseconds) to current time
		int64 total_usec = (now.tv_sec * 1000000LL) + now.tv_usec + timeout;
		ts->tv_sec = total_usec / 1000000LL;
		ts->tv_nsec = (total_usec % 1000000LL) * 1000L;
	}
	else /* B_ABSOLUTE_TIMEOUT */
	{
		// Convert absolute BeOS time (microseconds since boot) to timespec
		ts->tv_sec = timeout / 1000000LL;
		ts->tv_nsec = (timeout % 1000000LL) * 1000L;
	}

	while (ts->tv_nsec >= 1000000000L)
	{
		ts->tv_sec++;
		ts->tv_nsec -= 1000000000L;
	}

	//printf("timespec: %ld s, %ld ns\n", ts->tv_sec, ts->tv_nsec);
}


/*!	You need to own the port's lock when calling this function */
static bool
is_port_closed(int32 id)
{
	struct mq_attr attr;
	return (mq_getattr(id, &attr) < 0);
}


/*!	Fills the port_info structure with information from the specified
	port.
	The port lock must be held when called.
*/
static void
fill_port_info(port_id id, port_info *info, size_t size)
{
	struct mq_attr attr;
	if (mq_getattr(id, &attr) < 0)
		return;

	info->port = id;
	info->team = 0;
	info->capacity = attr.mq_maxmsg;
	info->queue_count = attr.mq_curmsgs;
	info->total_count = 0;

	if (portMap.find(id) != portMap.end()) {
		strlcpy(info->name, portMap[id].c_str(), B_OS_NAME_LENGTH - 1);
	}
}


//	#pragma mark - private kernel API


/*! This function delets all the ports that are owned by the passed team.
*/
int
delete_owned_ports(team_id owner)
{
	int count = 0;

	TRACE(("delete_owned_ports(owner = %ld)\n", owner));

	if (!sPortsActive)
		return B_BAD_PORT_ID;

	for (const auto& kv : portMap) {
    	delete_port(kv.first);
	}

	return count;
}


status_t
port_init(void)
{
	if (sPortsActive)
		return B_OK;

	atexit(teardown_ports);

    struct rlimit rlim;
    rlim.rlim_cur = RLIM_INFINITY;
    rlim.rlim_max = RLIM_INFINITY;

    if (setrlimit(RLIMIT_MSGQUEUE, &rlim) == -1) {
        perror("setrlimit");
        return 1;
    }

	TRACE(("port_init: exit\n"));

	portMap = map<port_id, string>();
	sPortsActive = true;
	return B_OK;
}


//	#pragma mark - public kernel API


port_id
create_port(int32 queueLength, const char* name)
{
	TRACE(("create_port(queueLength = %ld, name = \"%s\")\n", queueLength, name));

	if (!sPortsActive)
		port_init();
	if (!sPortsActive) {
		panic("ports used too early!\n");
		return B_BAD_PORT_ID;
	}
	if (queueLength < 1 || queueLength > MAX_QUEUE_LENGTH)
		return B_BAD_VALUE;

	string filename(portFilenameBase);
	filename.append(name);

	struct mq_attr attr;
	attr.mq_maxmsg = queueLength;
	attr.mq_msgsize = PORT_MAX_MESSAGE_SIZE;

	port_id id = mq_open(filename.c_str(), O_CREAT | O_EXCL | O_RDWR, 0700, &attr);
	if (id < 0)
		return B_ERROR;

	portMap[id] = string(name);
	return id;
}

status_t
close_port(port_id id)
{
	TRACE(("close_port(id = %ld)\n", id));

	if (!sPortsActive)
		port_init();
	if (!sPortsActive || id < 0)
		return B_BAD_PORT_ID;

	struct mq_attr attr;
	attr.mq_flags = O_NONBLOCK;
	int err = mq_setattr(id, &attr, NULL);
	if (err < 0)
		printf("mq_setattr err in close_port %d\n", errno);

	err = mq_close(id);

	return (err < 0) ? B_ERROR : B_OK;
}

status_t
delete_port(port_id id)
{
	status_t err = close_port(id);
	if (err < 0)
		return err;

	if (portMap.find(id) == portMap.end())
		return B_BAD_PORT_ID;

	string filename(portFilenameBase);
	filename += portMap[id];

	err = mq_unlink(filename.c_str());

	portMap.erase(id);

	return (err < 0) ? B_ERROR : B_OK;
}


port_id
find_port(const char* name)
{
	TRACE(("find_port(name = \"%s\")\n", name));

	port_id id = B_NAME_NOT_FOUND;

	if (!sPortsActive)
		port_init();
	if (!sPortsActive) {
		panic("ports used too early!\n");
		return B_NAME_NOT_FOUND;
	}
	if (name == NULL)
		return B_BAD_VALUE;

	// Do we already have an open port by that name?
	auto result = std::find_if(
          portMap.begin(),
          portMap.end(),
          [name](const auto& mo) {return mo.second == name; });

	if (result != portMap.end())
    	return result->first;

	// We didn't find it in our port map, so open a new descriptor
	std::string filename(portFilenameBase);
	filename.append(name);

	id = mq_open(filename.c_str(), O_RDWR, 0700, NULL);
	if (id < 0) {
		if (errno == ENOENT)
			return B_NAME_NOT_FOUND;
		return B_ERROR;
	}
	
	if (id >= 0)
	{
		TRACE(("find_port(): Port %ld matches search\n", id));
	}
	else
	{
		TRACE(("find_port(): Couldn't find port named \"%s\"\n", name));
	}
	
	return id;
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

	fill_port_info(id, info, size);

	return B_OK;
}


status_t
_get_next_port_info(team_id team, int32* _cookie, struct port_info* info,
	size_t size)
{
	TRACE(("get_next_port_info(team = %ld)\n", team));

	if (info == NULL || size != sizeof(port_info) || _cookie == NULL
		|| team < B_OK)
		return B_BAD_VALUE;
	if (!sPortsActive)
		port_init();
	if (!sPortsActive)
		return B_BAD_PORT_ID;

	int32 slot = *_cookie;

	if (team == B_CURRENT_TEAM)
		team = team_get_current_team_id();

	info->port = -1; // used as found flag

	// For each port in the list...  FIXME
	//		fill_port_info(&sPorts[slot], info, size);

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
	status_t error = get_port_message_info_etc(id, &info, flags, timeout);
	return error != B_OK ? error : info.size;
}


status_t
_get_port_message_info_etc(port_id id, port_message_info* info,
	size_t infoSize, uint32 flags, bigtime_t timeout)
{
	TRACE(("_get_port_message_info_etc(%ld): enter\n", (long)id));

	if (info == NULL || infoSize != sizeof(port_message_info))
		return B_BAD_VALUE;

	// // Gross.  Since there is no "peek" with POSIX message queues,
	// // we actually read the message, then slam it back in the queue
	// // with a higher priority to ensure it gets read first.
	// int32 code;

	// struct mq_attr attr;
	// if (mq_getattr(id, &attr) < 0)
	// 	return B_ERROR;

	// void* msg = malloc(attr.mq_msgsize);

	// info->size = read_port_etc(id, &code, msg, attr.mq_msgsize, flags, timeout);

	// status_t err = write_port_etc(id, code, msg, info->size, flags | COSMOE_RESEND_HACK_FLAG, B_INFINITE_TIMEOUT);

	// free(msg);

	// return err;


	if (!sPortsActive)
		port_init();

	if (!sPortsActive || id < 0 || is_port_closed(id))
		return B_BAD_PORT_ID;

	fd_set readfs;
	FD_ZERO(&readfs);
    FD_SET(id, &readfs);

	int nfds = id + 1;
	struct timespec	ts;
	construct_posix_timespec(&ts, flags, timeout);

	int result = pselect(nfds, &readfs, NULL, NULL, &ts, NULL);

	printf("pselect returned %d, errno %d\n", result, errno);

	if (result == 0)
		return B_TIMED_OUT;
	else if (result < 0)
		return B_ERROR;

	struct mq_attr attr;
	if (mq_getattr(id, &attr) < 0)
		return B_ERROR;

	info->size = attr.mq_msgsize;

	return B_OK;
}


ssize_t
port_count(port_id id)
{
	if (!sPortsActive == false)
		port_init();
	if (!sPortsActive || id < 0)
		return B_BAD_PORT_ID;

	struct mq_attr attributes;

	int err = mq_getattr(id, &attributes);
	if (err < 0)
		return B_BAD_PORT_ID;

	return attributes.mq_curmsgs;
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
	uint priority;
	int mq_errno = 0;

	if (!sPortsActive)
		port_init();

	if (!sPortsActive || id < 0)
		return B_BAD_PORT_ID;

	if ((buffer == NULL && bufferSize > 0) || timeout < 0)
		return B_BAD_VALUE;

	struct mq_attr attr;
	if (mq_getattr(id, &attr) < 0)
		return B_ERROR;

	port_msg* msg = (port_msg*)malloc(attr.mq_msgsize);
	ssize_t bytesReceived;

	if (flags & (B_ABSOLUTE_TIMEOUT | B_RELATIVE_TIMEOUT))
	{
		struct timespec	ts;
		construct_posix_timespec(&ts, flags, timeout);
		bytesReceived = mq_timedreceive(id, (char*)msg, attr.mq_msgsize, &priority, &ts);
		mq_errno = errno;
	}
	else
	{
		// In order to avoid blocking forever, waiting on a closed port,
		// we time out once per second.  This allows the new mq call
		// to check the closed status.
		struct mq_attr oldattr;
		struct timespec	ts;
		construct_posix_timespec(&ts, B_RELATIVE_TIMEOUT, 1000000LL);

	    attr.mq_flags = O_NONBLOCK;
    	if (mq_setattr(id, &attr, &oldattr) == -1) {
        	printf("mq_setattr failure\n");
		}

		do {
			bytesReceived = mq_timedreceive(id, (char*)msg, oldattr.mq_msgsize, &priority, &ts);
		} while (bytesReceived < 0 and errno == ETIMEDOUT);

		mq_errno = errno;

    	if (mq_setattr(id, &oldattr, NULL) == -1) {
        	printf("mq_setattr failure\n");
		}
	}

	printf("bytes received: %ld, errno = %d\n", bytesReceived, mq_errno);

	if (bytesReceived >= sizeof(port_msg)) {
		// copy message
		if (_code != NULL)
			*_code = msg->code;
		if (attr.mq_msgsize > 0) {
			if (buffer)
				memcpy(buffer, msg->message, msg->size);
		}
	}

	ssize_t size = msg->size;
	free(msg);

	if (bytesReceived <= 0) {
		if (mq_errno == ETIMEDOUT)
			return B_TIMED_OUT;
		else if (mq_errno == EINTR)
			return B_INTERRUPTED;
	}

	TRACE(("read_port_etc(): read %ld bytes from port %ld.\n", (long)msg->size, id));
	return (bytesReceived > 0) ? size : B_BAD_PORT_ID;
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
	int err;

	printf("write_port_etc(%d) buffer=%ld\n", id, bufferSize);

	if (!sPortsActive)
		port_init();

	if (!sPortsActive || id < 0)
		return B_BAD_PORT_ID;

	if (bufferSize > PORT_MAX_MESSAGE_SIZE)
		return B_BAD_VALUE;

	if (is_port_closed(id)) {
		return B_BAD_PORT_ID;
	}

	size_t	totalSize = sizeof(port_msg) + bufferSize;
	port_msg* msg = (port_msg*)malloc(totalSize);
	int priority = (flags & COSMOE_RESEND_HACK_FLAG) ? 10 : 1;

	msg->code = msgCode;
	msg->size = bufferSize;
	memcpy(msg->message, buffer, bufferSize);

	if (flags & (B_ABSOLUTE_TIMEOUT | B_RELATIVE_TIMEOUT))
	{
		struct timespec	ts;
		construct_posix_timespec(&ts, flags, timeout);
		err = mq_timedsend(id, (char*)msg, totalSize, priority, &ts);
		if (err < 0 && errno == ETIMEDOUT)
			return B_TIMED_OUT;
	}
	else
	{
		// In order to avoid blocking forever, waiting on a closed port,
		// we time out once per second.  This allows the new mq call
		// to check the closed status.

		struct mq_attr attr, oldattr;
		struct timespec	ts;
		construct_posix_timespec(&ts, B_RELATIVE_TIMEOUT, 1000000LL);

	    attr.mq_flags = O_NONBLOCK;
    	if (mq_setattr(id, &attr, &oldattr) == -1) {
        	printf("mq_setattr failure\n");
		}

		do {
			err = mq_timedsend(id, (char*)msg, totalSize, priority, &ts);
		} while (err < 0 and errno == ETIMEDOUT);

    	if (mq_setattr(id, &oldattr, NULL) == -1) {
        	printf("mq_setattr failure\n");
		}
	}

	TRACE(("write_port_etc(): wrote %ld bytes to port %ld queue.\n", (long)bufferSize, id));
	return (err == 0) ? B_NO_ERROR : B_ERROR;
}


status_t
set_port_owner(port_id id, team_id newTeamID)
{
	return B_ERROR;
}

//	#pragma mark -
/* 
 *	private functions
 */

void teardown_ports(void)
{
	/* remove all ports owned by our team */
	int num_deleted = delete_owned_ports(getpid());

	printf("teardown_ports(): %d ports deleted\n", num_deleted);
}

