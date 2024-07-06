/*
 * Copyright 2008, Ingo Weinhold, ingo_weinhold@gmx.de. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */


#include "AuthenticationManager.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/param.h>

#include <map>
#include <new>
#include <set>
#include <string>

#include <DataIO.h>
#include <StringList.h>

#include <AutoDeleter.h>
#include <LaunchRoster.h>
#include <RegistrarDefs.h>

#include <libroot_private.h>
#include <user_group.h>
//#include <util/KMessage.h>
#include <errno.h>
#include <string.h>

using std::map;
using std::string;

using namespace BPrivate;


typedef std::set<std::string> StringSet;


class AuthenticationManager::FlatStore {
public:
	FlatStore()
		: fSize(0)
	{
		fBuffer.SetBlockSize(1024);
	}

	void WriteData(size_t offset, const void* data, size_t length)
	{
		ssize_t result = fBuffer.WriteAt(offset, data, length);
		if (result < 0)
			throw status_t(result);
	}

	template<typename Type>
	void WriteData(size_t offset, const Type& data)
	{
		WriteData(&data, sizeof(Type));
	}

	size_t ReserveSpace(size_t length, bool align)
	{
		size_t pos = fSize;
		fSize += length;

		return pos;
	}

	void* AppendData(const void* data, size_t length, bool align)
	{
		size_t pos = ReserveSpace(length, align);
		WriteData(pos, data, length);
		return (void*)(addr_t)pos;
	}

	template<typename Type>
	Type* AppendData(const Type& data)
	{
		return (Type*)AppendData(&data, sizeof(Type), true);
	}

	char* AppendString(const char* string)
	{
		return (char*)AppendData(string, strlen(string) + 1, false);
	}

	char* AppendString(const string& str)
	{
		return (char*)AppendData(str.c_str(), str.length() + 1, false);
	}

	const void* Buffer() const
	{
		return fBuffer.Buffer();
	}

	size_t BufferLength() const
	{
		return fSize;
	}

private:
	BMallocIO	fBuffer;
	size_t		fSize;
};


class AuthenticationManager::User {
public:
	User()
		:
		fUID(0),
		fGID(0),
		fLastChanged(0),
		fMin(-1),
		fMax(-1),
		fWarn(-1),
		fInactive(-1),
		fExpiration(-1),
		fFlags(0)
	{
	}

	User(const char* name, const char* password, uid_t uid, gid_t gid,
		const char* home, const char* shell, const char* realName)
		:
		fUID(uid),
		fGID(gid),
		fName(name),
		fPassword(password),
		fHome(home),
		fShell(shell),
		fRealName(realName),
		fLastChanged(0),
		fMin(-1),
		fMax(-1),
		fWarn(-1),
		fInactive(-1),
		fExpiration(-1),
		fFlags(0)
	{
	}

	User(const User& other)
		:
		fUID(other.fUID),
		fGID(other.fGID),
		fName(other.fName),
		fPassword(other.fPassword),
		fHome(other.fHome),
		fShell(other.fShell),
		fRealName(other.fRealName),
		fShadowPassword(other.fShadowPassword),
		fLastChanged(other.fLastChanged),
		fMin(other.fMin),
		fMax(other.fMax),
		fWarn(other.fWarn),
		fInactive(other.fInactive),
		fExpiration(other.fExpiration),
		fFlags(other.fFlags)
	{
	}

	const string& Name() const	{ return fName; }
	const uid_t UID() const		{ return fUID; }

	void SetShadowInfo(const char* password, int lastChanged, int min, int max,
		int warn, int inactive, int expiration, int flags)
	{
		fShadowPassword = password;
		fLastChanged = lastChanged;
		fMin = min;
		fMax = max;
		fWarn = warn;
		fInactive = inactive;
		fExpiration = expiration;
		fFlags = flags;
	}

#if 0
	void UpdateFromMessage(const KMessage& message)
	{
		int32 intValue;
		const char* stringValue;

		if (message.FindInt32("uid", &intValue) == B_OK)
			fUID = intValue;

		if (message.FindInt32("gid", &intValue) == B_OK)
			fGID = intValue;

		if (message.FindString("name", &stringValue) == B_OK)
			fName = stringValue;

		if (message.FindString("password", &stringValue) == B_OK)
			fPassword = stringValue;

		if (message.FindString("home", &stringValue) == B_OK)
			fHome = stringValue;

		if (message.FindString("shell", &stringValue) == B_OK)
			fShell = stringValue;

		if (message.FindString("real name", &stringValue) == B_OK)
			fRealName = stringValue;

		if (message.FindString("shadow password", &stringValue) == B_OK) {
			fShadowPassword = stringValue;
			// TODO:
			// fLastChanged = now;
		}

		if (message.FindInt32("last changed", &intValue) == B_OK)
			fLastChanged = intValue;

		if (message.FindInt32("min", &intValue) == B_OK)
			fMin = intValue;

		if (message.FindInt32("max", &intValue) == B_OK)
			fMax = intValue;

		if (message.FindInt32("warn", &intValue) == B_OK)
			fWarn = intValue;

		if (message.FindInt32("inactive", &intValue) == B_OK)
			fInactive = intValue;

		if (message.FindInt32("expiration", &intValue) == B_OK)
			fExpiration = intValue;

		if (message.FindInt32("flags", &intValue) == B_OK)
			fFlags = intValue;
	}
#endif

	passwd* WriteFlatPasswd(FlatStore& store) const
	{
		struct passwd passwd;

		passwd.pw_uid = fUID;
		passwd.pw_gid = fGID;
		passwd.pw_name = store.AppendString(fName);
		passwd.pw_passwd = store.AppendString(fPassword);
		passwd.pw_dir = store.AppendString(fHome);
		passwd.pw_shell = store.AppendString(fShell);
		passwd.pw_gecos = store.AppendString(fRealName);

		return store.AppendData(passwd);
	}

	spwd* WriteFlatShadowPwd(FlatStore& store) const
	{
		struct spwd spwd;

		spwd.sp_namp = store.AppendString(fName);
		spwd.sp_pwdp = store.AppendString(fShadowPassword);
		spwd.sp_lstchg = fLastChanged;
		spwd.sp_min = fMin;
		spwd.sp_max = fMax;
		spwd.sp_warn = fWarn;
		spwd.sp_inact = fInactive;
		spwd.sp_expire = fExpiration;
		spwd.sp_flag = fFlags;

		return store.AppendData(spwd);
	}

#if 0
	status_t WriteToMessage(KMessage& message, bool addShadowPwd)
	{
		status_t error;
		if ((error = message.AddInt32("uid", fUID)) != B_OK
			|| (error = message.AddInt32("gid", fGID)) != B_OK
			|| (error = message.AddString("name", fName.c_str())) != B_OK
			|| (error = message.AddString("password", fPassword.c_str()))
					!= B_OK
			|| (error = message.AddString("home", fHome.c_str())) != B_OK
			|| (error = message.AddString("shell", fShell.c_str())) != B_OK
			|| (error = message.AddString("real name", fRealName.c_str()))
					!= B_OK) {
			return error;
		}

		if (!addShadowPwd)
			return B_OK;

		if ((error = message.AddString("shadow password",
					fShadowPassword.c_str())) != B_OK
			|| (error = message.AddInt32("last changed", fLastChanged)) != B_OK
			|| (error = message.AddInt32("min", fMin)) != B_OK
			|| (error = message.AddInt32("max", fMax)) != B_OK
			|| (error = message.AddInt32("warn", fWarn)) != B_OK
			|| (error = message.AddInt32("inactive", fInactive)) != B_OK
			|| (error = message.AddInt32("expiration", fExpiration)) != B_OK
			|| (error = message.AddInt32("flags", fFlags)) != B_OK) {
			return error;
		}

		return B_OK;
	}
#endif

	void WritePasswdLine(FILE* file)
	{
		fprintf(file, "%s:%s:%d:%d:%s:%s:%s\n",
			fName.c_str(), fPassword.c_str(), (int)fUID, (int)fGID,
			fRealName.c_str(), fHome.c_str(), fShell.c_str());
	}

	void WriteShadowPwdLine(FILE* file)
	{
		fprintf(file, "%s:%s:%d:", fName.c_str(), fShadowPassword.c_str(),
			fLastChanged);

		// The following values are supposed to be printed as empty strings,
		// if negative.
		int values[5] = { fMin, fMax, fWarn, fInactive, fExpiration };
		for (int i = 0; i < 5; i++) {
			if (values[i] >= 0)
				fprintf(file, "%d", values[i]);
			fprintf(file, ":");
		}

		fprintf(file, "%d\n", fFlags);
	}

private:
	uid_t	fUID;
	gid_t	fGID;
	string	fName;
	string	fPassword;
	string	fHome;
	string	fShell;
	string	fRealName;
	string	fShadowPassword;
	int		fLastChanged;
	int		fMin;
	int		fMax;
	int		fWarn;
	int		fInactive;
	int		fExpiration;
	int		fFlags;
};


class AuthenticationManager::Group {
public:
	Group()
		:
		fGID(0),
		fName(),
		fPassword(),
		fMembers()
	{
	}

	Group(const char* name, const char* password, gid_t gid,
		const char* const* members, int memberCount)
		:
		fGID(gid),
		fName(name),
		fPassword(password),
		fMembers()
	{
		for (int i = 0; i < memberCount; i++)
			fMembers.insert(members[i]);
	}

	~Group()
	{
	}

	const string& Name() const	{ return fName; }
	const gid_t GID() const		{ return fGID; }

	bool HasMember(const char* name)
	{
		try {
			return fMembers.find(name) != fMembers.end();
		} catch (...) {
			return false;
		}
	}

	bool MemberRemoved(const std::string& name)
	{
		return fMembers.erase(name) > 0;
	}

#if 0
	void UpdateFromMessage(const KMessage& message)
	{
		int32 intValue;
		if (message.FindInt32("gid", &intValue) == B_OK)
			fGID = intValue;

		const char* stringValue;
		if (message.FindString("name", &stringValue) == B_OK)
			fName = stringValue;

		if (message.FindString("password", &stringValue) == B_OK)
			fPassword = stringValue;

		if (message.FindString("members", &stringValue) == B_OK) {
			fMembers.clear();
			for (int32 i = 0;
				(stringValue = message.GetString("members", i, NULL)) != NULL;
				i++) {
				if (stringValue != NULL && *stringValue != '\0')
					fMembers.insert(stringValue);
			}
		}
	}
#endif

	group* WriteFlatGroup(FlatStore& store) const
	{
		struct group group;

		char* members[MAX_GROUP_MEMBER_COUNT + 1];
		int32 count = 0;
		for (StringSet::const_iterator it = fMembers.begin();
			it != fMembers.end(); ++it) {
			members[count++] = store.AppendString(it->c_str());
		}
		members[count] = (char*)-1;

		group.gr_gid = fGID;
		group.gr_name = store.AppendString(fName);
		group.gr_passwd = store.AppendString(fPassword);
		group.gr_mem = (char**)store.AppendData(members,
			sizeof(char*) * (count + 1), true);

		return store.AppendData(group);
	}

#if 0
	status_t WriteToMessage(KMessage& message)
	{
		status_t error;
		if ((error = message.AddInt32("gid", fGID)) != B_OK
			|| (error = message.AddString("name", fName.c_str())) != B_OK
			|| (error = message.AddString("password", fPassword.c_str()))
					!= B_OK) {
			return error;
		}

		for (StringSet::const_iterator it = fMembers.begin();
			it != fMembers.end(); ++it) {
			if ((error = message.AddString("members", it->c_str())) != B_OK)
				return error;
		}

		return B_OK;
	}
#endif

private:
	gid_t		fGID;
	string		fName;
	string		fPassword;
	StringSet	fMembers;
};


class AuthenticationManager::UserDB {
public:
	status_t AddUser(User* user)
	{
		try {
			fUsersByID[user->UID()] = user;
		} catch (...) {
			return B_NO_MEMORY;
		}

		try {
			fUsersByName[user->Name()] = user;
		} catch (...) {
			fUsersByID.erase(fUsersByID.find(user->UID()));
			return B_NO_MEMORY;
		}

		return B_OK;
	}

	void RemoveUser(User* user)
	{
		fUsersByID.erase(fUsersByID.find(user->UID()));
		fUsersByName.erase(fUsersByName.find(user->Name()));
	}

	User* UserByID(uid_t uid) const
	{
		map<uid_t, User*>::const_iterator it = fUsersByID.find(uid);
		return (it == fUsersByID.end() ? NULL : it->second);
	}

	User* UserByName(const char* name) const
	{
		map<string, User*>::const_iterator it = fUsersByName.find(name);
		return (it == fUsersByName.end() ? NULL : it->second);
	}

	int32 WriteFlatPasswdDB(FlatStore& store) const
	{
		int32 count = fUsersByID.size();

		size_t entriesSpace = sizeof(passwd*) * count;
		size_t offset = store.ReserveSpace(entriesSpace, true);
		passwd** entries = new passwd*[count];
		ArrayDeleter<passwd*> _(entries);

		int32 index = 0;
		for (map<uid_t, User*>::const_iterator it = fUsersByID.begin();
			 it != fUsersByID.end(); ++it) {
			entries[index++] = it->second->WriteFlatPasswd(store);
		}

		store.WriteData(offset, entries, entriesSpace);

		return count;
	}

	int32 WriteFlatShadowDB(FlatStore& store) const
	{
		int32 count = fUsersByID.size();

		size_t entriesSpace = sizeof(spwd*) * count;
		size_t offset = store.ReserveSpace(entriesSpace, true);
		spwd** entries = new spwd*[count];
		ArrayDeleter<spwd*> _(entries);

		int32 index = 0;
		for (map<uid_t, User*>::const_iterator it = fUsersByID.begin();
			 it != fUsersByID.end(); ++it) {
			entries[index++] = it->second->WriteFlatShadowPwd(store);
		}

		store.WriteData(offset, entries, entriesSpace);

		return count;
	}

	void WriteToDisk()
	{
		// rename the old files
		string passwdBackup(kPasswdFile);
		string shadowBackup(kShadowPwdFile);
		passwdBackup += ".old";
		shadowBackup += ".old";

		rename(kPasswdFile, passwdBackup.c_str());
		rename(kShadowPwdFile, shadowBackup.c_str());
			// Don't check errors. We can't do anything anyway.

		// open files
		FILE* passwdFile = fopen(kPasswdFile, "w");
		if (passwdFile == NULL) {
			debug_printf("REG: Failed to open passwd file \"%s\" for "
				"writing: %s\n", kPasswdFile, strerror(errno));
		}
		CObjectDeleter<FILE, int> _1(passwdFile, fclose);

		FILE* shadowFile = fopen(kShadowPwdFile, "w");
		if (shadowFile == NULL) {
			debug_printf("REG: Failed to open shadow passwd file \"%s\" for "
				"writing: %s\n", kShadowPwdFile, strerror(errno));
		}
		CObjectDeleter<FILE, int> _2(shadowFile, fclose);

		// write users
		for (map<uid_t, User*>::const_iterator it = fUsersByID.begin();
			 it != fUsersByID.end(); ++it) {
			User* user = it->second;
			user->WritePasswdLine(passwdFile);
			user->WriteShadowPwdLine(shadowFile);
		}
	}

private:
	map<uid_t, User*>	fUsersByID;
	map<string, User*>	fUsersByName;
};


class AuthenticationManager::GroupDB {
public:
	status_t AddGroup(Group* group)
	{
		try {
			fGroupsByID[group->GID()] = group;
		} catch (...) {
			return B_NO_MEMORY;
		}

		try {
			fGroupsByName[group->Name()] = group;
		} catch (...) {
			fGroupsByID.erase(fGroupsByID.find(group->GID()));
			return B_NO_MEMORY;
		}

		return B_OK;
	}

	void RemoveGroup(Group* group)
	{
		fGroupsByID.erase(fGroupsByID.find(group->GID()));
		fGroupsByName.erase(fGroupsByName.find(group->Name()));
	}

	bool UserRemoved(const std::string& user)
	{
		bool changed = false;
		for (map<gid_t, Group*>::const_iterator it = fGroupsByID.begin();
			 it != fGroupsByID.end(); ++it) {
			Group* group = it->second;
			changed |= group->MemberRemoved(user);
		}
		return changed;
	}

	Group* GroupByID(gid_t gid) const
	{
		map<gid_t, Group*>::const_iterator it = fGroupsByID.find(gid);
		return (it == fGroupsByID.end() ? NULL : it->second);
	}

	Group* GroupByName(const char* name) const
	{
		map<string, Group*>::const_iterator it = fGroupsByName.find(name);
		return (it == fGroupsByName.end() ? NULL : it->second);
	}

	int32 GetUserGroups(const char* name, gid_t* groups, int maxCount)
	{
		int count = 0;

		for (map<gid_t, Group*>::const_iterator it = fGroupsByID.begin();
			 it != fGroupsByID.end(); ++it) {
			Group* group = it->second;
			if (group->HasMember(name)) {
				if (count < maxCount)
					groups[count] = group->GID();
				count++;
			}
		}

		return count;
	}


	int32 WriteFlatGroupDB(FlatStore& store) const
	{
		int32 count = fGroupsByID.size();

		size_t entriesSpace = sizeof(group*) * count;
		size_t offset = store.ReserveSpace(entriesSpace, true);
		group** entries = new group*[count];
		ArrayDeleter<group*> _(entries);

		int32 index = 0;
		for (map<gid_t, Group*>::const_iterator it = fGroupsByID.begin();
			 it != fGroupsByID.end(); ++it) {
			entries[index++] = it->second->WriteFlatGroup(store);
		}

		store.WriteData(offset, entries, entriesSpace);

		return count;
	}

	void WriteToDisk()
	{
		// rename the old files
		string groupBackup(kGroupFile);
		groupBackup += ".old";

		rename(kGroupFile, groupBackup.c_str());
			// Don't check errors. We can't do anything anyway.

		// open file
		FILE* groupFile = fopen(kGroupFile, "w");
		if (groupFile == NULL) {
			debug_printf("REG: Failed to open group file \"%s\" for "
				"writing: %s\n", kGroupFile, strerror(errno));
		}
		CObjectDeleter<FILE, int> _1(groupFile, fclose);

		// write groups
		for (map<gid_t, Group*>::const_iterator it = fGroupsByID.begin();
			it != fGroupsByID.end(); ++it) {
			Group* group = it->second;
			//group->WriteGroupLine(groupFile);
		}
	}

private:
	map<uid_t, Group*>	fGroupsByID;
	map<string, Group*>	fGroupsByName;
};


AuthenticationManager::AuthenticationManager()
	:
	fRequestPort(-1),
	fRequestThread(-1),
	fUserDB(NULL),
	fGroupDB(NULL)
{
}


AuthenticationManager::~AuthenticationManager()
{
	// Quit the request thread and wait for it to finish
	write_port(fRequestPort, 'quit', NULL, 0);
	wait_for_thread(fRequestThread, NULL);

	delete fUserDB;
	delete fGroupDB;
}


status_t
AuthenticationManager::Init()
{
	fUserDB = new(std::nothrow) UserDB;
	fGroupDB = new(std::nothrow) GroupDB;

	if (fUserDB == NULL || fGroupDB == NULL) {
		return B_NO_MEMORY;
	}

	fRequestPort = create_port(100,
		B_REGISTRAR_AUTHENTICATION_PORT_NAME);
	if (fRequestPort < 0)
		return fRequestPort;

	fRequestThread = spawn_thread(&_RequestThreadEntry,
		"authentication manager", B_NORMAL_PRIORITY + 1, this);
	if (fRequestThread < 0)
		return fRequestThread;

	resume_thread(fRequestThread);

	return B_OK;
}


status_t
AuthenticationManager::_RequestThreadEntry(void* data)
{
	return ((AuthenticationManager*)data)->_RequestThread();
}


status_t
AuthenticationManager::_RequestThread()
{
	// read the DB files
	_InitPasswdDB();
	_InitGroupDB();
	_InitShadowPwdDB();

    // get our team ID
	team_id registrarTeam = -1;
	{
		thread_info info;
		if (get_thread_info(find_thread(NULL), &info) == B_OK)
			registrarTeam = info.team;
	}

	// FIXME real thread stuff goes here
}


status_t
AuthenticationManager::_InitPasswdDB()
{
	return B_OK;
}


status_t
AuthenticationManager::_InitGroupDB()
{
	return B_OK;
}


status_t
AuthenticationManager::_InitShadowPwdDB()
{
	return B_OK;
}
