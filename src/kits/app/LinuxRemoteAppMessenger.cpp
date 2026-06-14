#include <LinuxRemoteAppMessenger.h>

#include <Application.h>
#include <DataIO.h>
#include <Message.h>
#include <Roster.h>

#include <AppMisc.h>
#include <MessagePrivate.h>
#include <MessengerPrivate.h>

#ifdef __linux__

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>


namespace {

static const char* kSocketPrefix = "cosmoe:BMessenger:";
static const char* kTemporaryReplySignature
	= "application/x-vnd.cosmoe-remote-reply";
static const uint32 kMessageMagic = 0x43424d53; // CBMS
static const uint32 kMaxRemoteMessageSize = 64 * 1024 * 1024;

struct RemoteMessageHeader {
	uint32	magic;
	uint32	size;
	port_id	port;
};

struct RemoteAppEndpoint {
	char	signature[B_MIME_TYPE_LENGTH];
	team_id	team;
	port_id	port;
	char	socketName[sizeof(sockaddr_un::sun_path)];
};

pthread_mutex_t sRemoteAppLock = PTHREAD_MUTEX_INITIALIZER;
int sListenSocket = -1;
pthread_t sListenThread;
bool sListenThreadStarted = false;


bool
debug_remote_messaging()
{
	const char* debug = getenv("COSMOE_BMESSENGER_DEBUG");
	return debug != NULL && debug[0] != '\0' && strcmp(debug, "0") != 0;
}


void
debug_log(const char* format, ...)
{
	if (!debug_remote_messaging())
		return;

	fprintf(stderr, "BMessengerRemote[%d]: ", (int)getpid());
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fprintf(stderr, "\n");
}


status_t
build_socket_name(const char* signature, team_id team, port_id port,
	char* name, size_t nameSize)
{
	if (signature == NULL || signature[0] == '\0' || name == NULL
		|| nameSize == 0) {
		return B_BAD_VALUE;
	}

	int written = snprintf(name, nameSize, "%s%s:%d:%d", kSocketPrefix,
		signature, (int)team, (int)port);
	if (written < 0 || (size_t)written >= nameSize)
		return B_NAME_TOO_LONG;

	return B_OK;
}


status_t
fill_abstract_address(const char* name, sockaddr_un* address,
	socklen_t* _length)
{
	if (name == NULL || address == NULL || _length == NULL)
		return B_BAD_VALUE;

	size_t nameLength = strlen(name);
	if (nameLength + 1 > sizeof(address->sun_path))
		return B_NAME_TOO_LONG;

	memset(address, 0, sizeof(*address));
	address->sun_family = AF_UNIX;
	address->sun_path[0] = '\0';
	memcpy(address->sun_path + 1, name, nameLength);
	*_length = offsetof(sockaddr_un, sun_path) + 1 + nameLength;
	return B_OK;
}


bool
parse_socket_name(const char* name, RemoteAppEndpoint* endpoint)
{
	if (name == NULL || endpoint == NULL)
		return false;

	size_t prefixLength = strlen(kSocketPrefix);
	if (strncmp(name, kSocketPrefix, prefixLength) != 0)
		return false;

	const char* body = name + prefixLength;
	const char* portSeparator = strrchr(body, ':');
	if (portSeparator == NULL)
		return false;

	const char* teamSeparator = portSeparator;
	while (teamSeparator > body) {
		teamSeparator--;
		if (*teamSeparator == ':')
			break;
	}
	if (*teamSeparator != ':' || teamSeparator == body)
		return false;

	size_t signatureLength = teamSeparator - body;
	if (signatureLength == 0 || signatureLength >= B_MIME_TYPE_LENGTH)
		return false;

	char* end = NULL;
	long team = strtol(teamSeparator + 1, &end, 10);
	if (end != portSeparator)
		return false;

	long port = strtol(portSeparator + 1, &end, 10);
	if (end == portSeparator + 1 || *end != '\0')
		return false;

	memset(endpoint, 0, sizeof(*endpoint));
	memcpy(endpoint->signature, body, signatureLength);
	endpoint->signature[signatureLength] = '\0';
	endpoint->team = (team_id)team;
	endpoint->port = (port_id)port;
	strlcpy(endpoint->socketName, name, sizeof(endpoint->socketName));
	return true;
}


status_t
find_remote_endpoint(const char* signature, team_id team,
	RemoteAppEndpoint* endpoint)
{
	if (endpoint == NULL)
		return B_BAD_VALUE;
	if ((signature == NULL || signature[0] == '\0') && team < 0)
		return B_BAD_VALUE;

	FILE* file = fopen("/proc/net/unix", "r");
	if (file == NULL) {
		debug_log("lookup signature=%s team=%d failed opening /proc/net/unix: %s",
			signature != NULL ? signature : "(null)", (int)team,
			strerror(errno));
		return B_ENTRY_NOT_FOUND;
	}

	status_t result = B_ENTRY_NOT_FOUND;
	char line[1024];
	while (fgets(line, sizeof(line), file) != NULL) {
		char* marker = strchr(line, '@');
		if (marker == NULL)
			continue;

		char* end = marker;
		while (*end != '\0' && *end != '\n' && *end != '\r'
			&& *end != ' ' && *end != '\t') {
			end++;
		}
		*end = '\0';

		RemoteAppEndpoint candidate;
		if (!parse_socket_name(marker + 1, &candidate))
			continue;

		if (signature != NULL && signature[0] != '\0'
			&& strcasecmp(signature, candidate.signature) != 0) {
			continue;
		}
		if (team >= 0 && team != candidate.team)
			continue;

		*endpoint = candidate;
		debug_log("lookup signature=%s team=%d matched socket=%s port=%d",
			signature != NULL ? signature : "(null)", (int)team,
			candidate.socketName, (int)candidate.port);
		result = B_OK;
		break;
	}

	fclose(file);
	if (result != B_OK) {
		debug_log("lookup signature=%s team=%d found no socket",
			signature != NULL ? signature : "(null)", (int)team);
	}
	return result;
}


status_t
read_exact(int fd, void* buffer, size_t size)
{
	char* bytes = (char*)buffer;
	while (size > 0) {
		ssize_t bytesRead = read(fd, bytes, size);
		if (bytesRead == 0)
			return B_ERROR;
		if (bytesRead < 0) {
			if (errno == EINTR)
				continue;
			return B_ERROR;
		}

		bytes += bytesRead;
		size -= bytesRead;
	}

	return B_OK;
}


status_t
write_exact(int fd, const void* buffer, size_t size)
{
	const char* bytes = (const char*)buffer;
	while (size > 0) {
		ssize_t bytesWritten = write(fd, bytes, size);
		if (bytesWritten < 0) {
			if (errno == EINTR)
				continue;
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				return B_TIMED_OUT;
			return B_ERROR;
		}

		bytes += bytesWritten;
		size -= bytesWritten;
	}

	return B_OK;
}


void
set_socket_timeout(int fd, bigtime_t timeout)
{
	if (timeout == B_INFINITE_TIMEOUT || timeout < 0)
		return;

	timeval tv;
	tv.tv_sec = timeout / 1000000;
	tv.tv_usec = timeout % 1000000;
	setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}


void
handle_remote_connection(int fd)
{
	RemoteMessageHeader header;
	if (read_exact(fd, &header, sizeof(header)) != B_OK)
		return;
	if (header.magic != kMessageMagic || header.size > kMaxRemoteMessageSize) {
		debug_log("receive rejected magic=0x%08x size=%u port=%d",
			header.magic, header.size, (int)header.port);
		return;
	}

	char* buffer = (char*)malloc(header.size);
	if (buffer == NULL) {
		debug_log("receive failed allocating %u bytes for port=%d",
			header.size, (int)header.port);
		return;
	}

	status_t result = read_exact(fd, buffer, header.size);
	if (result == B_OK) {
		do {
			result = write_port_etc(header.port, kPortMessageCode, buffer,
				header.size, B_RELATIVE_TIMEOUT, B_INFINITE_TIMEOUT);
		} while (result == B_INTERRUPTED);
	}
	debug_log("receive inject port=%d size=%u result=%d", (int)header.port,
		header.size, (int)result);

	free(buffer);
}


void*
remote_app_listener(void*)
{
	while (true) {
		pthread_mutex_lock(&sRemoteAppLock);
		int listenSocket = sListenSocket;
		pthread_mutex_unlock(&sRemoteAppLock);

		if (listenSocket < 0)
			break;

		int fd = accept(listenSocket, NULL, NULL);
		if (fd < 0) {
			if (errno == EINTR)
				continue;

			pthread_mutex_lock(&sRemoteAppLock);
			bool stopped = sListenSocket < 0;
			pthread_mutex_unlock(&sRemoteAppLock);
			if (stopped)
				break;
			continue;
		}

		handle_remote_connection(fd);
		close(fd);
	}

	return NULL;
}


status_t
send_flattened_remote_message(const RemoteAppEndpoint& endpoint,
	port_id destinationPort, int32 token, BMessage* message,
	BMessenger replyTo, bigtime_t timeout, bool replyRequired)
{
	if (message == NULL)
		return B_BAD_VALUE;

	BMessage outbound(*message);
	BMessage::Private outboundPrivate(outbound);
	outboundPrivate.SetTarget(token);

	if (!replyTo.IsValid())
		replyTo = be_app_messenger;

	BMessenger::Private replyPrivate(replyTo);
	outboundPrivate.SetReply(replyPrivate.Team(), replyPrivate.Port(),
		replyPrivate.Token());

	BMessage::message_header* messageHeader
		= outboundPrivate.GetMessageHeader();
	messageHeader->flags |= MESSAGE_FLAG_WAS_DELIVERED;
	if (replyRequired) {
		messageHeader->flags |= MESSAGE_FLAG_REPLY_REQUIRED;
		messageHeader->flags &= ~MESSAGE_FLAG_REPLY_DONE;
	}

	ssize_t flattenedSize = outbound.FlattenedSize();
	if (flattenedSize < 0 || flattenedSize > (ssize_t)kMaxRemoteMessageSize)
		return B_BAD_VALUE;

	char* buffer = (char*)malloc(flattenedSize);
	if (buffer == NULL)
		return B_NO_MEMORY;

	status_t result = outbound.Flatten(buffer, flattenedSize);
	if (result != B_OK) {
		if (replyRequired)
			messageHeader->flags |= MESSAGE_FLAG_REPLY_DONE;
		free(buffer);
		return result;
	}

	if (replyRequired)
		messageHeader->flags |= MESSAGE_FLAG_REPLY_DONE;

	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0) {
		free(buffer);
		return B_ERROR;
	}

	set_socket_timeout(fd, timeout);

	sockaddr_un address;
	socklen_t addressLength;
	result = fill_abstract_address(endpoint.socketName, &address,
		&addressLength);
	if (result == B_OK && connect(fd, (sockaddr*)&address, addressLength) < 0)
		result = errno == EAGAIN || errno == EWOULDBLOCK ? B_TIMED_OUT : B_ERROR;

	RemoteMessageHeader header;
	header.magic = kMessageMagic;
	header.size = flattenedSize;
	header.port = destinationPort;

	if (result == B_OK)
		result = write_exact(fd, &header, sizeof(header));
	if (result == B_OK)
		result = write_exact(fd, buffer, flattenedSize);

	close(fd);
	free(buffer);
	return result;
}


} // namespace


namespace BPrivate {

status_t
RegisterRemoteAppMessenger(const char* signature, port_id port)
{
	if (signature == NULL || signature[0] == '\0' || port < 0)
		return B_BAD_VALUE;

	team_id team = current_team();
	char socketName[sizeof(sockaddr_un::sun_path)];
	status_t result = build_socket_name(signature, team, port, socketName,
		sizeof(socketName));
	if (result != B_OK) {
		debug_log("register signature=%s team=%d port=%d name failed: %d",
			signature, (int)team, (int)port, (int)result);
		return result;
	}

	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0) {
		debug_log("register socket=%s failed creating socket: %s",
			socketName, strerror(errno));
		return B_ERROR;
	}

	sockaddr_un address;
	socklen_t addressLength;
	result = fill_abstract_address(socketName, &address, &addressLength);
	if (result != B_OK) {
		close(fd);
		return result;
	}

	if (bind(fd, (sockaddr*)&address, addressLength) < 0) {
		debug_log("register socket=%s bind failed: %s", socketName,
			strerror(errno));
		close(fd);
		return B_ERROR;
	}

	if (listen(fd, 16) < 0) {
		debug_log("register socket=%s listen failed: %s", socketName,
			strerror(errno));
		close(fd);
		return B_ERROR;
	}

	pthread_mutex_lock(&sRemoteAppLock);
	if (sListenSocket >= 0)
		close(sListenSocket);
	sListenSocket = fd;
	sListenThreadStarted = pthread_create(&sListenThread, NULL,
		remote_app_listener, NULL) == 0;
	if (sListenThreadStarted)
		pthread_detach(sListenThread);
	else {
		close(sListenSocket);
		sListenSocket = -1;
		result = B_ERROR;
	}
	pthread_mutex_unlock(&sRemoteAppLock);

	debug_log("register socket=%s team=%d port=%d result=%d", socketName,
		(int)team, (int)port, (int)result);
	return result;
}


void
UnregisterRemoteAppMessenger()
{
	pthread_mutex_lock(&sRemoteAppLock);
	int fd = sListenSocket;
	sListenSocket = -1;
	pthread_mutex_unlock(&sRemoteAppLock);

	if (fd >= 0)
		close(fd);
	debug_log("unregister");
}


status_t
FindRemoteAppMessenger(const char* signature, team_id team, app_info* info)
{
	if (info == NULL)
		return B_BAD_VALUE;

	RemoteAppEndpoint endpoint;
	status_t result = find_remote_endpoint(signature, team, &endpoint);
	if (result != B_OK)
		return result;

	*info = app_info();
	info->thread = -1;
	info->team = endpoint.team;
	info->port = endpoint.port;
	info->flags = 0;
	strlcpy(info->signature, endpoint.signature, sizeof(info->signature));
	debug_log("messenger init signature=%s team=%d port=%d",
		info->signature, (int)info->team, (int)info->port);
	return B_OK;
}


bool
SendRemoteAppMessage(port_id port, team_id team, int32 token,
	BMessage* message, BMessenger replyTo, bigtime_t timeout,
	status_t* _result)
{
	if (team == current_team())
		return false;

	RemoteAppEndpoint endpoint;
	status_t result = find_remote_endpoint(NULL, team, &endpoint);
	if (result != B_OK) {
		debug_log("send team=%d port=%d token=%d no endpoint result=%d",
			(int)team, (int)port, (int)token, (int)result);
		return false;
	}

	result = send_flattened_remote_message(endpoint, port, token, message, replyTo,
		timeout, false);
	debug_log("send team=%d port=%d token=%d socket=%s result=%d",
		(int)team, (int)port, (int)token, endpoint.socketName, (int)result);
	if (_result != NULL)
		*_result = result;
	return true;
}


bool
SendRemoteAppMessage(port_id port, team_id team, int32 token,
	BMessage* message, BMessage* reply, bigtime_t deliveryTimeout,
	bigtime_t replyTimeout, status_t* _result)
{
	if (team == current_team())
		return false;

	RemoteAppEndpoint endpoint;
	status_t result = find_remote_endpoint(NULL, team, &endpoint);
	if (result != B_OK)
		return false;

	port_id replyPort = create_port(1, "remote_reply_port");
	if (replyPort < B_OK) {
		if (_result != NULL)
			*_result = replyPort;
		return true;
	}

	BMessenger replyTarget;
	BMessenger::Private(replyTarget).SetTo(current_team(), replyPort,
		B_PREFERRED_TOKEN);

	RemoteAppEndpoint replyEndpoint;
	bool temporaryReplyEndpoint = find_remote_endpoint(NULL, current_team(),
		&replyEndpoint) != B_OK;
	if (temporaryReplyEndpoint) {
		result = RegisterRemoteAppMessenger(kTemporaryReplySignature, replyPort);
		if (result != B_OK) {
			delete_port(replyPort);
			if (_result != NULL)
				*_result = result;
			return true;
		}
	}

	result = send_flattened_remote_message(endpoint, port, token, message,
		replyTarget, deliveryTimeout, true);
	if (result == B_OK) {
		ssize_t size;
		do {
			size = port_buffer_size_etc(replyPort, B_RELATIVE_TIMEOUT,
				replyTimeout);
		} while (size == B_INTERRUPTED);

		if (size < B_OK)
			result = size;
		else {
			char* buffer = (char*)malloc(size);
			if (buffer == NULL)
				result = B_NO_MEMORY;
			else {
				int32 code;
				ssize_t bytesRead = read_port_etc(replyPort, &code, buffer,
					size, B_RELATIVE_TIMEOUT, 0);
				if (bytesRead < B_OK)
					result = bytesRead;
				else if (reply != NULL)
					result = reply->Unflatten((const char*)buffer);
				free(buffer);
			}
		}
	}

	if (temporaryReplyEndpoint)
		UnregisterRemoteAppMessenger();

	delete_port(replyPort);
	if (_result != NULL)
		*_result = result;
	return true;
}

} // namespace BPrivate

#else

namespace BPrivate {

status_t
RegisterRemoteAppMessenger(const char*, port_id)
{
	return B_NOT_SUPPORTED;
}


void
UnregisterRemoteAppMessenger()
{
}


status_t
FindRemoteAppMessenger(const char*, team_id, app_info*)
{
	return B_NOT_SUPPORTED;
}


bool
SendRemoteAppMessage(port_id, team_id, int32, BMessage*, BMessenger,
	bigtime_t, status_t*)
{
	return false;
}


bool
SendRemoteAppMessage(port_id, team_id, int32, BMessage*, BMessage*,
	bigtime_t, bigtime_t, status_t*)
{
	return false;
}

} // namespace BPrivate

#endif
