/*
 * Copyright 2001-2010 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Ingo Weinhold, bonefish@@users.sf.net
 *		Axel Dörfler, axeld@pinc-software.de
 *		Clemens Zeidler <haiku@clemens-zeidler.de>
 */


#include <AppMisc.h>
#include <Looper.h>
#include <Message.h>
#include <Messenger.h>
#include <MessengerPrivate.h>
#include <NodeMonitor.h>

#include <algorithm>
#include <new>
#include <string>
#include <unordered_map>
#include <vector>

#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <pthread.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __linux__
#	include <sys/inotify.h>
#endif

// TODO: Tests!


/*!	\brief Subscribes a target to node and/or mount watching.

	Depending on \a flags, different actions are performed. If flags is \c 0,
	mount watching is requested. \a device and \a node must be \c -1 in this
	case. Otherwise node watching is requested. \a device and \a node must
	refer to a valid node, and \a flags must note contain the flag
	\c B_WATCH_MOUNT, but at least one of the other valid flags.

	\param device The device the node resides on (node_ref::device). \c -1, if
		   only mount watching is requested.
	\param node The node ID of the node (node_ref::device). \c -1, if
		   only mount watching is requested.
	\param flags A bit mask composed of the values specified in
		   <NodeMonitor.h>.
	\param port The port of the target (a looper port).
	\param handlerToken The token of the target handler. \c -2, if the
		   preferred handler of the looper is the target.
	\return \c B_OK, if everything went fine, another error code otherwise.
*/
extern "C" status_t _kstart_watching_path_(const char* path, ino_t node,
	uint32 flags, port_id port, int32 handlerToken);

/*!	\brief Unsubscribes a target from watching a node.
	\param device The device the node resides on (node_ref::device).
	\param node The node ID of the node (node_ref::device).
	\param port The port of the target (a looper port).
	\param handlerToken The token of the target handler. \c -2, if the
		   preferred handler of the looper is the target.
	\return \c B_OK, if everything went fine, another error code otherwise.
*/
extern "C" status_t _kstop_watching_path_(const char* path, port_id port,
	int32 handlerToken);


/*!	\brief Unsubscribes a target from node and mount monitoring.
	\param port The port of the target (a looper port).
	\param handlerToken The token of the target handler. \c -2, if the
		   preferred handler of the looper is the target.
	\return \c B_OK, if everything went fine, another error code otherwise.
*/
extern "C" status_t _kstop_notifying_(port_id port, int32 handlerToken);


namespace {


struct node_monitor_target {
	port_id port;
	int32 handlerToken;
	uint32 flags;
};


#ifdef __linux__

struct node_monitor_child {
	dev_t device;
	ino_t node;
	bool isDirectory;
};

struct parent_monitor_watch;


struct node_monitor_watch {
	int wd;
	std::string path;
	std::string parentPath;
	std::string name;
	dev_t device;
	ino_t node;
	ino_t parentNode;
	bool isDirectory;
	uint32 mask;
	std::vector<node_monitor_target> targets;
	std::unordered_map<std::string, node_monitor_child> children;
	parent_monitor_watch* parentWatch;
};


struct parent_monitor_watch {
	int wd;
	std::string path;
	dev_t device;
	ino_t node;
	int refCount;
	std::unordered_map<std::string, node_monitor_watch*> namedWatches;
};


struct pending_move {
	int wd;
	std::string path;
	dev_t device;
	ino_t node;
	ino_t directory;
	bool isDirectory;
};


struct pending_self_move {
	node_monitor_watch* watch;
	std::vector<node_monitor_target> targets;
	std::string path;
	std::string parentPath;
	std::string name;
	dev_t device;
	ino_t node;
	ino_t parentNode;
	bool isDirectory;
};


static pthread_mutex_t sNodeMonitorLock = PTHREAD_MUTEX_INITIALIZER;
static int sInotifyFD = -1;
static pthread_t sNodeMonitorThread;
static bool sNodeMonitorThreadStarted = false;
static std::unordered_map<int, node_monitor_watch*> sNodeMonitorWatchesByWD;
static std::unordered_map<std::string, node_monitor_watch*> sNodeMonitorWatchesByPath;
static std::unordered_map<int, parent_monitor_watch*> sParentMonitorWatchesByWD;
static std::unordered_map<std::string, parent_monitor_watch*> sParentMonitorWatchesByPath;
static std::unordered_map<uint32_t, pending_move> sPendingMoves;
static std::unordered_map<uint32_t, pending_self_move> sPendingSelfMoves;


static status_t
to_status(int error)
{
	switch (error) {
		case 0:
			return B_OK;
		case ENOENT:
			return B_ENTRY_NOT_FOUND;
		case ENOTDIR:
			return B_NOT_A_DIRECTORY;
		case ENOMEM:
			return B_NO_MEMORY;
		case EACCES:
		case EPERM:
			return B_PERMISSION_DENIED;
		case ENAMETOOLONG:
			return B_NAME_TOO_LONG;
		case EINVAL:
			return B_BAD_VALUE;
		default:
			return B_FROM_POSIX_ERROR(error);
	}
}


static uint32
watch_mask_for_flags(uint32 flags, bool isDirectory)
{
	uint32 mask = IN_DELETE_SELF | IN_MOVE_SELF;

	if ((flags & (B_WATCH_STAT | B_WATCH_INTERIM_STAT)) != 0)
		mask |= IN_ATTRIB | IN_MODIFY | IN_CLOSE_WRITE;
	if ((flags & B_WATCH_ATTR) != 0)
		mask |= IN_ATTRIB;
	if (isDirectory && (flags & (B_WATCH_DIRECTORY | B_WATCH_CHILDREN)) != 0) {
		mask |= IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO;
		mask |= IN_ATTRIB | IN_MODIFY | IN_CLOSE_WRITE;
	}

	return mask;
}


static uint32
combined_watch_mask(const node_monitor_watch& watch)
{
	uint32 mask = 0;
	for (const node_monitor_target& target : watch.targets)
		mask |= watch_mask_for_flags(target.flags, watch.isDirectory);
	return mask;
}


static bool
target_matches(const node_monitor_target& target, port_id port, int32 token)
{
	return target.port == port && target.handlerToken == token;
}


static bool
watch_needs_name_monitor(const node_monitor_watch& watch)
{
	for (const node_monitor_target& target : watch.targets) {
		if ((target.flags & B_WATCH_NAME) != 0)
			return true;
	}

	return false;
}


static void
split_path(const char* path, std::string& parent, std::string& name)
{
	char parentBuffer[PATH_MAX];
	char nameBuffer[PATH_MAX];
	strlcpy(parentBuffer, path, sizeof(parentBuffer));
	strlcpy(nameBuffer, path, sizeof(nameBuffer));

	parent = dirname(parentBuffer);
	name = basename(nameBuffer);
}


static status_t
stat_child(const std::string& path, const char* name, node_monitor_child& child)
{
	std::string childPath = path;
	if (!childPath.empty() && childPath.back() != '/')
		childPath += '/';
	childPath += name;

	struct stat st;
	if (lstat(childPath.c_str(), &st) != 0)
		return to_status(errno);

	child.device = st.st_dev;
	child.node = st.st_ino;
	child.isDirectory = S_ISDIR(st.st_mode);
	return B_OK;
}


static std::string
child_path(const std::string& parent, const char* name)
{
	if (name != NULL && name[0] == '/')
		return std::string(name);

	std::string path = parent;
	if (!path.empty() && path.back() != '/')
		path += '/';
	path += name;
	return path;
}


static void
refresh_children(node_monitor_watch& watch)
{
	watch.children.clear();

	if (!watch.isDirectory)
		return;

	DIR* dir = opendir(watch.path.c_str());
	if (dir == NULL)
		return;

	while (struct dirent* entry = readdir(dir)) {
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
			continue;

		node_monitor_child child;
		if (stat_child(watch.path, entry->d_name, child) == B_OK)
			watch.children[entry->d_name] = child;
	}

	closedir(dir);
}


static status_t
send_node_monitor_message(const node_monitor_target& target, BMessage& message)
{
	port_info info;
	status_t status = get_port_info(target.port, &info);
	if (status != B_OK)
		return status;

	BMessenger messenger;
	BMessenger::Private(messenger).SetTo(info.team, target.port,
		target.handlerToken);
	return messenger.SendMessage(&message);
}


static void
send_entry_created(const node_monitor_target& target,
	const node_monitor_watch& watch, const char* name,
	const node_monitor_child& child)
{
	if ((target.flags & (B_WATCH_DIRECTORY | B_WATCH_CHILDREN)) == 0)
		return;

	BMessage message(B_NODE_MONITOR);
	message.AddInt32("opcode", B_ENTRY_CREATED);
	message.AddDevice("device", child.device);
	message.AddInode("directory", watch.node);
	message.AddInode("node", child.node);
	message.AddString("name", child_path(watch.path, name).c_str());
	send_node_monitor_message(target, message);
}


static void
send_entry_removed(const node_monitor_target& target,
	const node_monitor_watch& watch, const char* name,
	const node_monitor_child& child)
{
	if ((target.flags & (B_WATCH_DIRECTORY | B_WATCH_CHILDREN)) == 0)
		return;

	BMessage message(B_NODE_MONITOR);
	message.AddInt32("opcode", B_ENTRY_REMOVED);
	message.AddDevice("device", child.device);
	message.AddInode("directory", watch.node);
	message.AddInode("node", child.node);
	message.AddString("name", child_path(watch.path, name).c_str());
	send_node_monitor_message(target, message);
}


static void
send_entry_moved(const node_monitor_target& target,
	const pending_move& from, const node_monitor_watch& toWatch,
	const char* toName, const node_monitor_child& child)
{
	if ((target.flags & (B_WATCH_DIRECTORY | B_WATCH_CHILDREN)) == 0)
		return;

	BMessage message(B_NODE_MONITOR);
	message.AddInt32("opcode", B_ENTRY_MOVED);
	message.AddDevice("device", from.device);
	message.AddInode("from directory", from.directory);
	message.AddInode("to directory", toWatch.node);
	message.AddDevice("node device", child.device);
	message.AddInode("node", child.node);
	message.AddString("from name", from.path.c_str());
	message.AddString("name", child_path(toWatch.path, toName).c_str());
	send_node_monitor_message(target, message);
}


static void
send_self_removed(const node_monitor_target& target, dev_t device,
	ino_t parentNode, ino_t node, const char* path)
{
	if ((target.flags & B_WATCH_NAME) == 0)
		return;

	BMessage message(B_NODE_MONITOR);
	message.AddInt32("opcode", B_ENTRY_REMOVED);
	message.AddDevice("device", device);
	message.AddInode("directory", parentNode);
	message.AddInode("node", node);
	message.AddString("name", path);
	send_node_monitor_message(target, message);
}


static void
send_self_removed(const node_monitor_target& target,
	const node_monitor_watch& watch)
{
	send_self_removed(target, watch.device, watch.parentNode, watch.node,
		watch.path.c_str());
}


static void
send_self_moved(const node_monitor_target& target,
	const pending_self_move& from, const node_monitor_watch& toWatch)
{
	if ((target.flags & B_WATCH_NAME) == 0)
		return;

	BMessage message(B_NODE_MONITOR);
	message.AddInt32("opcode", B_ENTRY_MOVED);
	message.AddDevice("device", toWatch.device);
	message.AddInode("from directory", from.parentNode);
	message.AddInode("to directory", toWatch.parentNode);
	message.AddDevice("node device", toWatch.device);
	message.AddInode("node", toWatch.node);
	message.AddString("from name", from.path.c_str());
	message.AddString("name", toWatch.path.c_str());
	send_node_monitor_message(target, message);
}


static void
send_stat_changed_for_node(const node_monitor_target& target, dev_t device,
	ino_t node, uint32 fields)
{
	if ((target.flags & (B_WATCH_STAT | B_WATCH_INTERIM_STAT)) == 0)
		return;

	BMessage message(B_NODE_MONITOR);
	message.AddInt32("opcode", B_STAT_CHANGED);
	message.AddDevice("device", device);
	message.AddInode("node", node);
	message.AddInt32("fields", (int32)fields);
	send_node_monitor_message(target, message);
}


static void
send_stat_changed(const node_monitor_target& target,
	const node_monitor_watch& watch, uint32 fields)
{
	send_stat_changed_for_node(target, watch.device, watch.node, fields);
}


static void
send_attr_changed_for_node(const node_monitor_target& target, dev_t device,
	ino_t node)
{
	if ((target.flags & B_WATCH_ATTR) == 0)
		return;

	BMessage message(B_NODE_MONITOR);
	message.AddInt32("opcode", B_ATTR_CHANGED);
	message.AddDevice("device", device);
	message.AddInode("node", node);
	send_node_monitor_message(target, message);
}


static void
send_attr_changed(const node_monitor_target& target,
	const node_monitor_watch& watch)
{
	send_attr_changed_for_node(target, watch.device, watch.node);
}


static void
copy_watch_state(int wd, node_monitor_watch& copy)
{
	pthread_mutex_lock(&sNodeMonitorLock);
	auto it = sNodeMonitorWatchesByWD.find(wd);
	if (it != sNodeMonitorWatchesByWD.end())
		copy = *it->second;
	pthread_mutex_unlock(&sNodeMonitorLock);
}


static status_t
refresh_inotify_watch(node_monitor_watch& watch);


static status_t
attach_parent_watch(node_monitor_watch& watch)
{
	if (watch.parentWatch != NULL || !watch_needs_name_monitor(watch))
		return B_OK;

	parent_monitor_watch* parentWatch = NULL;
	auto it = sParentMonitorWatchesByPath.find(watch.parentPath);
	if (it != sParentMonitorWatchesByPath.end()) {
		parentWatch = it->second;
	} else {
		parentWatch = new(std::nothrow) parent_monitor_watch;
		if (parentWatch == NULL)
			return B_NO_MEMORY;

		struct stat st;
		if (lstat(watch.parentPath.c_str(), &st) != 0) {
			delete parentWatch;
			return to_status(errno);
		}

		int wd = inotify_add_watch(sInotifyFD, watch.parentPath.c_str(),
			IN_MOVED_FROM | IN_MOVED_TO);
		if (wd < 0) {
			delete parentWatch;
			return to_status(errno);
		}

		parentWatch->wd = wd;
		parentWatch->path = watch.parentPath;
		parentWatch->device = st.st_dev;
		parentWatch->node = st.st_ino;
		parentWatch->refCount = 0;
		sParentMonitorWatchesByWD[wd] = parentWatch;
		sParentMonitorWatchesByPath[parentWatch->path] = parentWatch;
	}

	parentWatch->refCount++;
	parentWatch->namedWatches[watch.name] = &watch;
	watch.parentWatch = parentWatch;
	return B_OK;
}


static void
detach_parent_watch(node_monitor_watch& watch)
{
	parent_monitor_watch* parentWatch = watch.parentWatch;
	if (parentWatch == NULL)
		return;

	auto it = parentWatch->namedWatches.find(watch.name);
	if (it != parentWatch->namedWatches.end() && it->second == &watch)
		parentWatch->namedWatches.erase(it);

	watch.parentWatch = NULL;

	if (--parentWatch->refCount > 0)
		return;

	if (parentWatch->wd >= 0)
		inotify_rm_watch(sInotifyFD, parentWatch->wd);
	sParentMonitorWatchesByWD.erase(parentWatch->wd);
	sParentMonitorWatchesByPath.erase(parentWatch->path);
	delete parentWatch;
}


static bool
process_parent_inotify_event(const struct inotify_event* event)
{
	if (event->len == 0 || event->name[0] == '\0')
		return false;

	pthread_mutex_lock(&sNodeMonitorLock);
	auto parentIt = sParentMonitorWatchesByWD.find(event->wd);
	if (parentIt == sParentMonitorWatchesByWD.end()) {
		pthread_mutex_unlock(&sNodeMonitorLock);
		return false;
	}

	parent_monitor_watch* parentWatch = parentIt->second;
	std::string name(event->name);

	if ((event->mask & IN_MOVED_FROM) != 0 && event->cookie != 0) {
		auto childIt = parentWatch->namedWatches.find(name);
		if (childIt != parentWatch->namedWatches.end()) {
			node_monitor_watch* watch = childIt->second;
			sPendingSelfMoves[event->cookie] = {
				watch,
				watch->targets,
				watch->path,
				watch->parentPath,
				watch->name,
				watch->device,
				watch->node,
				watch->parentNode,
				watch->isDirectory
			};
		}

		pthread_mutex_unlock(&sNodeMonitorLock);
		return true;
	}

	if ((event->mask & IN_MOVED_TO) != 0 && event->cookie != 0) {
		auto moveIt = sPendingSelfMoves.find(event->cookie);
		if (moveIt == sPendingSelfMoves.end()) {
			pthread_mutex_unlock(&sNodeMonitorLock);
			return true;
		}

		pending_self_move move = moveIt->second;
		node_monitor_watch* watch = move.watch;
		if (watch == NULL || watch->parentWatch != parentWatch) {
			pthread_mutex_unlock(&sNodeMonitorLock);
			return true;
		}

		std::string newPath = child_path(parentWatch->path, name.c_str());
		struct stat st;
		if (lstat(newPath.c_str(), &st) != 0
			|| st.st_dev != watch->device
			|| st.st_ino != watch->node) {
			pthread_mutex_unlock(&sNodeMonitorLock);
			return true;
		}

		sPendingSelfMoves.erase(moveIt);

		parentWatch->namedWatches.erase(move.name);
		sNodeMonitorWatchesByPath.erase(watch->path);
		watch->path = newPath;
		watch->name = name;
		watch->parentPath = parentWatch->path;
		watch->parentNode = parentWatch->node;
		watch->isDirectory = S_ISDIR(st.st_mode);
		refresh_children(*watch);
		status_t error = refresh_inotify_watch(*watch);
		if (error == B_OK)
			sNodeMonitorWatchesByPath[watch->path] = watch;
		parentWatch->namedWatches[watch->name] = watch;

		std::vector<node_monitor_target> targets = watch->targets;
		pthread_mutex_unlock(&sNodeMonitorLock);

		for (const node_monitor_target& target : targets)
			send_self_moved(target, move, *watch);

		return true;
	}

	pthread_mutex_unlock(&sNodeMonitorLock);
	return true;
}


static void
process_inotify_event(const struct inotify_event* event)
{
	if ((event->mask & IN_IGNORED) != 0)
		return;

	if (process_parent_inotify_event(event))
		return;

	node_monitor_watch watch{};
	watch.wd = -1;
	copy_watch_state(event->wd, watch);
	if (watch.wd < 0)
		return;

	if (event->len > 0 && event->name[0] != '\0') {
		std::vector<node_monitor_target> targets = watch.targets;
		std::string name(event->name);

		if ((event->mask & (IN_CREATE | IN_MOVED_TO)) != 0) {
			node_monitor_child child;
			if (stat_child(watch.path, name.c_str(), child) != B_OK)
				return;

			pthread_mutex_lock(&sNodeMonitorLock);
			auto watchIt = sNodeMonitorWatchesByWD.find(event->wd);
			if (watchIt != sNodeMonitorWatchesByWD.end())
				watchIt->second->children[name] = child;

			auto moveIt = sPendingMoves.find(event->cookie);
			bool hasMove = (event->mask & IN_MOVED_TO) != 0
				&& event->cookie != 0 && moveIt != sPendingMoves.end();
			pending_move move;
			if (hasMove) {
				move = moveIt->second;
				sPendingMoves.erase(moveIt);
			}
			pthread_mutex_unlock(&sNodeMonitorLock);

			for (const node_monitor_target& target : targets) {
				if (hasMove)
					send_entry_moved(target, move, watch, name.c_str(), child);
				else
					send_entry_created(target, watch, name.c_str(), child);
			}
			return;
		}

		if ((event->mask & (IN_DELETE | IN_MOVED_FROM)) != 0) {
			node_monitor_child child = { watch.device, 0, false };

			pthread_mutex_lock(&sNodeMonitorLock);
			auto watchIt = sNodeMonitorWatchesByWD.find(event->wd);
			if (watchIt != sNodeMonitorWatchesByWD.end()) {
				auto childIt = watchIt->second->children.find(name);
				if (childIt != watchIt->second->children.end()) {
					child = childIt->second;
					watchIt->second->children.erase(childIt);
				}
			}
			if ((event->mask & IN_MOVED_FROM) != 0 && event->cookie != 0) {
				sPendingMoves[event->cookie] = {
					event->wd, child_path(watch.path, name.c_str()), child.device,
					child.node, watch.node,
					child.isDirectory
				};
				pthread_mutex_unlock(&sNodeMonitorLock);
				return;
			}
			pthread_mutex_unlock(&sNodeMonitorLock);

			for (const node_monitor_target& target : targets)
				send_entry_removed(target, watch, name.c_str(), child);
			return;
		}

		if ((event->mask & (IN_ATTRIB | IN_MODIFY | IN_CLOSE_WRITE)) != 0) {
			node_monitor_child child;
			if (stat_child(watch.path, name.c_str(), child) != B_OK) {
				auto childIt = watch.children.find(name);
				if (childIt == watch.children.end())
					return;
				child = childIt->second;
			}

			pthread_mutex_lock(&sNodeMonitorLock);
			auto watchIt = sNodeMonitorWatchesByWD.find(event->wd);
			if (watchIt != sNodeMonitorWatchesByWD.end())
				watchIt->second->children[name] = child;
			pthread_mutex_unlock(&sNodeMonitorLock);

			for (const node_monitor_target& target : targets) {
				if ((target.flags & B_WATCH_CHILDREN) == 0)
					continue;

				if ((event->mask & IN_ATTRIB) != 0) {
					send_attr_changed_for_node(target, child.device,
						child.node);
					send_stat_changed_for_node(target, child.device,
						child.node, B_STAT_MODE | B_STAT_UID | B_STAT_GID
							| B_STAT_CHANGE_TIME);
				} else {
					send_stat_changed_for_node(target, child.device,
						child.node, B_STAT_SIZE
							| B_STAT_MODIFICATION_TIME);
				}
			}
			return;
		}
	}

	if ((event->mask & IN_DELETE_SELF) != 0) {
		for (const node_monitor_target& target : watch.targets)
			send_self_removed(target, watch);
		return;
	}

	if ((event->mask & IN_MOVE_SELF) != 0)
		return;

	if ((event->mask & IN_ATTRIB) != 0) {
		for (const node_monitor_target& target : watch.targets) {
			send_attr_changed(target, watch);
			send_stat_changed(target, watch, B_STAT_MODE | B_STAT_UID
				| B_STAT_GID | B_STAT_CHANGE_TIME);
		}
		return;
	}

	if ((event->mask & (IN_MODIFY | IN_CLOSE_WRITE)) != 0) {
		for (const node_monitor_target& target : watch.targets) {
			send_stat_changed(target, watch,
				B_STAT_SIZE | B_STAT_MODIFICATION_TIME);
		}
	}
}


static void
flush_pending_moves_as_removals()
{
	std::vector<pending_move> moves;

	pthread_mutex_lock(&sNodeMonitorLock);
	for (const auto& entry : sPendingMoves)
		moves.push_back(entry.second);
	sPendingMoves.clear();
	pthread_mutex_unlock(&sNodeMonitorLock);

	for (const pending_move& move : moves) {
		node_monitor_watch watch{};
		watch.wd = -1;
		copy_watch_state(move.wd, watch);
		if (watch.wd < 0)
			continue;

		node_monitor_child child = { move.device, move.node, move.isDirectory };
		for (const node_monitor_target& target : watch.targets)
			send_entry_removed(target, watch, move.path.c_str(), child);
	}
}


static void
flush_pending_self_moves_as_removals()
{
	std::vector<pending_self_move> moves;

	pthread_mutex_lock(&sNodeMonitorLock);
	for (const auto& entry : sPendingSelfMoves)
		moves.push_back(entry.second);
	sPendingSelfMoves.clear();
	pthread_mutex_unlock(&sNodeMonitorLock);

	for (const pending_self_move& move : moves) {
		for (const node_monitor_target& target : move.targets) {
			send_self_removed(target, move.device, move.parentNode, move.node,
				move.path.c_str());
		}
	}
}


static void*
node_monitor_thread(void*)
{
	char buffer[16 * 1024]
		__attribute__((aligned(__alignof__(struct inotify_event))));

	for (;;) {
		ssize_t bytesRead = read(sInotifyFD, buffer, sizeof(buffer));
		if (bytesRead < 0) {
			if (errno == EINTR)
				continue;
			break;
		}

		for (char* pointer = buffer; pointer < buffer + bytesRead;) {
			const struct inotify_event* event
				= reinterpret_cast<const struct inotify_event*>(pointer);
			process_inotify_event(event);
			pointer += sizeof(struct inotify_event) + event->len;
		}

		flush_pending_moves_as_removals();
		flush_pending_self_moves_as_removals();
	}

	return NULL;
}


static status_t
ensure_node_monitor_thread()
{
	if (sInotifyFD >= 0 && sNodeMonitorThreadStarted)
		return B_OK;

	sInotifyFD = inotify_init1(IN_CLOEXEC);
	if (sInotifyFD < 0)
		return to_status(errno);

	int result = pthread_create(&sNodeMonitorThread, NULL, node_monitor_thread,
		NULL);
	if (result != 0) {
		close(sInotifyFD);
		sInotifyFD = -1;
		return to_status(result);
	}

	pthread_detach(sNodeMonitorThread);
	sNodeMonitorThreadStarted = true;
	return B_OK;
}


static status_t
refresh_inotify_watch(node_monitor_watch& watch)
{
	uint32 mask = combined_watch_mask(watch);
	if (mask == 0)
		return B_BAD_VALUE;

	int wd = inotify_add_watch(sInotifyFD, watch.path.c_str(), mask);
	if (wd < 0)
		return to_status(errno);

	if (watch.wd != wd) {
		sNodeMonitorWatchesByWD.erase(watch.wd);
		watch.wd = wd;
		sNodeMonitorWatchesByWD[wd] = &watch;
	}

	watch.mask = mask;
	return B_OK;
}

#endif	// __linux__


}	// namespace


extern "C" status_t
_kstart_watching_path_(const char* path, ino_t node, uint32 flags, port_id port,
	int32 handlerToken)
{
	(void)node;

#ifndef __linux__
	(void)path;
	(void)flags;
	(void)port;
	(void)handlerToken;
	return B_NOT_SUPPORTED;
#else
	if (path == NULL || path[0] == '\0' || port < 0)
		return B_BAD_VALUE;
	if (flags == 0 || (flags & B_WATCH_MOUNT) != 0)
		return B_NOT_SUPPORTED;

	char realPath[PATH_MAX];
	if (realpath(path, realPath) == NULL)
		return to_status(errno);

	struct stat st;
	if (lstat(realPath, &st) != 0)
		return to_status(errno);

	std::string parentPath;
	std::string name;
	split_path(realPath, parentPath, name);

	struct stat parentStat;
	ino_t parentNode = 0;
	if (lstat(parentPath.c_str(), &parentStat) == 0)
		parentNode = parentStat.st_ino;

	pthread_mutex_lock(&sNodeMonitorLock);
	status_t error = ensure_node_monitor_thread();
	if (error != B_OK) {
		pthread_mutex_unlock(&sNodeMonitorLock);
		return error;
	}

	node_monitor_watch* watch = NULL;
	auto it = sNodeMonitorWatchesByPath.find(realPath);
	if (it != sNodeMonitorWatchesByPath.end())
		watch = it->second;
	else {
		watch = new(std::nothrow) node_monitor_watch;
		if (watch == NULL) {
			pthread_mutex_unlock(&sNodeMonitorLock);
			return B_NO_MEMORY;
		}

		watch->wd = -1;
		watch->path = realPath;
		watch->parentPath = parentPath;
		watch->name = name;
		watch->device = st.st_dev;
		watch->node = st.st_ino;
		watch->parentNode = parentNode;
		watch->isDirectory = S_ISDIR(st.st_mode);
		watch->mask = 0;
		watch->parentWatch = NULL;
		refresh_children(*watch);
		sNodeMonitorWatchesByPath[watch->path] = watch;
	}

	auto targetIt = std::find_if(watch->targets.begin(), watch->targets.end(),
		[port, handlerToken](const node_monitor_target& target) {
			return target_matches(target, port, handlerToken);
		});
	if (targetIt != watch->targets.end())
		targetIt->flags |= flags;
	else
		watch->targets.push_back({ port, handlerToken, flags });

	error = attach_parent_watch(*watch);
	if (error != B_OK) {
		if (targetIt != watch->targets.end())
			targetIt->flags &= ~flags;
		else
			watch->targets.pop_back();
		if (watch->targets.empty()) {
			sNodeMonitorWatchesByPath.erase(watch->path);
			delete watch;
		}
		pthread_mutex_unlock(&sNodeMonitorLock);
		return error;
	}

	error = refresh_inotify_watch(*watch);
	if (error != B_OK && watch->targets.size() == 1) {
		detach_parent_watch(*watch);
		sNodeMonitorWatchesByPath.erase(watch->path);
		delete watch;
	}

	pthread_mutex_unlock(&sNodeMonitorLock);
	return error;
#endif
}


extern "C" status_t
_kstop_watching_path_(const char* path, port_id port, int32 handlerToken)
{
#ifndef __linux__
	(void)path;
	(void)port;
	(void)handlerToken;
	return B_NOT_SUPPORTED;
#else
	if (path == NULL || path[0] == '\0' || port < 0)
		return B_BAD_VALUE;

	char realPath[PATH_MAX];
	if (realpath(path, realPath) == NULL)
		strlcpy(realPath, path, sizeof(realPath));

	pthread_mutex_lock(&sNodeMonitorLock);
	auto it = sNodeMonitorWatchesByPath.find(realPath);
	if (it == sNodeMonitorWatchesByPath.end()) {
		pthread_mutex_unlock(&sNodeMonitorLock);
		return B_NAME_NOT_FOUND;
	}

	node_monitor_watch* watch = it->second;
	watch->targets.erase(std::remove_if(watch->targets.begin(),
		watch->targets.end(),
		[port, handlerToken](const node_monitor_target& target) {
			return target_matches(target, port, handlerToken);
		}), watch->targets.end());

	if (watch->targets.empty()) {
		detach_parent_watch(*watch);
		if (watch->wd >= 0) {
			inotify_rm_watch(sInotifyFD, watch->wd);
			sNodeMonitorWatchesByWD.erase(watch->wd);
		}
		sNodeMonitorWatchesByPath.erase(it);
		delete watch;
	} else {
		if (!watch_needs_name_monitor(*watch))
			detach_parent_watch(*watch);
		refresh_inotify_watch(*watch);
	}

	pthread_mutex_unlock(&sNodeMonitorLock);
	return B_OK;
#endif
}


extern "C" status_t
_kstop_notifying_(port_id port, int32 handlerToken)
{
#ifndef __linux__
	(void)port;
	(void)handlerToken;
	return B_NOT_SUPPORTED;
#else
	if (port < 0)
		return B_BAD_VALUE;

	pthread_mutex_lock(&sNodeMonitorLock);
	for (auto it = sNodeMonitorWatchesByPath.begin();
			it != sNodeMonitorWatchesByPath.end();) {
		node_monitor_watch* watch = it->second;
		watch->targets.erase(std::remove_if(watch->targets.begin(),
			watch->targets.end(),
			[port, handlerToken](const node_monitor_target& target) {
				return target_matches(target, port, handlerToken);
			}), watch->targets.end());

		if (watch->targets.empty()) {
			detach_parent_watch(*watch);
			if (watch->wd >= 0) {
				inotify_rm_watch(sInotifyFD, watch->wd);
				sNodeMonitorWatchesByWD.erase(watch->wd);
			}
			it = sNodeMonitorWatchesByPath.erase(it);
			delete watch;
		} else {
			if (!watch_needs_name_monitor(*watch))
				detach_parent_watch(*watch);
			refresh_inotify_watch(*watch);
			++it;
		}
	}

	pthread_mutex_unlock(&sNodeMonitorLock);
	return B_OK;
#endif
}


// actual implementation

// Subscribes or unsubscribes a target to node and/or mount watching.
status_t
watch_node(const node_ref* node, uint32 flags, BMessenger target)
{
	status_t error = (target.IsValid() ? B_OK : B_BAD_VALUE);
	if (error == B_OK) {
		BLooper *looper = NULL;
		BHandler *handler = target.Target(&looper);
		error = watch_node(node, flags, handler, looper);
	}
	return error;
}


// Subscribes or unsubscribes a handler or looper to node and/or mount
// watching.
status_t
watch_node(const node_ref* node, uint32 flags, const BHandler* handler, const BLooper* looper)
{
	return B_NOT_SUPPORTED;
}

status_t
watch_path(const char* path, uint32 flags, BMessenger target)
{
	status_t error = (target.IsValid() ? B_OK : B_BAD_VALUE);
	if (error == B_OK) {
		BLooper* looper = NULL;
		BHandler* handler = target.Target(&looper);
		error = watch_path(path, flags, handler, looper);
	}
	return error;
}


status_t
watch_path(const char* path, uint32 flags, const BHandler* handler, const BLooper* looper)
{
	status_t error = B_OK;
	// check looper and handler and get the handler token
	int32 handlerToken = -2;
	if (handler) {
		handlerToken = _get_object_token_(handler);
		if (looper) {
			if (looper != handler->Looper())
				error = B_BAD_VALUE;
		} else {
			looper = handler->Looper();
			if (!looper)
				error = B_BAD_VALUE;
		}
	} else if (!looper)
		error = B_BAD_VALUE;
	if (error == B_OK) {
		port_id port = _get_looper_port_(looper);
		if (flags == B_STOP_WATCHING) {
			// unsubscribe from path node watching
			if (path)
				error = _kstop_watching_path_(path, port, handlerToken);
			else
				error = B_BAD_VALUE;
		} else {
			// subscribe to...
			// mount watching
			if (flags & B_WATCH_MOUNT) {
				// TODO: check this, probably incorrect
				error = _kstart_watching_path_(path, 0, B_WATCH_MOUNT, port,
					handlerToken);
				flags &= ~B_WATCH_MOUNT;
			}
			// path watching
			if (error == B_OK && flags)
				error = _kstart_watching_path_(path, 0, flags, port,
					handlerToken);
		}
	}
	return error;
}


// Unsubscribes a target from node and mount monitoring.
status_t
stop_watching(BMessenger target)
{
	status_t error = (target.IsValid() ? B_OK : B_BAD_VALUE);
	if (error == B_OK) {
		BLooper *looper = NULL;
		BHandler *handler = target.Target(&looper);
		error = stop_watching(handler, looper);
	}
	return error;
}


// Unsubscribes a target from node and mount monitoring.
status_t
stop_watching(const BHandler* handler, const BLooper* looper)
{
	status_t error = B_OK;
	// check looper and handler and get the handler token
	int32 handlerToken = -2;
	if (handler) {
		handlerToken = _get_object_token_(handler);
		if (looper) {
			if (looper != handler->Looper())
				error = B_BAD_VALUE;
		} else {
			looper = handler->Looper();
			if (!looper)
				error = B_BAD_VALUE;
		}
	} else if (!looper)
		error = B_BAD_VALUE;
	// unsubscribe
	if (error == B_OK) {
		port_id port = _get_looper_port_(looper);
		error = _kstop_notifying_(port, handlerToken);
	}
	return error;
}

