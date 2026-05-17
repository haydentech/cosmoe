/*
 * Copyright 2002-2006 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Axel Dörfler, axeld@pinc-software.de
 *		Tyler Dauwalder
 *		Ingo Weinhold, bonefish@users.sf.net
 */


#include "MimeType.h"

#include <Bitmap.h>
#include <Directory.h>
#include <Entry.h>
#include <Path.h>
#include <TranslationUtils.h>
#include <View.h>
#include <mime/Database.h>
#include <mime/database_support.h>
#include <mime/DatabaseLocation.h>
#include <sniffer/Parser.h>

#include <RegistrarDefs.h>
//#include <RosterPrivate.h>

#include <ctype.h>
#include <new>
#include <stdio.h>
#include <strings.h>
#include <vector>


using namespace BPrivate;

// Private helper functions
static bool isValidMimeChar(const char ch);
static status_t default_database_status(BPrivate::Storage::Mime::Database*&
	database);

using namespace BPrivate::Storage::Mime;
using namespace std;


static status_t
default_database_status(BPrivate::Storage::Mime::Database*& database)
{
	database = default_database();
	if (database == NULL)
		return B_NO_MEMORY;

	return database->InitCheck();
}

const char* B_PEF_APP_MIME_TYPE		= "application/x-be-executable";
const char* B_PE_APP_MIME_TYPE		= "application/x-vnd.Be-peexecutable";
const char* B_ELF_APP_MIME_TYPE		= "application/x-vnd.Be-elfexecutable";
const char* B_RESOURCE_MIME_TYPE	= "application/x-be-resource";
const char* B_FILE_MIME_TYPE		= "application/octet-stream";
// Might be defined platform depended, but ELF will certainly be the common
// format for all platforms anyway.
const char* B_APP_MIME_TYPE			= B_ELF_APP_MIME_TYPE;


#ifdef __linux__
static void
desktop_id_from_mime_type(const char* type, char* desktopID,
	size_t desktopIDSize)
{
	if (desktopID == NULL || desktopIDSize == 0)
		return;

	desktopID[0] = '\0';
	if (type == NULL || type[0] == '\0')
		return;

	const char* normalized = type;
	if (strncasecmp(normalized, "application/", 12) == 0)
		normalized += 12;

	strlcpy(desktopID, normalized, desktopIDSize);
}


static status_t
desktop_entry_value(const BPath& desktopPath, const char* key, char* value,
	size_t valueSize);


static status_t
normalize_xdg_lookup_key(const char* input, char* output, size_t outputSize)
{
	if (output == NULL || outputSize == 0)
		return B_BAD_VALUE;

	output[0] = '\0';
	if (input == NULL || input[0] == '\0')
		return B_BAD_VALUE;

	size_t out = 0;
	for (size_t i = 0; input[i] != '\0' && out + 1 < outputSize; i++) {
		unsigned char c = (unsigned char)input[i];
		if (isalnum(c))
			output[out++] = (char)tolower(c);
		else if (c == '.' || c == '-' || c == '_')
			output[out++] = '-';
		else if (isspace(c))
			output[out++] = '-';
	}
	output[out] = '\0';
	return output[0] != '\0' ? B_OK : B_BAD_VALUE;
}


static status_t
find_xdg_desktop_file_for_type(const char* type, BPath& desktopPath)
{
	char desktopID[B_MIME_TYPE_LENGTH];
	desktop_id_from_mime_type(type, desktopID, sizeof(desktopID));
	if (desktopID[0] == '\0')
		return B_BAD_VALUE;

	char normalizedDesktopID[B_MIME_TYPE_LENGTH];
	normalize_xdg_lookup_key(desktopID, normalizedDesktopID,
		sizeof(normalizedDesktopID));

	const char* dataDirs = getenv("XDG_DATA_DIRS");
	if (dataDirs == NULL || dataDirs[0] == '\0')
		dataDirs = "/usr/local/share:/usr/share";

	char* dataDirsCopy = strdup(dataDirs);
	if (dataDirsCopy == NULL)
		return B_NO_MEMORY;

	status_t status = B_ENTRY_NOT_FOUND;
	char* last = NULL;
	for (char* root = strtok_r(dataDirsCopy, ":", &last);
		root != NULL; root = strtok_r(NULL, ":", &last)) {
		if (root[0] == '\0')
			continue;

		char desktopFileName[B_MIME_TYPE_LENGTH + 16];
		strlcpy(desktopFileName, desktopID, sizeof(desktopFileName));
		strlcat(desktopFileName, ".desktop", sizeof(desktopFileName));

		BPath candidate(root, "applications");
		candidate.Append(desktopFileName);

		BEntry entry(candidate.Path());
		if (entry.Exists() && entry.IsFile()) {
			desktopPath = candidate;
			status = B_OK;
			break;
		}

		BPath applicationsPath;
		candidate.GetParent(&applicationsPath);
		BDirectory applications(applicationsPath.Path());
		if (applications.InitCheck() != B_OK)
			continue;

		BEntry appEntry;
		while (applications.GetNextEntry(&appEntry) == B_OK) {
			if (!appEntry.IsFile())
				continue;

			BPath appPath;
			if (appEntry.GetPath(&appPath) != B_OK)
				continue;

			const char* leaf = appPath.Leaf();
			if (leaf == NULL)
				continue;
			size_t leafLength = strlen(leaf);
			if (leafLength <= 8
				|| strcasecmp(leaf + leafLength - 8, ".desktop") != 0) {
				continue;
			}

			char candidateDesktopID[B_MIME_TYPE_LENGTH];
			strlcpy(candidateDesktopID, leaf, sizeof(candidateDesktopID));
			candidateDesktopID[leafLength - 8] = '\0';

			char normalizedCandidateID[B_MIME_TYPE_LENGTH];
			normalize_xdg_lookup_key(candidateDesktopID, normalizedCandidateID,
				sizeof(normalizedCandidateID));
			if (strcmp(normalizedCandidateID, normalizedDesktopID) == 0) {
				desktopPath = appPath;
				status = B_OK;
				break;
			}

			char value[B_PATH_NAME_LENGTH];
			if (desktop_entry_value(appPath, "StartupWMClass", value,
					sizeof(value)) == B_OK) {
				char normalizedValue[B_PATH_NAME_LENGTH];
				normalize_xdg_lookup_key(value, normalizedValue,
					sizeof(normalizedValue));
				if (strcmp(normalizedValue, normalizedDesktopID) == 0) {
					desktopPath = appPath;
					status = B_OK;
					break;
				}
			}

			if (desktop_entry_value(appPath, "Name", value, sizeof(value))
					== B_OK) {
				char normalizedValue[B_PATH_NAME_LENGTH];
				normalize_xdg_lookup_key(value, normalizedValue,
					sizeof(normalizedValue));
				if (strcmp(normalizedValue, normalizedDesktopID) == 0) {
					desktopPath = appPath;
					status = B_OK;
					break;
				}
			}
		}

		if (status == B_OK)
			break;
	}

	free(dataDirsCopy);
	return status;
}


static status_t
desktop_entry_value(const BPath& desktopPath, const char* key, char* value,
	size_t valueSize)
{
	if (key == NULL || value == NULL || valueSize == 0)
		return B_BAD_VALUE;

	value[0] = '\0';
	FILE* file = fopen(desktopPath.Path(), "r");
	if (file == NULL)
		return errno != 0 ? errno : B_ENTRY_NOT_FOUND;

	char prefix[128];
	strlcpy(prefix, key, sizeof(prefix));
	strlcat(prefix, "=", sizeof(prefix));
	size_t prefixLength = strlen(prefix);

	bool inDesktopEntry = false;
	char line[1024];
	while (fgets(line, sizeof(line), file) != NULL) {
		size_t length = strlen(line);
		while (length > 0 && (line[length - 1] == '\n'
			|| line[length - 1] == '\r')) {
			line[--length] = '\0';
		}

		if (strcmp(line, "[Desktop Entry]") == 0) {
			inDesktopEntry = true;
			continue;
		}

		if (line[0] == '[') {
			inDesktopEntry = false;
			continue;
		}

		if (inDesktopEntry && strncmp(line, prefix, prefixLength) == 0) {
			strlcpy(value, line + prefixLength, valueSize);
			fclose(file);
			return B_OK;
		}
	}

	fclose(file);
	return B_ENTRY_NOT_FOUND;
}


static status_t
import_bitmap_from_path(const char* path, BBitmap* icon)
{
	if (path == NULL || path[0] == '\0' || icon == NULL)
		return B_BAD_VALUE;

	BBitmap* loaded = BTranslationUtils::GetBitmap(path);
	if (loaded == NULL)
		return B_ENTRY_NOT_FOUND;

	status_t status;
	if (loaded->Bounds() == icon->Bounds()) {
		status = icon->ImportBits(loaded);
	} else {
		BBitmap rendered(icon->Bounds(), B_BITMAP_ACCEPTS_VIEWS, B_RGBA32);
		status = rendered.InitCheck();
		if (status == B_OK) {
			memset(rendered.Bits(), 0, rendered.BitsLength());
			if (rendered.Lock()) {
				BView* helper = new(std::nothrow) BView(rendered.Bounds(),
					"icon-scale-helper", B_FOLLOW_NONE, B_WILL_DRAW);
				if (helper != NULL) {
					rendered.AddChild(helper);
					helper->SetViewColor(B_TRANSPARENT_COLOR);
					helper->SetHighColor(B_TRANSPARENT_COLOR);
					helper->FillRect(rendered.Bounds(), B_SOLID_LOW);
					helper->SetDrawingMode(B_OP_OVER);
					helper->DrawBitmap(loaded, loaded->Bounds(),
						rendered.Bounds());
					helper->Sync();
				} else {
					status = B_NO_MEMORY;
				}
				rendered.Unlock();
			}
			if (status == B_OK)
				status = icon->ImportBits(&rendered);
		}
	}

	delete loaded;
	return status;
}


static status_t
try_import_icon_path(const char* path, BBitmap* icon)
{
	if (path == NULL || path[0] == '\0' || icon == NULL)
		return B_BAD_VALUE;

	BEntry entry(path);
	if (!entry.Exists() || !entry.IsFile())
		return B_ENTRY_NOT_FOUND;

	return import_bitmap_from_path(path, icon);
}


static status_t
import_icon_from_theme_dir(const char* themePath, const char* iconName,
	icon_size size, BBitmap* icon)
{
	if (themePath == NULL || iconName == NULL || iconName[0] == '\0'
		|| icon == NULL) {
		return B_BAD_VALUE;
	}

	const int preferredSize = size == B_MINI_ICON ? 16 : 32;
	const int sizeOrder[] = { preferredSize, 24, 22, 32, 48, 64, 96, 128, 256, 16, 0 };
	const char* extensions[] = { ".png", ".xpm", ".svg", NULL };
	char candidate[B_PATH_NAME_LENGTH];
	status_t lastError = B_ENTRY_NOT_FOUND;

	for (int sizeIndex = 0; sizeOrder[sizeIndex] != 0; sizeIndex++) {
		for (int extIndex = 0; extensions[extIndex] != NULL; extIndex++) {
			snprintf(candidate, sizeof(candidate), "%s/%dx%d/apps/%s%s",
				themePath, sizeOrder[sizeIndex], sizeOrder[sizeIndex], iconName,
				extensions[extIndex]);
			status_t status = try_import_icon_path(candidate, icon);
			if (status == B_OK)
				return B_OK;
			if (status != B_ENTRY_NOT_FOUND)
				lastError = status;

			snprintf(candidate, sizeof(candidate), "%s/apps/%d/%s%s",
				themePath, sizeOrder[sizeIndex], iconName,
				extensions[extIndex]);
			status = try_import_icon_path(candidate, icon);
			if (status == B_OK)
				return B_OK;
			if (status != B_ENTRY_NOT_FOUND)
				lastError = status;
		}
	}

	for (int extIndex = 0; extensions[extIndex] != NULL; extIndex++) {
		snprintf(candidate, sizeof(candidate), "%s/scalable/apps/%s%s",
			themePath, iconName, extensions[extIndex]);
		status_t status = try_import_icon_path(candidate, icon);
		if (status == B_OK)
			return B_OK;
		if (status != B_ENTRY_NOT_FOUND)
			lastError = status;

		snprintf(candidate, sizeof(candidate), "%s/apps/scalable/%s%s",
			themePath, iconName, extensions[extIndex]);
		status = try_import_icon_path(candidate, icon);
		if (status == B_OK)
			return B_OK;
		if (status != B_ENTRY_NOT_FOUND)
			lastError = status;
	}

	return lastError;
}


static status_t
import_xdg_icon_by_name(const char* iconName, icon_size size, BBitmap* icon)
{
	if (iconName == NULL || iconName[0] == '\0' || icon == NULL)
		return B_BAD_VALUE;

	status_t lastError = B_ENTRY_NOT_FOUND;

	if (strchr(iconName, '/') != NULL) {
		status_t status = try_import_icon_path(iconName, icon);
		if (status == B_OK)
			return B_OK;
		if (status != B_ENTRY_NOT_FOUND)
			lastError = status;

		const char* extensions[] = { ".png", ".xpm", ".svg", NULL };
		for (int extIndex = 0; extensions[extIndex] != NULL; extIndex++) {
			char withExtension[B_PATH_NAME_LENGTH];
			strlcpy(withExtension, iconName, sizeof(withExtension));
			strlcat(withExtension, extensions[extIndex], sizeof(withExtension));
			status = try_import_icon_path(withExtension, icon);
			if (status == B_OK)
				return B_OK;
			if (status != B_ENTRY_NOT_FOUND)
				lastError = status;
		}
	}

	const char* preferredThemes[] = {
		"hicolor",
		"elementary-xfce",
		"elementary-xfce-dark",
		"elementary-xfce-darker",
		"elementary-xfce-darkest",
		"Adwaita",
		"gnome",
		NULL
	};

	std::vector<BString> iconThemeRoots;
	const char* home = getenv("HOME");
	if (home != NULL && home[0] != '\0') {
		BPath legacyIcons(home, ".icons");
		iconThemeRoots.emplace_back(legacyIcons.Path());

		const char* xdgDataHome = getenv("XDG_DATA_HOME");
		if (xdgDataHome != NULL && xdgDataHome[0] != '\0') {
			BPath dataHomeIcons(xdgDataHome, "icons");
			iconThemeRoots.emplace_back(dataHomeIcons.Path());
		} else {
			BPath defaultDataHome(home, ".local/share/icons");
			iconThemeRoots.emplace_back(defaultDataHome.Path());
		}
	}

	const char* xdgDataDirs = getenv("XDG_DATA_DIRS");
	if (xdgDataDirs == NULL || xdgDataDirs[0] == '\0')
		xdgDataDirs = "/usr/local/share:/usr/share";

	char* xdgDataDirsCopy = strdup(xdgDataDirs);
	if (xdgDataDirsCopy != NULL) {
		char* last = NULL;
		for (char* root = strtok_r(xdgDataDirsCopy, ":", &last);
			root != NULL; root = strtok_r(NULL, ":", &last)) {
			if (root[0] == '\0')
				continue;

			BPath iconsPath(root, "icons");
			iconThemeRoots.emplace_back(iconsPath.Path());
		}
		free(xdgDataDirsCopy);
	}

	for (size_t rootIndex = 0; rootIndex < iconThemeRoots.size(); rootIndex++) {
		const char* rootPath = iconThemeRoots[rootIndex].String();
		if (rootPath == NULL || rootPath[0] == '\0')
			continue;

		for (int themeIndex = 0; preferredThemes[themeIndex] != NULL;
			themeIndex++) {
			BPath themePath(rootPath, preferredThemes[themeIndex]);
			status_t status = import_icon_from_theme_dir(themePath.Path(), iconName,
				size, icon);
			if (status == B_OK)
				return B_OK;
			if (status != B_ENTRY_NOT_FOUND)
				lastError = status;
		}

		BDirectory root(rootPath);
		if (root.InitCheck() != B_OK)
			continue;

		BEntry themeEntry;
		while (root.GetNextEntry(&themeEntry) == B_OK) {
			if (!themeEntry.IsDirectory())
				continue;

			BPath themePath;
			if (themeEntry.GetPath(&themePath) != B_OK)
				continue;

			const char* themeName = themePath.Leaf();
			if (themeName == NULL)
				continue;

			bool skip = strcasecmp(themeName, "HighContrast") == 0;
			for (int themeIndex = 0; !skip && preferredThemes[themeIndex] != NULL;
				themeIndex++) {
				if (strcasecmp(themeName, preferredThemes[themeIndex]) == 0)
					skip = true;
			}
			if (skip)
				continue;

			status_t status = import_icon_from_theme_dir(themePath.Path(), iconName,
				size, icon);
			if (status == B_OK)
				return B_OK;
			if (status != B_ENTRY_NOT_FOUND)
				lastError = status;
		}

		BPath highContrastPath(rootPath, "HighContrast");
		status_t status = import_icon_from_theme_dir(highContrastPath.Path(),
			iconName, size, icon);
		if (status == B_OK)
			return B_OK;
		if (status != B_ENTRY_NOT_FOUND)
			lastError = status;
	}

	std::vector<BString> pixmapRoots;
	pixmapRoots.emplace_back("/usr/share/pixmaps");
	pixmapRoots.emplace_back("/usr/local/share/pixmaps");
	const char* extensions[] = { ".png", ".xpm", ".svg", NULL };
	for (size_t rootIndex = 0; rootIndex < pixmapRoots.size(); rootIndex++) {
		const char* pixmapRoot = pixmapRoots[rootIndex].String();
		for (int extIndex = 0; extensions[extIndex] != NULL; extIndex++) {
			char candidate[B_PATH_NAME_LENGTH];
			snprintf(candidate, sizeof(candidate), "%s/%s%s",
				pixmapRoot, iconName, extensions[extIndex]);
			status_t status = try_import_icon_path(candidate, icon);
			if (status == B_OK)
				return B_OK;
			if (status != B_ENTRY_NOT_FOUND)
				lastError = status;
		}
	}

	return lastError;
}


static status_t
get_xdg_icon_for_type(const char* type, BBitmap* icon, icon_size size)
{
	if (type == NULL || icon == NULL)
		return B_BAD_VALUE;

	BPath desktopPath;
	status_t status = find_xdg_desktop_file_for_type(type, desktopPath);
	if (status != B_OK)
		return status;

	char iconName[B_PATH_NAME_LENGTH];
	status = desktop_entry_value(desktopPath, "Icon", iconName,
		sizeof(iconName));
	if (status != B_OK)
		return status;

	return import_xdg_icon_by_name(iconName, size, icon);
}
#endif


static bool
isValidMimeChar(const char ch)
{
	// Handles white space and most CTLs
	return ch > 32
		&& ch != '/'
		&& ch != '<'
		&& ch != '>'
		&& ch != '@'
		&& ch != ','
		&& ch != ';'
		&& ch != ':'
		&& ch != '"'
		&& ch != '('
		&& ch != ')'
		&& ch != '['
		&& ch != ']'
		&& ch != '?'
		&& ch != '='
		&& ch != '\\'
		&& ch != 127;	// DEL
}


//	#pragma mark -


// Creates an uninitialized BMimeType object.
BMimeType::BMimeType()
	:
	fType(NULL),
	fCStatus(B_NO_INIT)
{
}


// Creates a BMimeType object and initializes it to the supplied
// MIME type.
BMimeType::BMimeType(const char* mimeType)
	:
	fType(NULL),
	fCStatus(B_NO_INIT)
{
	SetTo(mimeType);
}


// Frees all resources associated with this object.
BMimeType::~BMimeType()
{
	Unset();
}


// Initializes this object to the supplied MIME type.
status_t
BMimeType::SetTo(const char* mimeType)
{
	if (mimeType == NULL) {
		Unset();
	} else if (!BMimeType::IsValid(mimeType)) {
		fCStatus = B_BAD_VALUE;
	} else {
		Unset();
		fType = new(std::nothrow) char[B_MIME_TYPE_LENGTH];	// Cosmoe bugfix
		if (fType) {
			strlcpy(fType, mimeType, B_MIME_TYPE_LENGTH);
			fCStatus = B_OK;
		} else {
			fCStatus = B_NO_MEMORY;
		}
	}
	return fCStatus;
}


// Returns the object to an uninitialized state
void
BMimeType::Unset()
{
	delete [] fType;
	fType = NULL;
	fCStatus = B_NO_INIT;
}


// Returns the result of the most recent constructor or SetTo() call
status_t
BMimeType::InitCheck() const
{
	return fCStatus;
}


// Returns the MIME string represented by this object
const char*
BMimeType::Type() const
{
	return fType;
}


// Returns whether the object represents a valid MIME type
bool
BMimeType::IsValid() const
{
	return InitCheck() == B_OK && BMimeType::IsValid(Type());
}


// Returns whether this objects represents a supertype
bool
BMimeType::IsSupertypeOnly() const
{
	if (fCStatus == B_OK) {
		// We assume here fCStatus will be B_OK *only* if
		// the MIME string is valid
		size_t len = strlen(fType);
		for (size_t i = 0; i < len; i++) {
			if (fType[i] == '/')
				return false;
		}
		return true;
	} else
		return false;
}


// Returns whether or not this type is currently installed in the
// MIME database
bool
BMimeType::IsInstalled() const
{
	return InitCheck() == B_OK
		&& default_database_location()->IsInstalled(Type());
}


// Gets the supertype of the MIME type represented by this object
status_t
BMimeType::GetSupertype(BMimeType* supertype) const
{
	if (supertype == NULL)
		return B_BAD_VALUE;

	supertype->Unset();
	status_t status = fCStatus == B_OK ? B_OK : B_BAD_VALUE;
	if (status == B_OK) {
		size_t len = strlen(fType);
		size_t i = 0;
		for (; i < len; i++) {
			if (fType[i] == '/')
				break;
		}
		if (i == len) {
			// object is a supertype only
			status = B_BAD_VALUE;
		} else {
			char superMime[B_MIME_TYPE_LENGTH];
			strncpy(superMime, fType, i);
			superMime[i] = 0;
			status = supertype->SetTo(superMime) == B_OK ? B_OK : B_BAD_VALUE;
		}
	}

	return status;
}


// Returns whether this and the supplied MIME type are equal
bool
BMimeType::operator==(const BMimeType &type) const
{
	if (InitCheck() == B_NO_INIT && type.InitCheck() == B_NO_INIT)
		return true;
	else if (InitCheck() == B_OK && type.InitCheck() == B_OK)
		return strcasecmp(Type(), type.Type()) == 0;

	return false;
}


// Returns whether this and the supplied MIME type are equal
bool
BMimeType::operator==(const char* type) const
{
	BMimeType mime;
	if (type)
		mime.SetTo(type);

	return (*this) == mime;
}


// Returns whether this MIME type is a supertype of or equals the
// supplied one
bool
BMimeType::Contains(const BMimeType* type) const
{
	if (type == NULL)
		return false;

	if (*this == *type)
		return true;

	BMimeType super;
	if (type->GetSupertype(&super) == B_OK && *this == super)
		return true;
	return false;
}


// Adds the MIME type to the MIME database
status_t
BMimeType::Install()
{
	Database* database;
	status_t status = InitCheck();
	if (status == B_OK)
		status = default_database_status(database);
	if (status != B_OK)
		return status;

	status = database->Install(Type());
	return status == B_FILE_EXISTS ? B_OK : status;
}


// Removes the MIME type from the MIME database
status_t
BMimeType::Delete()
{
	Database* database;
	status_t status = InitCheck();
	if (status == B_OK)
		status = default_database_status(database);
	if (status != B_OK)
		return status;

	return database->Delete(Type());
}


// Fetches the large or mini icon associated with the MIME type
status_t
BMimeType::GetIcon(BBitmap* icon, icon_size size) const
{
	if (icon == NULL)
		return B_BAD_VALUE;

	status_t err = InitCheck();
	if (err == B_OK)
		err = default_database_location()->GetIcon(Type(), *icon, size);

#ifdef __linux__
	if (err != B_OK)
		err = get_xdg_icon_for_type(Type(), icon, size);
#endif

	return err;
}


//	Fetches the vector icon associated with the MIME type
status_t
BMimeType::GetIcon(uint8** data, size_t* size) const
{
	if (data == NULL || size == NULL)
		return B_BAD_VALUE;

	status_t err = InitCheck();
	if (err == B_OK)
		err = default_database_location()->GetIcon(Type(), *data, *size);

	return err;
}


// Fetches the signature of the MIME type's preferred application from the
// MIME database
status_t
BMimeType::GetPreferredApp(char* signature, app_verb verb) const
{
	status_t err = InitCheck();
	if (err == B_OK) {
		err = default_database_location()->GetPreferredApp(Type(), signature,
			verb);
	}

	return err;
}


// Fetches from the MIME database a BMessage describing the attributes
// typically associated with files of the given MIME type
status_t
BMimeType::GetAttrInfo(BMessage* info) const
{
	if (info == NULL)
		return B_BAD_VALUE;

	status_t err = InitCheck();
	if (err == B_OK)
		err = default_database_location()->GetAttributesInfo(Type(), *info);

	return err;
}


// Fetches the MIME type's associated filename extensions from the MIME
// database
status_t
BMimeType::GetFileExtensions(BMessage* extensions) const
{
	if (extensions == NULL)
		return B_BAD_VALUE;

	status_t err = InitCheck();
	if (err == B_OK) {
		err = default_database_location()->GetFileExtensions(Type(),
			*extensions);
	}

	return err;
}


// Fetches the MIME type's short description from the MIME database
status_t
BMimeType::GetShortDescription(char* description) const
{
	status_t err = InitCheck();
	if (err == B_OK) {
		err = default_database_location()->GetShortDescription(Type(),
			description);
	}

	return err;
}


// Fetches the MIME type's long description from the MIME database
status_t
BMimeType::GetLongDescription(char* description) const
{
	status_t err = InitCheck();
	if (err == B_OK) {
		err = default_database_location()->GetLongDescription(Type(),
			description);
	}

	return err;
}


// Fetches a \c BMessage containing a list of MIME signatures of
// applications that are able to handle files of this MIME type.
status_t
BMimeType::GetSupportingApps(BMessage* signatures) const
{
	if (signatures == NULL)
		return B_BAD_VALUE;

	Database* database;
	status_t status = InitCheck();
	if (status == B_OK)
		status = default_database_status(database);
	if (status != B_OK)
		return status;

	return database->GetSupportingApps(Type(), signatures);
}


// Sets the large or mini icon for the MIME type
status_t
BMimeType::SetIcon(const BBitmap* icon, icon_size which)
{
	return SetIconForType(NULL, icon, which);
}


// Sets the vector icon for the MIME type
status_t
BMimeType::SetIcon(const uint8* data, size_t size)
{
	return SetIconForType(NULL, data, size);
}


// Sets the preferred application for the MIME type
status_t
BMimeType::SetPreferredApp(const char* signature, app_verb verb)
{
	Database* database;
	status_t status = InitCheck();
	if (status == B_OK)
		status = default_database_status(database);
	if (status != B_OK)
		return status;

	if (signature != NULL)
		return database->SetPreferredApp(Type(), signature, verb);

	char currentSignature[B_MIME_TYPE_LENGTH];
	status_t existingStatus = default_database_location()->GetPreferredApp(Type(),
		currentSignature, verb);
	status = database->DeletePreferredApp(Type(), verb);
	if (status == B_OK && existingStatus == B_ENTRY_NOT_FOUND)
		return B_ENTRY_NOT_FOUND;

	return status;
}


// Sets the description of the attributes typically associated with files
// of the given MIME type
status_t
BMimeType::SetAttrInfo(const BMessage* info)
{
	Database* database;
	status_t status = InitCheck();
	if (status == B_OK)
		status = default_database_status(database);
	if (status != B_OK)
		return status;

	return info != NULL
		? database->SetAttrInfo(Type(), info)
		: database->DeleteAttrInfo(Type());
}


// Sets the list of filename extensions associated with the MIME type
status_t
BMimeType::SetFileExtensions(const BMessage* extensions)
{
	Database* database;
	status_t status = InitCheck();
	if (status == B_OK)
		status = default_database_status(database);
	if (status != B_OK)
		return status;

	return extensions != NULL
		? database->SetFileExtensions(Type(), extensions)
		: database->DeleteFileExtensions(Type());
}


// Sets the short description field for the MIME type
status_t
BMimeType::SetShortDescription(const char* description)
{
	Database* database;
	status_t status = InitCheck();
	if (status == B_OK)
		status = default_database_status(database);
	if (status != B_OK)
		return status;

	return description != NULL
		? database->SetShortDescription(Type(), description)
		: database->DeleteShortDescription(Type());
}


// Sets the long description field for the MIME type
status_t
BMimeType::SetLongDescription(const char* description)
{
	Database* database;
	status_t status = InitCheck();
	if (status == B_OK)
		status = default_database_status(database);
	if (status != B_OK)
		return status;

	return description != NULL
		? database->SetLongDescription(Type(), description)
		: database->DeleteLongDescription(Type());
}


// Fetches a BMessage listing all the MIME supertypes currently
// installed in the MIME database.
/*static*/ status_t
BMimeType::GetInstalledSupertypes(BMessage* supertypes)
{
	if (supertypes == NULL)
		return B_BAD_VALUE;

	Database* database;
	status_t status = default_database_status(database);
	if (status != B_OK)
		return status;

	return database->GetInstalledSupertypes(supertypes);
}


// Fetches a BMessage listing all the MIME types currently installed
// in the MIME database.
status_t
BMimeType::GetInstalledTypes(BMessage* types)
{
	return GetInstalledTypes(NULL, types);
}


// Fetches a BMessage listing all the MIME subtypes of the given
// supertype currently installed in the MIME database.
/*static*/ status_t
BMimeType::GetInstalledTypes(const char* supertype, BMessage* types)
{
	if (types == NULL)
		return B_BAD_VALUE;

	Database* database;
	status_t status = default_database_status(database);
	if (status != B_OK)
		return status;

	return supertype != NULL
		? database->GetInstalledTypes(supertype, types)
		: database->GetInstalledTypes(types);
}


// Fetches a \c BMessage containing a list of MIME signatures of
// applications that are able to handle files of any type.
status_t
BMimeType::GetWildcardApps(BMessage* wild_ones)
{
	BMimeType mime;
	status_t err = mime.SetTo("application/octet-stream");
	if (err == B_OK)
		err = mime.GetSupportingApps(wild_ones);
	return err;
}


// Returns whether the given string represents a valid MIME type.
bool
BMimeType::IsValid(const char* string)
{
	if (string == NULL)
		return false;

	bool foundSlash = false;
	size_t len = strlen(string);
	if (len >= B_MIME_TYPE_LENGTH || len == 0)
		return false;

	for (size_t i = 0; i < len; i++) {
		char ch = string[i];
		if (ch == '/') {
			if (foundSlash || i == 0 || i == len - 1)
				return false;
			else
				foundSlash = true;
		} else if (!isValidMimeChar(ch)) {
			return false;
		}
	}
	return true;
}


// Fetches an \c entry_ref that serves as a hint as to where the MIME type's
// preferred application might live
status_t
BMimeType::GetAppHint(entry_ref* ref) const
{
	if (ref == NULL)
		return B_BAD_VALUE;

	status_t err = InitCheck();
	if (err == B_OK)
		err = default_database_location()->GetAppHint(Type(), *ref);
	return err;
}


// Sets the app hint field for the MIME type
status_t
BMimeType::SetAppHint(const entry_ref* ref)
{
	Database* database;
	status_t status = InitCheck();
	if (status == B_OK)
		status = default_database_status(database);
	if (status != B_OK)
		return status;

	if (ref != NULL)
		return database->SetAppHint(Type(), ref);

	entry_ref currentRef;
	status_t existingStatus = default_database_location()->GetAppHint(Type(),
		currentRef);
	status = database->DeleteAppHint(Type());
	if (status == B_OK && existingStatus == B_ENTRY_NOT_FOUND)
		return B_ENTRY_NOT_FOUND;

	return status;
}


// Fetches the large or mini icon used by an application of this type for
// files of the given type.
status_t
BMimeType::GetIconForType(const char* type, BBitmap* icon, icon_size which) const
{
	if (icon == NULL)
		return B_BAD_VALUE;

	// If type is NULL, this function works just like GetIcon(), othewise,
	// we need to make sure the give type is valid.
	status_t err;
	if (type) {
		err = BMimeType::IsValid(type) ? B_OK : B_BAD_VALUE;
		if (err == B_OK) {
			err = default_database_location()->GetIconForType(Type(), type,
				*icon, which);
		}
	} else
		err = GetIcon(icon, which);

	return err;
}


// Fetches the vector icon used by an application of this type for files of
// the given type.
status_t
BMimeType::GetIconForType(const char* type, uint8** _data, size_t* _size) const
{
	if (_data == NULL || _size == NULL)
		return B_BAD_VALUE;

	// If type is NULL, this function works just like GetIcon(), otherwise,
	// we need to make sure the give type is valid.
	if (type == NULL)
		return GetIcon(_data, _size);

	if (!BMimeType::IsValid(type))
		return B_BAD_VALUE;

	return default_database_location()->GetIconForType(Type(), type, *_data,
		*_size);
}


// Sets the large or mini icon used by an application of this type for
// files of the given type.
status_t
BMimeType::SetIconForType(const char* type, const BBitmap* icon, icon_size which)
{
	Database* database;
	status_t status = InitCheck();
	if (status == B_OK)
		status = default_database_status(database);
	if (status != B_OK)
		return status;

	if (type != NULL && !BMimeType::IsValid(type))
		return B_BAD_VALUE;

	if (icon == NULL) {
		if (!IsInstalled())
			return B_ENTRY_NOT_FOUND;

		return type != NULL
			? database->DeleteIconForType(Type(), type, which)
			: database->DeleteIcon(Type(), which);
	}

	void* data = NULL;
	int32 dataSize = 0;
	status = get_icon_data(icon, which, &data, &dataSize);
	if (status != B_OK)
		return status;

	status = type != NULL
		? database->SetIconForType(Type(), type, data, dataSize, which)
		: database->SetIcon(Type(), data, dataSize, which);

	delete[] static_cast<char*>(data);
	return status;
}


// Sets the large or mini icon used by an application of this type for
// files of the given type.
status_t
BMimeType::SetIconForType(const char* type, const uint8* data, size_t dataSize)
{
	Database* database;
	status_t status = InitCheck();
	if (status == B_OK)
		status = default_database_status(database);
	if (status != B_OK)
		return status;

	if (type != NULL && !BMimeType::IsValid(type))
		return B_BAD_VALUE;

	if (data == NULL) {
		if (!IsInstalled())
			return B_ENTRY_NOT_FOUND;

		return type != NULL
			? database->DeleteIconForType(Type(), type)
			: database->DeleteIcon(Type());
	}

	return type != NULL
		? database->SetIconForType(Type(), type, data, dataSize)
		: database->SetIcon(Type(), data, dataSize);
}


// Retrieves the MIME type's sniffer rule
status_t
BMimeType::GetSnifferRule(BString* result) const
{
	if (result == NULL)
		return B_BAD_VALUE;

	status_t err = InitCheck();
	if (err == B_OK)
		err = default_database_location()->GetSnifferRule(Type(), *result);

	return err;
}


// Sets the MIME type's sniffer rule
status_t
BMimeType::SetSnifferRule(const char* rule)
{
	Database* database;
	status_t status = InitCheck();
	if (status == B_OK)
		status = default_database_status(database);
	if (status != B_OK)
		return status;

	if (rule != NULL)
		return database->SetSnifferRule(Type(), rule);

	BString currentRule;
	status_t existingStatus = default_database_location()->GetSnifferRule(Type(),
		currentRule);
	status = database->DeleteSnifferRule(Type());
	if (status == B_OK && existingStatus == B_ENTRY_NOT_FOUND)
		return B_ENTRY_NOT_FOUND;

	return status;
}


// Checks whether a MIME sniffer rule is valid or not.
status_t
BMimeType::CheckSnifferRule(const char* rule, BString* parseError)
{
	if (rule == NULL)
		return B_BAD_VALUE;

	BPrivate::Storage::Sniffer::Rule parsedRule;
	status_t status = BPrivate::Storage::Sniffer::parse(rule, &parsedRule,
		parseError);
	return status == B_OK ? B_OK : B_BAD_MIME_SNIFFER_RULE;
}


// Guesses a MIME type for the entry referred to by the given
// entry_ref.
status_t
BMimeType::GuessMimeType(const entry_ref* file, BMimeType* type)
{
	if (file == NULL || type == NULL)
		return B_BAD_VALUE;

	Database* database;
	status_t status = default_database_status(database);
	if (status != B_OK)
		return status;

	BString mimeType;
	status = database->GuessMimeType(file, &mimeType);
	if (status == B_OK)
		status = type->SetTo(mimeType.String());

	return status;
}


// Guesses a MIME type for the supplied chunk of data.
status_t
BMimeType::GuessMimeType(const void* buffer, int32 length, BMimeType* type)
{
	if (buffer == NULL || type == NULL)
		return B_BAD_VALUE;

	Database* database;
	status_t status = default_database_status(database);
	if (status != B_OK)
		return status;

	BString mimeType;
	status = database->GuessMimeType(buffer, length, &mimeType);
	if (status == B_OK)
		status = type->SetTo(mimeType.String());

	return status;
}


// Guesses a MIME type for the given filename.
status_t
BMimeType::GuessMimeType(const char* filename, BMimeType* type)
{
	if (filename == NULL || type == NULL)
		return B_BAD_VALUE;

	Database* database;
	status_t status = default_database_status(database);
	if (status != B_OK)
		return status;

	BString mimeType;
	status = database->GuessMimeType(filename, &mimeType);
	if (status == B_OK)
		status = type->SetTo(mimeType.String());

	return status;
}


// Starts monitoring the MIME database for a given target.
status_t
BMimeType::StartWatching(BMessenger target)
{
	Database* database;
	status_t status = default_database_status(database);
	if (status != B_OK)
		return status;

	return database->StartWatching(target);
}


// Stops monitoring the MIME database for a given target
status_t
BMimeType::StopWatching(BMessenger target)
{
	Database* database;
	status_t status = default_database_status(database);
	if (status != B_OK)
		return status;

	return database->StopWatching(target);
}


// Initializes this object to the supplied MIME type
status_t
BMimeType::SetType(const char* mimeType)
{
	return SetTo(mimeType);
}


void BMimeType::_ReservedMimeType1() {}
void BMimeType::_ReservedMimeType2() {}
void BMimeType::_ReservedMimeType3() {}


#ifdef __HAIKU_BEOS_COMPATIBLE
// assignment operator.
// Unimplemented
BMimeType&
BMimeType::operator=(const BMimeType &)
{
	return *this;
		// not implemented
}


// copy constructor
// Unimplemented
BMimeType::BMimeType(const BMimeType &)
{
}
#endif


status_t
BMimeType::GetSupportedTypes(BMessage* types)
{
	if (types == NULL)
		return B_BAD_VALUE;

	status_t err = InitCheck();
	if (err == B_OK)
		err = default_database_location()->GetSupportedTypes(Type(), *types);

	return err;
}


/*!	Sets the list of MIME types supported by the MIME type (which is
	assumed to be an application signature).

	If \a types is \c NULL the application's supported types are unset.

	The supported MIME types must be stored in a field "types" of type
	\c B_STRING_TYPE in \a types.

	For each supported type the result of BMimeType::GetSupportingApps() will
	afterwards include the signature of this application.

	\a fullSync specifies whether or not any types that are no longer
	listed as supported types as of this call to SetSupportedTypes() shall be
	updated as well, i.e. whether this application shall be removed from their
	lists of supporting applications.

	If \a fullSync is \c false, this application will not be removed from the
	previously supported types' supporting apps lists until the next call
	to BMimeType::SetSupportedTypes() or BMimeType::DeleteSupportedTypes()
	with a \c true \a fullSync parameter, the next call to BMimeType::Delete(),
	or the next reboot.

	\param types The supported types to be assigned to the file.
	       May be \c NULL.
	\param fullSync \c true to also synchronize the previously supported
	       types, \c false otherwise.

	\returns \c B_OK on success or another error code on failure.
*/
status_t
BMimeType::SetSupportedTypes(const BMessage* types, bool fullSync)
{
	Database* database;
	status_t status = InitCheck();
	if (status == B_OK)
		status = default_database_status(database);
	if (status != B_OK)
		return status;

	if (types != NULL)
		return database->SetSupportedTypes(Type(), types, fullSync);

	BMessage existingTypes;
	status_t existingStatus = default_database_location()->GetSupportedTypes(Type(),
		existingTypes);
	status = database->DeleteSupportedTypes(Type(), fullSync);
	if (status == B_OK && existingStatus == B_ENTRY_NOT_FOUND)
		return B_ENTRY_NOT_FOUND;

	return status;
}


/*!	Returns a list of mime types associated with the given file extension

	The list of types is returned in the pre-allocated \c BMessage pointed to
	by \a types. The types are stored in the message's "types" field, which
	is an array of \c B_STRING_TYPE values.

	\param extension The file extension of interest
	\param types Pointer to a pre-allocated BMessage into which the result will
	       be stored.

	\returns \c B_OK on success or another error code on failure.
*/
status_t
BMimeType::GetAssociatedTypes(const char* extension, BMessage* types)
{
	return B_UNSUPPORTED;
}

