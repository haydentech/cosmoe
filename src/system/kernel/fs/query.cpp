/*
 * Copyright 2026, Bill Hayden, hayden@haydentech.com.
 * Distributed under the terms of the MIT License.
 */

/*!
	\file query.cpp
	Userspace query engine behind _kern_open_query().

	Cosmoe's file systems have no query indexes, so a query is answered by
	walking the volume once and evaluating the predicate against every entry.
	Matches are written to an unlinked temporary file, whose descriptor is
	handed back as the query fd: _kern_read_dir() turns the records back into
	dirents, and lseek() on the descriptor rewinds the result set.

	Live queries (B_LIVE_QUERY) are accepted, but not yet monitored -- the
	result set is the snapshot taken when the query was opened.
*/

#include "query.h"

#include <config.h>

#include <errno.h>
#include <NodeMonitor.h>
#include <fs_query.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>

#include <StorageDefs.h>

#if defined(__linux__)
#	include <mntent.h>
#	include <poll.h>
#	include <sys/inotify.h>
#	include <sys/stat.h>
#	if defined(HAVE_SYS_XATTR_H)
#		include <sys/xattr.h>
#	endif
#endif


namespace BPrivate {
namespace Storage {
namespace Query {


#if defined(__linux__)

namespace {

// Safety valve against pathological trees; real paths hit PATH_MAX first.
const int kMaxDepth = 256;
// Attributes bigger than this are never meaningful to a query.
const off_t kMaxAttributeSize = 64 * 1024;

// What a live query needs to hear about.
const uint32 kWatchMask = IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO
	| IN_ATTRIB | IN_DELETE_SELF | IN_MOVE_SELF | IN_ONLYDIR;

// inotify watches are a per-user, system-wide resource (fs.inotify
// .max_user_watches, typically 65536). A query over a large volume could
// consume every one of them and leave node monitoring -- and with it the
// rest of the desktop -- without watches, so each live query stops adding
// them once it has used its share and degrades to the snapshot it scanned.
const size_t kMaxLiveQueryWatches = 8192;


//	#pragma mark - Predicate tree


enum NodeKind {
	kAndNode,
	kOrNode,
	kNotNode,
	kComparisonNode
};


enum Comparison {
	kEqual,
	kNotEqual,
	kLess,
	kLessOrEqual,
	kGreater,
	kGreaterOrEqual,
	kContains
};


struct EntryInfo {
	const char*	path;
	const char*	name;
	struct stat	stat;
};


struct Node {
	Node(NodeKind kind)
		:
		fKind(kind),
		fLeft(NULL),
		fRight(NULL),
		fComparison(kEqual),
		fIntegerValue(0),
		fValueIsInteger(false)
	{
	}

	~Node()
	{
		delete fLeft;
		delete fRight;
	}

	NodeKind		fKind;
	Node*			fLeft;
	Node*			fRight;
	std::string		fAttribute;
	Comparison		fComparison;
	std::string		fStringValue;
	int64			fIntegerValue;
	bool			fValueIsInteger;
};


/*! Matches \a string against a Haiku style pattern: '*' and '?' wildcards,
	'[...]' character classes and '\' escapes. Tracker turns case-insensitive
	searches into "[aA]" patterns, so matching is case sensitive.
*/
bool
Matches(const char* pattern, const char* string)
{
	if (pattern[0] == '\0')
		return string[0] == '\0';

	if (pattern[0] == '*') {
		while (pattern[0] == '*')
			pattern++;
		if (pattern[0] == '\0')
			return true;
		for (const char* position = string; ; position++) {
			if (Matches(pattern, position))
				return true;
			if (*position == '\0')
				return false;
		}
	}

	if (string[0] == '\0')
		return false;

	if (pattern[0] == '?')
		return Matches(pattern + 1, string + 1);

	if (pattern[0] == '[') {
		const char* position = pattern + 1;
		bool negate = false;
		if (*position == '!' || *position == '^') {
			negate = true;
			position++;
		}

		bool matched = false;
		while (*position != '\0' && *position != ']') {
			if (position[1] == '-' && position[2] != '\0'
					&& position[2] != ']') {
				if (string[0] >= position[0] && string[0] <= position[2])
					matched = true;
				position += 3;
			} else {
				if (string[0] == position[0])
					matched = true;
				position++;
			}
		}

		// A malformed class matches nothing.
		if (*position != ']')
			return false;
		if (matched == negate)
			return false;

		return Matches(position + 1, string + 1);
	}

	if (pattern[0] == '\\' && pattern[1] != '\0')
		pattern++;

	if (pattern[0] != string[0])
		return false;

	return Matches(pattern + 1, string + 1);
}


bool
ReadAttribute(const EntryInfo& entry, const std::string& attribute,
	std::string& value)
{
#if defined(HAVE_SYS_XATTR_H)
	if (attribute.empty() || attribute.size() > B_ATTR_NAME_LENGTH)
		return false;

	std::string name("user.");
	name += attribute;

	ssize_t size = lgetxattr(entry.path, name.c_str(), NULL, 0);
	if (size < 0 || size > kMaxAttributeSize)
		return false;

	value.resize(size);
	if (size > 0
			&& lgetxattr(entry.path, name.c_str(), &value[0], size) != size) {
		return false;
	}

	return true;
#else
	(void)entry;
	(void)attribute;
	(void)value;
	return false;
#endif
}


bool
AttributeString(const EntryInfo& entry, const std::string& attribute,
	std::string& value)
{
	// "name" is synthetic; every other attribute lives in the user xattr
	// namespace, where Tracker's MIME type also resides.
	if (attribute == "name") {
		value = entry.name;
		return true;
	}

	if (attribute == "mime")
		return ReadAttribute(entry, "BEOS:TYPE", value);

	return ReadAttribute(entry, attribute, value);
}


bool
AttributeInteger(const EntryInfo& entry, const std::string& attribute,
	int64& value)
{
	if (attribute == "size") {
		value = entry.stat.st_size;
		return true;
	}
	if (attribute == "last_modified") {
		value = entry.stat.st_mtime;
		return true;
	}
	if (attribute == "created" || attribute == "creation_time") {
		value = entry.stat.st_ctime;
		return true;
	}

	std::string raw;
	if (!AttributeString(entry, attribute, raw))
		return false;

	// Attributes written by Cosmoe are typed; fall back to parsing text for
	// attributes set by other tools.
	if (raw.size() == sizeof(int32)) {
		int32 number;
		memcpy(&number, raw.data(), sizeof(number));
		value = number;
		return true;
	}
	if (raw.size() == sizeof(int64)) {
		int64 number;
		memcpy(&number, raw.data(), sizeof(number));
		value = number;
		return true;
	}

	char* end = NULL;
	long long number = strtoll(raw.c_str(), &end, 0);
	if (end == raw.c_str())
		return false;

	value = number;
	return true;
}


bool
EvaluateNode(const Node& node, const EntryInfo& entry);


bool
EvaluateComparison(const Node& node, const EntryInfo& entry)
{
	if (node.fValueIsInteger) {
		int64 value;
		if (!AttributeInteger(entry, node.fAttribute, value))
			return false;

		switch (node.fComparison) {
			case kEqual:
				return value == node.fIntegerValue;
			case kNotEqual:
				return value != node.fIntegerValue;
			case kLess:
				return value < node.fIntegerValue;
			case kLessOrEqual:
				return value <= node.fIntegerValue;
			case kGreater:
				return value > node.fIntegerValue;
			case kGreaterOrEqual:
				return value >= node.fIntegerValue;
			case kContains:
				return false;
		}
		return false;
	}

	std::string value;
	if (!AttributeString(entry, node.fAttribute, value))
		return false;

	switch (node.fComparison) {
		case kEqual:
			return Matches(node.fStringValue.c_str(), value.c_str());
		case kNotEqual:
			return !Matches(node.fStringValue.c_str(), value.c_str());
		case kLess:
			return value < node.fStringValue;
		case kLessOrEqual:
			return value <= node.fStringValue;
		case kGreater:
			return value > node.fStringValue;
		case kGreaterOrEqual:
			return value >= node.fStringValue;
		case kContains:
			return value.find(node.fStringValue) != std::string::npos;
	}

	return false;
}


bool
EvaluateNode(const Node& node, const EntryInfo& entry)
{
	switch (node.fKind) {
		case kAndNode:
			return EvaluateNode(*node.fLeft, entry)
				&& EvaluateNode(*node.fRight, entry);
		case kOrNode:
			return EvaluateNode(*node.fLeft, entry)
				|| EvaluateNode(*node.fRight, entry);
		case kNotNode:
			return !EvaluateNode(*node.fLeft, entry);
		case kComparisonNode:
			return EvaluateComparison(node, entry);
	}

	return false;
}


//	#pragma mark - Predicate parser


enum TokenType {
	kTokenEnd,
	kTokenIdentifier,
	kTokenString,
	kTokenNumber,
	kTokenLeftParenthesis,
	kTokenRightParenthesis,
	kTokenAnd,
	kTokenOr,
	kTokenNot,
	kTokenEqual,
	kTokenNotEqual,
	kTokenLess,
	kTokenLessOrEqual,
	kTokenGreater,
	kTokenGreaterOrEqual,
	kTokenContains
};


class Parser {
public:
	Parser(const char* predicate, size_t length)
		:
		fPosition(predicate),
		fEnd(predicate + length),
		fToken(kTokenEnd),
		fNumber(0)
	{
	}

	Node* Parse()
	{
		if (!Advance())
			return NULL;

		Node* node = ParseOr();
		if (node == NULL)
			return NULL;

		if (fToken != kTokenEnd) {
			delete node;
			return NULL;
		}

		return node;
	}

private:
	bool Advance();
	bool ParseString();

	Node* ParseOr();
	Node* ParseAnd();
	Node* ParseUnary();
	Node* ParseComparison();

	const char*	fPosition;
	const char*	fEnd;
	TokenType	fToken;
	std::string	fText;
	int64		fNumber;
};


bool
Parser::Advance()
{
	while (fPosition < fEnd && isspace((unsigned char)*fPosition))
		fPosition++;

	fText.clear();
	fNumber = 0;

	if (fPosition >= fEnd) {
		fToken = kTokenEnd;
		return true;
	}

	char c = *fPosition;
	switch (c) {
		case '(':
			fPosition++;
			fToken = kTokenLeftParenthesis;
			return true;
		case ')':
			fPosition++;
			fToken = kTokenRightParenthesis;
			return true;
		case '&':
			fPosition++;
			if (fPosition < fEnd && *fPosition == '&')
				fPosition++;
			fToken = kTokenAnd;
			return true;
		case '|':
			fPosition++;
			if (fPosition < fEnd && *fPosition == '|')
				fPosition++;
			fToken = kTokenOr;
			return true;
		case '!':
			fPosition++;
			if (fPosition < fEnd && *fPosition == '=') {
				fPosition++;
				fToken = kTokenNotEqual;
			} else
				fToken = kTokenNot;
			return true;
		case '=':
			fPosition++;
			if (fPosition < fEnd && *fPosition == '=') {
				fPosition++;
			} else if (fPosition < fEnd && *fPosition == '~') {
				fPosition++;
				fToken = kTokenContains;
				return true;
			}
			fToken = kTokenEqual;
			return true;
		case '<':
			fPosition++;
			if (fPosition < fEnd && *fPosition == '=') {
				fPosition++;
				fToken = kTokenLessOrEqual;
			} else
				fToken = kTokenLess;
			return true;
		case '>':
			fPosition++;
			if (fPosition < fEnd && *fPosition == '=') {
				fPosition++;
				fToken = kTokenGreaterOrEqual;
			} else
				fToken = kTokenGreater;
			return true;
		case '"':
			return ParseString();
		case '%': {
			// BQuery normally resolves dates before calling the kernel; cope
			// with any that slip through, numeric or not.
			const char* end = fPosition + 1;
			while (end < fEnd && *end != '%')
				end++;
			if (end >= fEnd)
				return false;

			std::string inner(fPosition + 1, end);
			fPosition = end + 1;

			char* numberEnd = NULL;
			long long number = strtoll(inner.c_str(), &numberEnd, 0);
			if (numberEnd != inner.c_str() && *numberEnd == '\0') {
				fToken = kTokenNumber;
				fNumber = number;
			} else {
				fToken = kTokenString;
				fText = inner;
			}
			return true;
		}
	}

	if (isdigit((unsigned char)c)
			|| (c == '-' && fPosition + 1 < fEnd
				&& isdigit((unsigned char)fPosition[1]))) {
		char* end = NULL;
		long long number = strtoll(fPosition, &end, 0);
		if (end == fPosition)
			return false;

		fNumber = number;
		fPosition = end;
		fToken = kTokenNumber;
		return true;
	}

	const char* start = fPosition;
	while (fPosition < fEnd && !isspace((unsigned char)*fPosition)
			&& strchr("()!=<>&|\"", *fPosition) == NULL) {
		fPosition++;
	}

	if (fPosition == start) {
		fPosition++;
		return false;
	}

	fText.assign(start, fPosition - start);
	fToken = kTokenIdentifier;
	return true;
}


bool
Parser::ParseString()
{
	fPosition++;	// opening quote

	std::string value;
	while (fPosition < fEnd) {
		char c = *fPosition++;
		if (c == '\\') {
			if (fPosition >= fEnd)
				return false;
			value += *fPosition++;
			continue;
		}
		if (c == '"') {
			fText = value;
			fToken = kTokenString;
			return true;
		}
		value += c;
	}

	return false;
}


Node*
Parser::ParseOr()
{
	Node* node = ParseAnd();
	if (node == NULL)
		return NULL;

	while (fToken == kTokenOr) {
		if (!Advance()) {
			delete node;
			return NULL;
		}

		Node* right = ParseAnd();
		if (right == NULL) {
			delete node;
			return NULL;
		}

		Node* parent = new Node(kOrNode);
		parent->fLeft = node;
		parent->fRight = right;
		node = parent;
	}

	return node;
}


Node*
Parser::ParseAnd()
{
	Node* node = ParseUnary();
	if (node == NULL)
		return NULL;

	while (fToken == kTokenAnd) {
		if (!Advance()) {
			delete node;
			return NULL;
		}

		Node* right = ParseUnary();
		if (right == NULL) {
			delete node;
			return NULL;
		}

		Node* parent = new Node(kAndNode);
		parent->fLeft = node;
		parent->fRight = right;
		node = parent;
	}

	return node;
}


Node*
Parser::ParseUnary()
{
	if (fToken == kTokenNot) {
		if (!Advance())
			return NULL;

		Node* child = ParseUnary();
		if (child == NULL)
			return NULL;

		Node* node = new Node(kNotNode);
		node->fLeft = child;
		return node;
	}

	if (fToken == kTokenLeftParenthesis) {
		if (!Advance())
			return NULL;

		Node* node = ParseOr();
		if (node == NULL)
			return NULL;

		if (fToken != kTokenRightParenthesis) {
			delete node;
			return NULL;
		}

		if (!Advance()) {
			delete node;
			return NULL;
		}

		return node;
	}

	return ParseComparison();
}


Node*
Parser::ParseComparison()
{
	if (fToken != kTokenIdentifier)
		return NULL;

	std::string attribute = fText;
	if (!Advance())
		return NULL;

	Comparison comparison;
	switch (fToken) {
		case kTokenEqual:
			comparison = kEqual;
			break;
		case kTokenNotEqual:
			comparison = kNotEqual;
			break;
		case kTokenLess:
			comparison = kLess;
			break;
		case kTokenLessOrEqual:
			comparison = kLessOrEqual;
			break;
		case kTokenGreater:
			comparison = kGreater;
			break;
		case kTokenGreaterOrEqual:
			comparison = kGreaterOrEqual;
			break;
		case kTokenContains:
			comparison = kContains;
			break;
		default:
			return NULL;
	}

	if (!Advance())
		return NULL;

	if (fToken != kTokenString && fToken != kTokenNumber)
		return NULL;

	Node* node = new Node(kComparisonNode);
	node->fAttribute = attribute;
	node->fComparison = comparison;

	if (fToken == kTokenString)
		node->fStringValue = fText;
	else {
		node->fValueIsInteger = true;
		node->fIntegerValue = fNumber;
	}

	if (!Advance()) {
		delete node;
		return NULL;
	}

	return node;
}


Node*
ParsePredicate(const char* predicate, size_t length)
{
	Parser parser(predicate, length);
	return parser.Parse();
}


//	#pragma mark - Result set


struct ResultRecord {
	uint64	directory;	// inode of the directory containing the entry
	uint32	length;		// path length, excluding the terminator
	uint32	type;		// dirent d_type
};


int
TypeForMode(mode_t mode)
{
	if (S_ISDIR(mode))
		return DT_DIR;
	if (S_ISLNK(mode))
		return DT_LNK;
	if (S_ISREG(mode))
		return DT_REG;
	if (S_ISFIFO(mode))
		return DT_FIFO;
	if (S_ISSOCK(mode))
		return DT_SOCK;
	if (S_ISCHR(mode))
		return DT_CHR;
	if (S_ISBLK(mode))
		return DT_BLK;
	return DT_UNKNOWN;
}


bool
WriteRecord(int fd, ino_t directory, const std::string& path, mode_t mode)
{
	ResultRecord record;
	record.directory = (uint64)directory;
	record.length = (uint32)path.size();
	record.type = (uint32)TypeForMode(mode);

	if (write(fd, &record, sizeof(record)) != (ssize_t)sizeof(record))
		return false;

	return write(fd, path.data(), path.size()) == (ssize_t)path.size();
}


//	#pragma mark - Query descriptors


std::mutex&
QueryLock()
{
	static std::mutex lock;
	return lock;
}


std::set<int>&
QueryFDs()
{
	static std::set<int> fds;
	return fds;
}


//	#pragma mark - Live queries


struct LiveQuery {
	LiveQuery()
		:
		fd(-1),
		inotifyFD(-1),
		port(-1),
		token(0),
		predicate(NULL),
		device(0),
		stopped(false),
		watchLimitReached(false)
	{
	}

	~LiveQuery()
	{
		delete predicate;
	}

	int								fd;
	int								inotifyFD;
	port_id							port;
	int32							token;
	Node*							predicate;
	dev_t							device;
	std::map<int, std::string>		watchPaths;
	std::map<int, ino_t>			watchInodes;
	std::map<std::string, ino_t>	entries;
	std::atomic<bool>				stopped;
	std::atomic<bool>				watchLimitReached;
};


std::map<int, std::shared_ptr<LiveQuery> >&
LiveQueries()
{
	static std::map<int, std::shared_ptr<LiveQuery> > queries;
	return queries;
}


QueryUpdateSender&
UpdateSender()
{
	static QueryUpdateSender sender = NULL;
	return sender;
}


void
SendUpdate(const std::shared_ptr<LiveQuery>& live, uint32 opcode,
	ino_t directory, ino_t node, const char* name)
{
	QueryUpdateSender sender = UpdateSender();
	if (sender == NULL || live->port < 0)
		return;

	sender(live->port, live->token, opcode, live->device, directory, node,
		name);
}


void
AddWatch(LiveQuery* live, const std::string& path, ino_t inode)
{
	if (live == NULL || live->inotifyFD < 0 || live->watchLimitReached)
		return;

	if (live->watchPaths.size() >= kMaxLiveQueryWatches) {
		live->watchLimitReached = true;
		return;
	}

	int descriptor = inotify_add_watch(live->inotifyFD, path.c_str(),
		kWatchMask);
	if (descriptor < 0) {
		// Out of watches (or descriptors) for the whole user; stop asking
		// rather than repeating a failing syscall for every directory.
		if (errno == ENOSPC || errno == EMFILE)
			live->watchLimitReached = true;
		return;
	}

	if (live->watchPaths.find(descriptor) != live->watchPaths.end())
		return;

	live->watchPaths[descriptor] = path;
	live->watchInodes[descriptor] = inode;
}


void
NotifyIfMatching(const std::shared_ptr<LiveQuery>& live, const std::string& path,
	const char* name, ino_t directory)
{
	struct stat stat;
	if (lstat(path.c_str(), &stat) != 0)
		return;

	EntryInfo info;
	info.path = path.c_str();
	info.name = name;
	info.stat = stat;

	bool matches = EvaluateNode(*live->predicate, info);
	std::map<std::string, ino_t>::iterator found = live->entries.find(path);

	if (matches) {
		bool known = found != live->entries.end();
		live->entries[path] = stat.st_ino;
		SendUpdate(live, known ? B_ATTR_CHANGED : B_ENTRY_CREATED, directory,
			stat.st_ino, path.c_str());
	} else if (found != live->entries.end()) {
		SendUpdate(live, B_ENTRY_REMOVED, directory, found->second,
			path.c_str());
		live->entries.erase(found);
	}
}


/*! Registers watches for a directory that appeared after the initial scan, so
	entries created inside it are still reported.
*/
void
WatchTree(const std::shared_ptr<LiveQuery>& live, const std::string& path,
	ino_t directory, int depth)
{
	if (depth > kMaxDepth)
		return;

	struct stat stat;
	if (lstat(path.c_str(), &stat) != 0 || stat.st_dev != live->device
			|| !S_ISDIR(stat.st_mode)) {
		return;
	}

	AddWatch(live.get(), path, stat.st_ino);

	const char* leaf = strrchr(path.c_str(), '/');
	NotifyIfMatching(live, path, leaf != NULL ? leaf + 1 : path.c_str(),
		directory);

	DIR* dir = opendir(path.c_str());
	if (dir == NULL)
		return;

	while (struct dirent* entry = readdir(dir)) {
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
			continue;

		std::string child(path);
		if (child.empty() || child[child.size() - 1] != '/')
			child += '/';
		child += entry->d_name;

		WatchTree(live, child, stat.st_ino, depth + 1);
	}

	closedir(dir);
}


void
HandleEvent(const std::shared_ptr<LiveQuery>& live,
	const struct inotify_event* event)
{
	std::map<int, std::string>::iterator watch
		= live->watchPaths.find(event->wd);
	if (watch == live->watchPaths.end())
		return;

	std::string directory = watch->second;
	ino_t directoryInode = live->watchInodes[event->wd];

	if ((event->mask & IN_IGNORED) != 0 || event->len == 0
			|| event->name[0] == '\0') {
		// The watched directory itself went away.
		live->watchInodes.erase(event->wd);
		live->watchPaths.erase(watch);
		return;
	}

	std::string path(directory);
	if (path.empty() || path[path.size() - 1] != '/')
		path += '/';
	path += event->name;

	if ((event->mask & (IN_CREATE | IN_MOVED_TO)) != 0) {
		struct stat stat;
		if (lstat(path.c_str(), &stat) == 0 && S_ISDIR(stat.st_mode))
			WatchTree(live, path, directoryInode, 0);
		else
			NotifyIfMatching(live, path, event->name, directoryInode);
	} else if ((event->mask & (IN_DELETE | IN_MOVED_FROM)) != 0) {
		std::map<std::string, ino_t>::iterator found = live->entries.find(path);
		if (found != live->entries.end()) {
			SendUpdate(live, B_ENTRY_REMOVED, directoryInode, found->second,
				path.c_str());
			live->entries.erase(found);
		}
	} else if ((event->mask & IN_ATTRIB) != 0) {
		NotifyIfMatching(live, path, event->name, directoryInode);
	}
}


void*
WatchThread(void* data)
{
	std::shared_ptr<LiveQuery>* holder
		= reinterpret_cast<std::shared_ptr<LiveQuery>*>(data);
	std::shared_ptr<LiveQuery> live = *holder;
	delete holder;

	char buffer[4096]
		__attribute__((aligned(__alignof__(struct inotify_event))));

	while (!live->stopped) {
		struct pollfd descriptor;
		descriptor.fd = live->inotifyFD;
		descriptor.events = POLLIN;
		descriptor.revents = 0;

		if (poll(&descriptor, 1, 250) <= 0)
			continue;

		ssize_t length = read(live->inotifyFD, buffer, sizeof(buffer));
		if (length <= 0)
			break;

		for (char* pointer = buffer; pointer < buffer + length;) {
			const struct inotify_event* event
				= reinterpret_cast<const struct inotify_event*>(pointer);
			HandleEvent(live, event);
			pointer += sizeof(struct inotify_event) + event->len;
		}
	}

	std::lock_guard<std::mutex> guard(QueryLock());
	std::map<int, std::shared_ptr<LiveQuery> >::iterator found
		= LiveQueries().find(live->fd);
	if (found != LiveQueries().end() && found->second.get() == live.get())
		LiveQueries().erase(found);

	return NULL;
}


bool
StartLiveQuery(const std::shared_ptr<LiveQuery>& live)
{
	pthread_t thread;
	std::shared_ptr<LiveQuery>* holder
		= new(std::nothrow) std::shared_ptr<LiveQuery>(live);
	if (holder == NULL)
		return false;

	if (pthread_create(&thread, NULL, WatchThread, holder) != 0) {
		delete holder;
		return false;
	}

	pthread_detach(thread);
	return true;
}


//	#pragma mark - Volume traversal


bool
MountPointForDevice(dev_t device, std::string& mountPoint)
{
	FILE* mounts = setmntent("/proc/mounts", "r");
	if (mounts == NULL)
		return false;

	bool found = false;
	struct mntent* entry;
	while ((entry = getmntent(mounts)) != NULL) {
		struct stat stat;
		if (::stat(entry->mnt_dir, &stat) != 0)
			continue;
		if (stat.st_dev != device)
			continue;

		// A device can be mounted several times; search the topmost one.
		if (!found || strlen(entry->mnt_dir) < mountPoint.size()) {
			mountPoint = entry->mnt_dir;
			found = true;
		}
	}

	endmntent(mounts);
	return found;
}


/*! Walks \a directory and writes a record for every entry matching
	\a predicate. Entries on other devices are skipped, so the walk stays on
	the queried volume (and never wanders into /proc, /sys or /dev).
*/
bool
ScanDirectory(const std::string& directory, dev_t device, const Node& predicate,
	int resultFD, int depth, LiveQuery* live)
{
	DIR* dir = opendir(directory.c_str());
	if (dir == NULL)
		return true;

	ino_t directoryInode = 0;
	struct stat directoryStat;
	if (lstat(directory.c_str(), &directoryStat) == 0)
		directoryInode = directoryStat.st_ino;

	if (live != NULL)
		AddWatch(live, directory, directoryInode);

	bool success = true;
	while (success) {
		struct dirent* entry = readdir(dir);
		if (entry == NULL)
			break;

		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
			continue;

		std::string path(directory);
		if (path.empty() || path[path.size() - 1] != '/')
			path += '/';
		path += entry->d_name;

		struct stat stat;
		if (lstat(path.c_str(), &stat) != 0)
			continue;
		if (stat.st_dev != device)
			continue;

		EntryInfo info;
		info.path = path.c_str();
		info.name = entry->d_name;
		info.stat = stat;

		if (EvaluateNode(predicate, info)) {
			if (live != NULL)
				live->entries[path] = stat.st_ino;
			if (!WriteRecord(resultFD, directoryInode, path, stat.st_mode)) {
				success = false;
				break;
			}
		}

		if (S_ISDIR(stat.st_mode) && depth < kMaxDepth) {
			success = ScanDirectory(path, device, predicate, resultFD,
				depth + 1, live);
		}
	}

	closedir(dir);
	return success;
}


}	// unnamed namespace


void
set_query_update_sender(QueryUpdateSender sender)
{
	UpdateSender() = sender;
}


int
open_query(dev_t device, const char* predicate, size_t length, uint32 flags,
	port_id port, int32 token)
{
	// Live queries are served as a snapshot; the notification port and token
	// are not used yet.
	(void)flags;
	(void)port;
	(void)token;

	if (predicate == NULL || length == 0) {
		errno = EINVAL;
		return -1;
	}

	Node* root = ParsePredicate(predicate, length);
	if (root == NULL) {
		errno = EINVAL;
		return -1;
	}

	std::string mountPoint;
	if (!MountPointForDevice(device, mountPoint)) {
		delete root;
		errno = ENOENT;
		return -1;
	}

	const char* tmpDirectory = getenv("TMPDIR");
	char templatePath[B_PATH_NAME_LENGTH];
	snprintf(templatePath, sizeof(templatePath), "%s/cosmoe-query-XXXXXX",
		(tmpDirectory != NULL && tmpDirectory[0] != '\0') ? tmpDirectory : "/tmp");

	int fd = mkstemp(templatePath);
	if (fd < 0) {
		delete root;
		return -1;
	}
	// The results are private to this process; the file disappears with it.
	unlink(templatePath);

	std::shared_ptr<LiveQuery> live;
	bool watch = (flags & B_LIVE_QUERY) != 0 && port >= 0;
	if (watch) {
		live.reset(new(std::nothrow) LiveQuery);
		if (live != NULL) {
			live->fd = fd;
			live->port = port;
			live->token = token;
			live->device = device;
			live->inotifyFD = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
			if (live->inotifyFD < 0)
				live.reset();
		}
	}

	bool success = ScanDirectory(mountPoint, device, *root, fd, 0, live.get());
	if (live != NULL) {
		live->predicate = root;
		if (!StartLiveQuery(live)) {
			close(live->inotifyFD);
			live->predicate = NULL;
			live.reset();
			delete root;
		}
	} else
		delete root;

	if (!success || lseek(fd, 0, SEEK_SET) == (off_t)-1) {
		if (live != NULL) {
			live->stopped = true;
			close(live->inotifyFD);
		}
		close(fd);
		errno = EIO;
		return -1;
	}

	{
		std::lock_guard<std::mutex> guard(QueryLock());
		QueryFDs().insert(fd);
		if (live != NULL)
			LiveQueries()[fd] = live;
	}

	return fd;
}


bool
is_query_fd(int fd)
{
	if (fd < 0)
		return false;

	std::lock_guard<std::mutex> guard(QueryLock());
	return QueryFDs().find(fd) != QueryFDs().end();
}


ssize_t
read_query_dir(int fd, struct dirent* buffer, size_t length, uint32 maxCount)
{
	if (buffer == NULL || !is_query_fd(fd))
		return B_BAD_VALUE;
	if (maxCount == 0)
		return 0;

	off_t recordStart = lseek(fd, 0, SEEK_CUR);
	if (recordStart == (off_t)-1)
		return B_FILE_ERROR;

	ResultRecord record;
	ssize_t bytesRead = read(fd, &record, sizeof(record));
	if (bytesRead == 0)
		return 0;	// end of the result set
	if (bytesRead != (ssize_t)sizeof(record) || record.length == 0
			|| record.length > B_PATH_NAME_LENGTH)
		return B_FILE_ERROR;

	std::string path(record.length, '\0');
	if (read(fd, &path[0], record.length) != (ssize_t)record.length)
		return B_FILE_ERROR;

	size_t entrySize = offsetof(struct dirent, d_name) + record.length + 1;
	if (entrySize > length) {
		// Leave the record in place for a caller with a bigger buffer.
		lseek(fd, recordStart, SEEK_SET);
		return B_BAD_VALUE;
	}

	memset(buffer, 0, entrySize);
	buffer->d_ino = (ino_t)record.directory;
	buffer->d_off = 0;
	buffer->d_reclen = (unsigned short)entrySize;
	buffer->d_type = (unsigned char)record.type;
	memcpy(buffer->d_name, path.c_str(), path.size() + 1);

	return 1;
}


status_t
close_query(int fd)
{
	std::shared_ptr<LiveQuery> live;

	{
		std::lock_guard<std::mutex> guard(QueryLock());
		QueryFDs().erase(fd);

		std::map<int, std::shared_ptr<LiveQuery> >::iterator found
			= LiveQueries().find(fd);
		if (found != LiveQueries().end()) {
			live = found->second;
			LiveQueries().erase(found);
		}
	}

	if (live != NULL) {
		live->stopped = true;
		// Wakes the watcher thread's poll(); it exits on its own.
		close(live->inotifyFD);
		live->inotifyFD = -1;
	}

	return B_OK;
}


#else	// !__linux__

// The engine relies on /proc/mounts and the Linux xattr API. Other platforms
// keep answering queries with "not supported", as before.

int
open_query(dev_t device, const char* predicate, size_t length, uint32 flags,
	port_id port, int32 token)
{
	(void)device;
	(void)predicate;
	(void)length;
	(void)flags;
	(void)port;
	(void)token;

	errno = ENOTSUP;
	return -1;
}


bool
is_query_fd(int fd)
{
	(void)fd;
	return false;
}


ssize_t
read_query_dir(int fd, struct dirent* buffer, size_t length, uint32 maxCount)
{
	(void)fd;
	(void)buffer;
	(void)length;
	(void)maxCount;

	return B_BAD_VALUE;
}


status_t
close_query(int fd)
{
	(void)fd;
	return B_OK;
}


void
set_query_update_sender(QueryUpdateSender sender)
{
	(void)sender;
}

#endif	// __linux__


}	// namespace Query
}	// namespace Storage
}	// namespace BPrivate
