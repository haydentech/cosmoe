/*
 * Copyright 2011, Axel Dörfler, axeld@pinc-software.de.
 * Distributed under the terms of the MIT License.
 */


#include <KeyStore.h>

#include <KeyStoreDefs.h>

#include <Directory.h>
#include <File.h>
#include <FindDirectory.h>
#include <Path.h>

#include <map>
#include <mutex>
#include <set>
#include <string>


using namespace BPrivate;


namespace {


const char* kMasterKeyringName = "Master";
const uint32 kLocalKeyStoreFormatVersion = 1;


struct LocalKeyring {
	LocalKeyring()
		:
		hasUnlockKey(false),
		unlocked(true)
	{
	}

	explicit LocalKeyring(const std::string& keyringName)
		:
		name(keyringName),
		hasUnlockKey(false),
		unlocked(true)
	{
	}

	std::string	name;
	bool		hasUnlockKey;
	bool		unlocked;
	BMessage	unlockKey;
	BMessage	data;
	BMessage	applications;
};


class LocalKeyStore {
public:
	static LocalKeyStore& Instance()
	{
		static LocalKeyStore sInstance;
		return sInstance;
	}

	status_t Dispatch(BMessage& message, BMessage* reply)
	{
		std::lock_guard<std::mutex> locker(fLock);

		status_t result = _EnsureLoaded();
		if (result != B_OK)
			return result;

		BMessage localReply;
		if (reply == NULL)
			reply = &localReply;

		reply->MakeEmpty();

		switch (message.what) {
			case KEY_STORE_GET_KEY:
			{
				LocalKeyring* keyring = _MessageKeyring(message);
				if (keyring == NULL)
					return B_BAD_VALUE;

				const char* identifier = NULL;
				if (message.FindString("identifier", &identifier) != B_OK)
					return B_BAD_VALUE;

				bool secondaryIdentifierOptional = false;
				if (message.FindBool("secondaryIdentifierOptional",
						&secondaryIdentifierOptional) != B_OK) {
					secondaryIdentifierOptional = false;
				}

				BString secondaryIdentifier;
				if (message.FindString("secondaryIdentifier", &secondaryIdentifier)
						!= B_OK) {
					secondaryIdentifier = "";
					secondaryIdentifierOptional = true;
				}

				_Unlock(*keyring);

				BMessage keyMessage;
				result = _FindKey(*keyring, identifier,
					secondaryIdentifier.String(), secondaryIdentifierOptional,
					&keyMessage);
				if (result == B_OK)
					reply->AddMessage("key", &keyMessage);
				return result;
			}

			case KEY_STORE_ADD_KEY:
			{
				LocalKeyring* keyring = _MessageKeyring(message);
				if (keyring == NULL)
					return B_BAD_VALUE;

				BMessage keyMessage;
				BString identifier;
				if (message.FindMessage("key", &keyMessage) != B_OK
					|| keyMessage.FindString("identifier", &identifier) != B_OK) {
					return B_BAD_VALUE;
				}

				BString secondaryIdentifier;
				if (keyMessage.FindString("secondaryIdentifier",
						&secondaryIdentifier) != B_OK) {
					secondaryIdentifier = "";
				}

				_Unlock(*keyring);

				result = _FindKey(*keyring, identifier.String(),
					secondaryIdentifier.String(), false, NULL);
				if (result == B_OK)
					return B_NAME_IN_USE;
				if (result != B_ENTRY_NOT_FOUND)
					return result;

				result = keyring->data.AddMessage(identifier.String(), &keyMessage);
				if (result != B_OK)
					return result;

				return _SaveLocked();
			}

			case KEY_STORE_REMOVE_KEY:
			{
				LocalKeyring* keyring = _MessageKeyring(message);
				if (keyring == NULL)
					return B_BAD_VALUE;

				BMessage keyMessage;
				BString identifier;
				if (message.FindMessage("key", &keyMessage) != B_OK
					|| keyMessage.FindString("identifier", &identifier) != B_OK) {
					return B_BAD_VALUE;
				}

				_Unlock(*keyring);

				int32 count = 0;
				type_code type = B_ANY_TYPE;
				if (keyring->data.GetInfo(identifier.String(), &type, &count)
						!= B_OK) {
					return B_ENTRY_NOT_FOUND;
				}

				for (int32 i = 0; i < count; i++) {
					BMessage candidate;
					if (keyring->data.FindMessage(identifier.String(), i, &candidate)
							!= B_OK) {
						return B_ERROR;
					}

					if (!candidate.HasSameData(keyMessage))
						continue;

					result = keyring->data.RemoveData(identifier.String(), i);
					if (result != B_OK)
						return result;

					return _SaveLocked();
				}

				return B_ENTRY_NOT_FOUND;
			}

			case KEY_STORE_GET_NEXT_KEY:
			{
				LocalKeyring* keyring = _MessageKeyring(message);
				if (keyring == NULL)
					return B_BAD_VALUE;

				BKeyType type;
				BKeyPurpose purpose;
				uint32 cookie;
				if (message.FindUInt32("type", (uint32*)&type) != B_OK
					|| message.FindUInt32("purpose", (uint32*)&purpose) != B_OK
					|| message.FindUInt32("cookie", &cookie) != B_OK) {
					return B_BAD_VALUE;
				}

				_Unlock(*keyring);

				BMessage keyMessage;
				result = _FindKeyByIndex(*keyring, type, purpose, cookie,
					keyMessage);
				if (result != B_OK)
					return result;

				reply->AddUInt32("cookie", cookie + 1);
				reply->AddMessage("key", &keyMessage);
				return B_OK;
			}

			case KEY_STORE_ADD_KEYRING:
			{
				BString keyringName;
				if (message.FindString("keyring", &keyringName) != B_OK)
					return B_BAD_VALUE;

				std::string canonical = _CanonicalKeyringName(
					keyringName.String());
				if (fKeyrings.find(canonical) != fKeyrings.end())
					return B_NAME_IN_USE;

				fKeyrings.insert(std::make_pair(canonical,
					LocalKeyring(canonical)));
				return _SaveLocked();
			}

			case KEY_STORE_REMOVE_KEYRING:
			{
				BString keyringName;
				if (message.FindString("keyring", &keyringName) != B_OK)
					keyringName = "";

				std::string canonical = _CanonicalKeyringName(
					keyringName.String());
				if (canonical == kMasterKeyringName)
					return B_NOT_ALLOWED;

				std::map<std::string, LocalKeyring>::iterator it
					= fKeyrings.find(canonical);
				if (it == fKeyrings.end())
					return B_ENTRY_NOT_FOUND;

				fKeyrings.erase(it);
				fMasterKeyrings.erase(canonical);
				return _SaveLocked();
			}

			case KEY_STORE_GET_NEXT_KEYRING:
			{
				uint32 cookie;
				if (message.FindUInt32("cookie", &cookie) != B_OK)
					return B_BAD_VALUE;

				if (cookie >= fKeyrings.size())
					return B_ENTRY_NOT_FOUND;

				std::map<std::string, LocalKeyring>::const_iterator it
					= fKeyrings.begin();
				std::advance(it, cookie);

				reply->AddUInt32("cookie", cookie + 1);
				reply->AddString("keyring", it->second.name.c_str());
				return B_OK;
			}

			case KEY_STORE_SET_UNLOCK_KEY:
			{
				LocalKeyring* keyring = _MessageKeyring(message);
				if (keyring == NULL)
					return B_BAD_VALUE;

				BMessage keyMessage;
				if (message.FindMessage("key", &keyMessage) != B_OK)
					return B_BAD_VALUE;

				keyring->hasUnlockKey = true;
				keyring->unlockKey = keyMessage;
				keyring->unlocked = true;
				return _SaveLocked();
			}

			case KEY_STORE_REMOVE_UNLOCK_KEY:
			{
				LocalKeyring* keyring = _MessageKeyring(message);
				if (keyring == NULL)
					return B_BAD_VALUE;

				keyring->hasUnlockKey = false;
				keyring->unlockKey.MakeEmpty();
				keyring->unlocked = true;
				return _SaveLocked();
			}

			case KEY_STORE_ADD_KEYRING_TO_MASTER:
			{
				LocalKeyring* keyring = _MessageKeyring(message);
				if (keyring == NULL)
					return B_BAD_VALUE;

				fMasterKeyrings.insert(keyring->name);
				return _SaveLocked();
			}

			case KEY_STORE_REMOVE_KEYRING_FROM_MASTER:
			{
				LocalKeyring* keyring = _MessageKeyring(message);
				if (keyring == NULL)
					return B_BAD_VALUE;

				if (fMasterKeyrings.erase(keyring->name) == 0)
					return B_ENTRY_NOT_FOUND;

				return _SaveLocked();
			}

			case KEY_STORE_GET_NEXT_MASTER_KEYRING:
			{
				uint32 cookie;
				if (message.FindUInt32("cookie", &cookie) != B_OK)
					return B_BAD_VALUE;

				if (cookie >= fMasterKeyrings.size())
					return B_ENTRY_NOT_FOUND;

				std::set<std::string>::const_iterator it
					= fMasterKeyrings.begin();
				std::advance(it, cookie);

				reply->AddUInt32("cookie", cookie + 1);
				reply->AddString("keyring", it->c_str());
				return B_OK;
			}

			case KEY_STORE_IS_KEYRING_UNLOCKED:
			{
				LocalKeyring* keyring = _MessageKeyring(message);
				if (keyring == NULL)
					return B_BAD_VALUE;

				reply->AddBool("unlocked", keyring->unlocked);
				return B_OK;
			}

			case KEY_STORE_LOCK_KEYRING:
			{
				LocalKeyring* keyring = _MessageKeyring(message);
				if (keyring == NULL)
					return B_BAD_VALUE;

				keyring->unlocked = false;
				return B_OK;
			}

			case KEY_STORE_GET_NEXT_APPLICATION:
			{
				LocalKeyring* keyring = _MessageKeyring(message);
				if (keyring == NULL)
					return B_BAD_VALUE;

				uint32 cookie;
				if (message.FindUInt32("cookie", &cookie) != B_OK)
					return B_BAD_VALUE;

				char* signature = NULL;
				if (keyring->applications.GetInfo(B_MESSAGE_TYPE, cookie,
						&signature, NULL) != B_OK) {
					return B_ENTRY_NOT_FOUND;
				}

				reply->AddUInt32("cookie", cookie + 1);
				reply->AddString("signature", signature);

				BMessage appMessage;
				if (keyring->applications.FindMessage(signature, 0, &appMessage)
						== B_OK) {
					BString path;
					if (appMessage.FindString("path", &path) == B_OK)
						reply->AddString("path", path);
				}

				return B_OK;
			}

			case KEY_STORE_REMOVE_APPLICATION:
			{
				LocalKeyring* keyring = _MessageKeyring(message);
				if (keyring == NULL)
					return B_BAD_VALUE;

				const char* signature = NULL;
				if (message.FindString("signature", &signature) != B_OK)
					return B_BAD_VALUE;

				const char* path = NULL;
				message.FindString("path", &path);

				if (path == NULL) {
					result = keyring->applications.RemoveName(signature);
					if (result != B_OK)
						return B_ENTRY_NOT_FOUND;
					return _SaveLocked();
				}

				int32 count = 0;
				type_code type = B_ANY_TYPE;
				if (keyring->applications.GetInfo(signature, &type, &count)
						!= B_OK) {
					return B_ENTRY_NOT_FOUND;
				}

				for (int32 i = 0; i < count; i++) {
					BMessage appMessage;
					if (keyring->applications.FindMessage(signature, i, &appMessage)
							!= B_OK) {
						return B_ERROR;
					}

					BString appPath;
					if (appMessage.FindString("path", &appPath) != B_OK)
						continue;

					if (appPath != path)
						continue;

					result = keyring->applications.RemoveData(signature, i);
					if (result != B_OK)
						return result;

					return _SaveLocked();
				}

				return B_ENTRY_NOT_FOUND;
			}
		}

		return B_UNSUPPORTED;
	}

private:
	LocalKeyStore()
		:
		fLoaded(false)
	{
	}

	status_t _EnsureLoaded()
	{
		if (fLoaded)
			return B_OK;

		status_t result = _LoadLocked();
		_EnsureMasterLocked();
		fLoaded = true;

		if (result == B_OK || result == B_ENTRY_NOT_FOUND)
			return B_OK;

		return result;
	}

	void _EnsureMasterLocked()
	{
		if (fKeyrings.find(kMasterKeyringName) == fKeyrings.end())
			fKeyrings.insert(std::make_pair(std::string(kMasterKeyringName),
				LocalKeyring(kMasterKeyringName)));
	}

	LocalKeyring* _MessageKeyring(const BMessage& message)
	{
		BString keyringName;
		if (message.FindString("keyring", &keyringName) != B_OK)
			keyringName = "";

		std::map<std::string, LocalKeyring>::iterator it
			= fKeyrings.find(_CanonicalKeyringName(keyringName.String()));
		if (it == fKeyrings.end())
			return NULL;

		return &it->second;
	}

	status_t _LoadLocked()
	{
		fKeyrings.clear();
		fMasterKeyrings.clear();

		BPath path;
		status_t result = _GetStorePath(path, false);
		if (result != B_OK)
			return result;

		BFile file(path.Path(), B_READ_ONLY);
		result = file.InitCheck();
		if (result != B_OK)
			return result;

		BMessage archive;
		result = archive.Unflatten(&file);
		if (result != B_OK)
			return result;

		uint32 format = 0;
		if (archive.FindUInt32("format", &format) != B_OK
			|| format != kLocalKeyStoreFormatVersion) {
			return B_BAD_DATA;
		}

		for (int32 i = 0;; i++) {
			BMessage keyringArchive;
			if (archive.FindMessage("keyring", i, &keyringArchive) != B_OK)
				break;

			LocalKeyring keyring;
			BString name;
			if (keyringArchive.FindString("name", &name) != B_OK)
				continue;

			keyring.name = name.String();
			if (keyringArchive.FindBool("hasUnlockKey", &keyring.hasUnlockKey)
					!= B_OK) {
				keyring.hasUnlockKey = false;
			}

			if (keyringArchive.FindBool("unlocked", &keyring.unlocked) != B_OK)
				keyring.unlocked = true;

			keyringArchive.FindMessage("unlockKey", &keyring.unlockKey);
			keyringArchive.FindMessage("data", &keyring.data);
			keyringArchive.FindMessage("applications", &keyring.applications);

			fKeyrings[keyring.name] = keyring;
		}

		for (int32 i = 0;; i++) {
			BString name;
			if (archive.FindString("masterKeyring", i, &name) != B_OK)
				break;

			fMasterKeyrings.insert(name.String());
		}

		return B_OK;
	}

	status_t _SaveLocked()
	{
		BPath path;
		status_t result = _GetStorePath(path, true);
		if (result != B_OK)
			return result;

		BFile file(path.Path(), B_READ_WRITE | B_CREATE_FILE);
		result = file.InitCheck();
		if (result != B_OK)
			return result;

		BMessage archive;
		result = archive.AddUInt32("format", kLocalKeyStoreFormatVersion);
		if (result != B_OK)
			return result;

		for (std::map<std::string, LocalKeyring>::const_iterator it
				= fKeyrings.begin(); it != fKeyrings.end(); ++it) {
			BMessage keyringArchive;
			result = keyringArchive.AddString("name", it->second.name.c_str());
			if (result != B_OK)
				return result;

			result = keyringArchive.AddBool("hasUnlockKey",
				it->second.hasUnlockKey);
			if (result != B_OK)
				return result;

			result = keyringArchive.AddBool("unlocked", it->second.unlocked);
			if (result != B_OK)
				return result;

			if (!it->second.unlockKey.IsEmpty()) {
				result = keyringArchive.AddMessage("unlockKey",
					&it->second.unlockKey);
				if (result != B_OK)
					return result;
			}

			result = keyringArchive.AddMessage("data", &it->second.data);
			if (result != B_OK)
				return result;

			result = keyringArchive.AddMessage("applications",
				&it->second.applications);
			if (result != B_OK)
				return result;

			result = archive.AddMessage("keyring", &keyringArchive);
			if (result != B_OK)
				return result;
		}

		for (std::set<std::string>::const_iterator it = fMasterKeyrings.begin();
				it != fMasterKeyrings.end(); ++it) {
			result = archive.AddString("masterKeyring", it->c_str());
			if (result != B_OK)
				return result;
		}

		result = file.SetSize(0);
		if (result != B_OK)
			return result;

		result = file.Seek(0, SEEK_SET);
		if (result < B_OK)
			return result;

		return archive.Flatten(&file);
	}

	static status_t _GetStorePath(BPath& path, bool createParents)
	{
		status_t result = find_directory(B_USER_SETTINGS_DIRECTORY, &path);
		if (result != B_OK)
			return result;

		path.Append("system");
		if (createParents) {
			result = create_directory(path.Path(), 0755);
			if (result != B_OK && result != B_FILE_EXISTS)
				return result;
		}

		path.Append("keystore");
		if (createParents) {
			result = create_directory(path.Path(), 0755);
			if (result != B_OK && result != B_FILE_EXISTS)
				return result;
		}

		return path.Append("keystore_database");
	}

	static void _Unlock(LocalKeyring& keyring)
	{
		keyring.unlocked = true;
	}

	static std::string _CanonicalKeyringName(const char* name)
	{
		if (name == NULL || name[0] == '\0' || strcmp(name, kMasterKeyringName) == 0)
			return kMasterKeyringName;

		return name;
	}

	static status_t _FindKey(const LocalKeyring& keyring,
		const char* identifier, const char* secondaryIdentifier,
		bool secondaryIdentifierOptional, BMessage* foundKeyMessage)
	{
		int32 count = 0;
		type_code type = B_ANY_TYPE;
		if (keyring.data.GetInfo(identifier, &type, &count) != B_OK)
			return B_ENTRY_NOT_FOUND;

		for (int32 i = 0; i < count; i++) {
			BMessage candidate;
			if (keyring.data.FindMessage(identifier, i, &candidate) != B_OK)
				return B_ERROR;

			BString candidateIdentifier;
			if (candidate.FindString("secondaryIdentifier",
					&candidateIdentifier) != B_OK) {
				candidateIdentifier = "";
			}

			if (candidateIdentifier != secondaryIdentifier)
				continue;

			if (foundKeyMessage != NULL)
				*foundKeyMessage = candidate;
			return B_OK;
		}

		if (!secondaryIdentifierOptional)
			return B_ENTRY_NOT_FOUND;

		if (foundKeyMessage == NULL)
			return B_OK;

		return keyring.data.FindMessage(identifier, 0, foundKeyMessage);
	}

	static bool _KeyMatches(const BMessage& keyMessage, BKeyType type,
		BKeyPurpose purpose)
	{
		if (type != B_KEY_TYPE_ANY) {
			BKeyType keyType;
			if (keyMessage.FindUInt32("type", (uint32*)&keyType) != B_OK)
				return false;

			if (keyType != type)
				return false;
		}

		if (purpose != B_KEY_PURPOSE_ANY) {
			BKeyPurpose keyPurpose;
			if (keyMessage.FindUInt32("purpose", (uint32*)&keyPurpose)
					!= B_OK) {
				return false;
			}

			if (keyPurpose != purpose)
				return false;
		}

		return true;
	}

	static status_t _FindKeyByIndex(const LocalKeyring& keyring, BKeyType type,
		BKeyPurpose purpose, uint32 index, BMessage& foundKeyMessage)
	{
		for (int32 keyIndex = 0;; keyIndex++) {
			int32 count = 0;
			char* identifier = NULL;
			if (keyring.data.GetInfo(B_MESSAGE_TYPE, keyIndex, &identifier, NULL,
					&count) != B_OK) {
				break;
			}

			if (type == B_KEY_TYPE_ANY && purpose == B_KEY_PURPOSE_ANY) {
				if ((int32)index >= count) {
					index -= count;
					continue;
				}

				return keyring.data.FindMessage(identifier, index, &foundKeyMessage);
			}

			for (int32 subkeyIndex = 0; subkeyIndex < count; subkeyIndex++) {
				BMessage subkey;
				if (keyring.data.FindMessage(identifier, subkeyIndex, &subkey)
						!= B_OK) {
					return B_ERROR;
				}

				if (!_KeyMatches(subkey, type, purpose))
					continue;

				if (index == 0) {
					foundKeyMessage = subkey;
					return B_OK;
				}

				index--;
			}
		}

		return B_ENTRY_NOT_FOUND;
	}

	std::mutex						fLock;
	bool							fLoaded;
	std::map<std::string, LocalKeyring>	fKeyrings;
	std::set<std::string>				fMasterKeyrings;
};


} // namespace


BKeyStore::BKeyStore()
{
}


BKeyStore::~BKeyStore()
{
}


// #pragma mark - Key handling


status_t
BKeyStore::GetKey(BKeyType type, const char* identifier, BKey& key)
{
	return GetKey(NULL, type, identifier, NULL, true, key);
}


status_t
BKeyStore::GetKey(BKeyType type, const char* identifier,
	const char* secondaryIdentifier, BKey& key)
{
	return GetKey(NULL, type, identifier, secondaryIdentifier, false, key);
}


status_t
BKeyStore::GetKey(BKeyType type, const char* identifier,
	const char* secondaryIdentifier, bool secondaryIdentifierOptional,
	BKey& key)
{
	return GetKey(NULL, type, identifier, secondaryIdentifier,
		secondaryIdentifierOptional, key);
}


status_t
BKeyStore::GetKey(const char* keyring, BKeyType type, const char* identifier,
	BKey& key)
{
	return GetKey(keyring, type, identifier, NULL, true, key);
}


status_t
BKeyStore::GetKey(const char* keyring, BKeyType type, const char* identifier,
	const char* secondaryIdentifier, BKey& key)
{
	return GetKey(keyring, type, identifier, secondaryIdentifier, false, key);
}


status_t
BKeyStore::GetKey(const char* keyring, BKeyType type, const char* identifier,
	const char* secondaryIdentifier, bool secondaryIdentifierOptional,
	BKey& key)
{
	BMessage message(KEY_STORE_GET_KEY);
	message.AddString("keyring", keyring);
	message.AddUInt32("type", type);
	message.AddString("identifier", identifier);
	message.AddString("secondaryIdentifier", secondaryIdentifier);
	message.AddBool("secondaryIdentifierOptional", secondaryIdentifierOptional);

	BMessage reply;
	status_t result = _SendKeyMessage(message, &reply);
	if (result != B_OK)
		return result;

	BMessage keyMessage;
	if (reply.FindMessage("key", &keyMessage) != B_OK)
		return B_ERROR;

	return key.Unflatten(keyMessage);
}


status_t
BKeyStore::AddKey(const BKey& key)
{
	return AddKey(NULL, key);
}


status_t
BKeyStore::AddKey(const char* keyring, const BKey& key)
{
	BMessage keyMessage;
	if (key.Flatten(keyMessage) != B_OK)
		return B_BAD_VALUE;

	BMessage message(KEY_STORE_ADD_KEY);
	message.AddString("keyring", keyring);
	message.AddMessage("key", &keyMessage);

	return _SendKeyMessage(message, NULL);
}


status_t
BKeyStore::RemoveKey(const BKey& key)
{
	return RemoveKey(NULL, key);
}


status_t
BKeyStore::RemoveKey(const char* keyring, const BKey& key)
{
	BMessage keyMessage;
	if (key.Flatten(keyMessage) != B_OK)
		return B_BAD_VALUE;

	BMessage message(KEY_STORE_REMOVE_KEY);
	message.AddString("keyring", keyring);
	message.AddMessage("key", &keyMessage);

	return _SendKeyMessage(message, NULL);
}


status_t
BKeyStore::GetNextKey(uint32& cookie, BKey& key)
{
	return GetNextKey(NULL, cookie, key);
}


status_t
BKeyStore::GetNextKey(BKeyType type, BKeyPurpose purpose, uint32& cookie,
	BKey& key)
{
	return GetNextKey(NULL, type, purpose, cookie, key);
}


status_t
BKeyStore::GetNextKey(const char* keyring, uint32& cookie, BKey& key)
{
	return GetNextKey(keyring, B_KEY_TYPE_ANY, B_KEY_PURPOSE_ANY, cookie, key);
}


status_t
BKeyStore::GetNextKey(const char* keyring, BKeyType type, BKeyPurpose purpose,
	uint32& cookie, BKey& key)
{
	BMessage message(KEY_STORE_GET_NEXT_KEY);
	message.AddString("keyring", keyring);
	message.AddUInt32("type", type);
	message.AddUInt32("purpose", purpose);
	message.AddUInt32("cookie", cookie);

	BMessage reply;
	status_t result = _SendKeyMessage(message, &reply);
	if (result != B_OK)
		return result;

	BMessage keyMessage;
	if (reply.FindMessage("key", &keyMessage) != B_OK)
		return B_ERROR;

	reply.FindUInt32("cookie", &cookie);
	return key.Unflatten(keyMessage);
}


// #pragma mark - Keyrings


status_t
BKeyStore::AddKeyring(const char* keyring)
{
	BMessage message(KEY_STORE_ADD_KEYRING);
	message.AddString("keyring", keyring);
	return _SendKeyMessage(message, NULL);
}


status_t
BKeyStore::RemoveKeyring(const char* keyring)
{
	BMessage message(KEY_STORE_REMOVE_KEYRING);
	message.AddString("keyring", keyring);
	return _SendKeyMessage(message, NULL);
}


status_t
BKeyStore::GetNextKeyring(uint32& cookie, BString& keyring)
{
	BMessage message(KEY_STORE_GET_NEXT_KEYRING);
	message.AddUInt32("cookie", cookie);

	BMessage reply;
	status_t result = _SendKeyMessage(message, &reply);
	if (result != B_OK)
		return result;

	if (reply.FindString("keyring", &keyring) != B_OK)
		return B_ERROR;

	reply.FindUInt32("cookie", &cookie);
	return B_OK;
}


status_t
BKeyStore::SetUnlockKey(const char* keyring, const BKey& key)
{
	BMessage keyMessage;
	if (key.Flatten(keyMessage) != B_OK)
		return B_BAD_VALUE;

	BMessage message(KEY_STORE_SET_UNLOCK_KEY);
	message.AddString("keyring", keyring);
	message.AddMessage("key", &keyMessage);

	return _SendKeyMessage(message, NULL);
}


status_t
BKeyStore::RemoveUnlockKey(const char* keyring)
{
	BMessage message(KEY_STORE_REMOVE_UNLOCK_KEY);
	message.AddString("keyring", keyring);
	return _SendKeyMessage(message, NULL);
}


// #pragma mark - Master key


status_t
BKeyStore::SetMasterUnlockKey(const BKey& key)
{
	return SetUnlockKey(NULL, key);
}


status_t
BKeyStore::RemoveMasterUnlockKey()
{
	return RemoveUnlockKey(NULL);
}


status_t
BKeyStore::AddKeyringToMaster(const char* keyring)
{
	BMessage message(KEY_STORE_ADD_KEYRING_TO_MASTER);
	message.AddString("keyring", keyring);
	return _SendKeyMessage(message, NULL);
}


status_t
BKeyStore::RemoveKeyringFromMaster(const char* keyring)
{
	BMessage message(KEY_STORE_REMOVE_KEYRING_FROM_MASTER);
	message.AddString("keyring", keyring);
	return _SendKeyMessage(message, NULL);
}


status_t
BKeyStore::GetNextMasterKeyring(uint32& cookie, BString& keyring)
{
	BMessage message(KEY_STORE_GET_NEXT_MASTER_KEYRING);
	message.AddUInt32("cookie", cookie);

	BMessage reply;
	status_t result = _SendKeyMessage(message, &reply);
	if (result != B_OK)
		return result;

	if (reply.FindString("keyring", &keyring) != B_OK)
		return B_ERROR;

	reply.FindUInt32("cookie", &cookie);
	return B_OK;
}


// #pragma mark - Locking


bool
BKeyStore::IsKeyringUnlocked(const char* keyring)
{
	BMessage message(KEY_STORE_IS_KEYRING_UNLOCKED);
	message.AddString("keyring", keyring);

	BMessage reply;
	if (_SendKeyMessage(message, &reply) != B_OK)
		return false;

	bool unlocked;
	if (reply.FindBool("unlocked", &unlocked) != B_OK)
		return false;

	return unlocked;
}


status_t
BKeyStore::LockKeyring(const char* keyring)
{
	BMessage message(KEY_STORE_LOCK_KEYRING);
	message.AddString("keyring", keyring);
	return _SendKeyMessage(message, NULL);
}


status_t
BKeyStore::LockMasterKeyring()
{
	return LockKeyring(NULL);
}



// #pragma mark - Applications


status_t
BKeyStore::GetNextApplication(uint32& cookie, BString& signature) const
{
	return GetNextApplication(NULL, cookie, signature);
}


status_t
BKeyStore::GetNextApplication(const char* keyring, uint32& cookie,
	BString& signature) const
{
	BMessage message(KEY_STORE_GET_NEXT_APPLICATION);
	message.AddString("keyring", keyring);
	message.AddUInt32("cookie", cookie);

	BMessage reply;
	status_t result = _SendKeyMessage(message, &reply);
	if (result != B_OK)
		return result;

	if (reply.FindString("signature", &signature) != B_OK)
		return B_ERROR;

	reply.FindUInt32("cookie", &cookie);
	return B_OK;
}


status_t
BKeyStore::RemoveApplication(const char* signature)
{
	return RemoveApplication(NULL, signature);
}


status_t
BKeyStore::RemoveApplication(const char* keyring, const char* signature)
{
	BMessage message(KEY_STORE_REMOVE_APPLICATION);
	message.AddString("keyring", keyring);
	message.AddString("signature", signature);

	return _SendKeyMessage(message, NULL);
}


// #pragma mark - Service functions


status_t
BKeyStore::GeneratePassword(BPasswordKey& password, size_t length, uint32 flags)
{
	return B_ERROR;
}


float
BKeyStore::PasswordStrength(const char* password)
{
	return 0;
}


// #pragma mark - Private functions


status_t
BKeyStore::_SendKeyMessage(BMessage& message, BMessage* reply) const
{
	return LocalKeyStore::Instance().Dispatch(message, reply);
}
