/*
Open Tracker License

Terms and Conditions

Copyright (c) 1991-2000, Be Incorporated. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice applies to all licensees
and shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF TITLE, MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
BE INCORPORATED BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF, OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of Be Incorporated shall not be
used in advertising or otherwise to promote the sale, use or other dealings in
this Software without prior written authorization from Be Incorporated.

Tracker(TM), Be(R), BeOS(R), and BeIA(TM) are trademarks or registered trademarks
of Be Incorporated in the United States and other countries. Other brand product
names are registered trademarks or trademarks of their respective holders.
All rights reserved.
*/

//	Dedicated to BModel


#include "Model.h"

#include <stdlib.h>
#include <strings.h>

#include <fs_info.h>
#include <fs_attr.h>

#include <Volume.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Model"



//	#pragma mark - Model()


Model::Model()
	:
	fBaseType(kUnknownNode),
	fWritable(false),
	fNode(NULL),
	fStatus(B_NO_INIT),
	fHasLocalizedName(false),
	fLocalizedNameIsCached(false)
{
}


Model::Model(const Model& other)
	:
	fEntryRef(other.fEntryRef),
	fMimeType(other.fMimeType),
	fBaseType(other.fBaseType),
	fWritable(false),
	fNode(NULL),
	fLocalizedName(other.fLocalizedName),
	fHasLocalizedName(other.fHasLocalizedName),
	fLocalizedNameIsCached(other.fLocalizedNameIsCached)
{
	if (other.IsSymLink() && other.LinkTo())
		fLinkTo = new Model(*other.LinkTo());
}


Model::Model(const BEntry* entry, bool open, bool writable)
	:
	fWritable(false),
	fNode(NULL),
	fHasLocalizedName(false),
	fLocalizedNameIsCached(false)
{
	SetTo(entry, open, writable);
}


Model::Model(const entry_ref* ref, bool traverse, bool open, bool writable)
	:
	fBaseType(kUnknownNode),
	fWritable(false),
	fNode(NULL),
	fHasLocalizedName(false),
	fLocalizedNameIsCached(false)
{
	BEntry entry(ref, traverse);
	fStatus = entry.InitCheck();
	if (fStatus == B_OK)
		SetTo(&entry, open, writable);
}


Model::~Model()
{
	delete fNode;
}


status_t
Model::SetTo(const BEntry* entry, bool open, bool writable)
{
	delete fNode;
	fNode = NULL;
	fBaseType = kUnknownNode;
	fMimeType = "";

	fStatus = entry->GetRef(&fEntryRef);
	if (fStatus != B_OK)
		return fStatus;

	fStatus = entry->GetStat(&fStatBuf);
	if (fStatus != B_OK)
		return fStatus;

	SetupBaseType();
	
	return fStatus;
}


status_t
Model::SetTo(const entry_ref* newRef, bool traverse, bool open, bool writable)
{
	delete fNode;
	fNode = NULL;
	fBaseType = kUnknownNode;
	fMimeType = "";

	BEntry tmpEntry(newRef, traverse);
	fStatus = tmpEntry.InitCheck();
	if (fStatus != B_OK)
		return fStatus;

	if (traverse)
		tmpEntry.GetRef(&fEntryRef);
	else
		fEntryRef = *newRef;

	fStatus = tmpEntry.GetStat(&fStatBuf);
	if (fStatus != B_OK)
		return fStatus;

	SetupBaseType();

	return fStatus;
}


status_t
Model::InitCheck() const
{
	return fStatus;
}


int
Model::CompareFolderNamesFirst(const Model* compare) const
{
	if (compare == NULL)
		return -1;

	const Model* resolved = ResolveIfLink();
	const Model* resolvedCompare = compare->ResolveIfLink();

	bool meIsRoot = resolved->IsRoot();
	bool otherIsRoot = resolvedCompare->IsRoot();

	// sort root directory first

	if (meIsRoot && !otherIsRoot)
		return -1;
	else if (!meIsRoot && otherIsRoot)
		return 1;

	bool meIsVolume = resolved->IsVolume();
	bool otherIsVolume = resolvedCompare->IsVolume();

	// sort volume as a directory if capacity is 0

	if (meIsVolume) {
		BVolume volume(resolved->NodeRef()->device);
		if (volume.InitCheck() == B_OK && volume.Capacity() == 0)
			meIsVolume = false;
	}

	if (otherIsVolume) {
		BVolume volume(resolvedCompare->NodeRef()->device);
		if (volume.InitCheck() == B_OK && volume.Capacity() == 0)
			otherIsVolume = false;
	}

	// sort by volume then by directory then by file name

	if (meIsVolume && !otherIsVolume)
		return -1;
	else if (!meIsVolume && otherIsVolume)
		return 1;

	bool meIsDir = resolved->IsDirectory() || resolved->IsVirtualDirectory();
	bool otherIsDir = resolvedCompare->IsDirectory() || resolvedCompare->IsVirtualDirectory();

	if (meIsDir && !otherIsDir)
		return -1;
	else if (!meIsDir && otherIsDir)
		return 1;

	return NaturalCompare(Name(), compare->Name());
}


const char*
Model::Name() const
{
	return fEntryRef.name;
}


void
Model::SetupBaseType()
{
	switch (fStatBuf.st_mode & S_IFMT) {
		case S_IFDIR:
			// folder
			fBaseType = kDirectoryNode;
			break;

		case S_IFREG:
			// regular file
			if ((fStatBuf.st_mode & S_IXUSR) != 0) {
				// executable
				fBaseType = kExecutableNode;
			} else {
				// non-executable
				fBaseType = kPlainNode;
			}
			break;

#ifdef S_IFLNK
		case S_IFLNK:
			// symlink
			fBaseType = kLinkNode;
			break;
#endif

		default:
			fBaseType = kUnknownNode;
			break;
	}
}


bool
Model::InTrash() const
{
	return false;
}


const Model*
Model::ResolveIfLink() const
{
	if (!IsSymLink())
		return this;

	if (!fLinkTo)
		return this;

	return fLinkTo;
}


Model*
Model::ResolveIfLink()
{
	if (!IsSymLink())
		return this;

	if (!fLinkTo)
		return this;

	return fLinkTo;
}


void
Model::SetLinkTo(Model* model)
{
	ASSERT(IsSymLink());
	ASSERT(!fLinkTo || (fLinkTo != model));

	delete fLinkTo;
	fLinkTo = model;
}


void
Model::GetEntry(BEntry* entry) const
{
	entry->SetTo(EntryRef());
}


void
Model::GetPath(BPath* path) const
{
	BEntry entry(EntryRef());
	entry.GetPath(path);
}


ssize_t
Model::WriteAttr(const char* attr, type_code type, off_t offset,
	const void* buffer, size_t length)
{
	if (!fNode)
		return 0;

	ssize_t result = fNode->WriteAttr(attr, type, offset, buffer, length);
	return result;
}


status_t
Model::GetLongVersionString(BString &result, version_kind kind)
{
	BFile file(EntryRef(), O_RDONLY);
	status_t error = file.InitCheck();
	if (error != B_OK)
		return error;

	BAppFileInfo info(&file);
	error = info.InitCheck();
	if (error != B_OK)
		return error;

	version_info version;
	error = info.GetVersionInfo(&version, kind);
	if (error != B_OK)
		return error;

	result = version.long_info;
	return B_OK;
}


status_t
Model::GetVersionString(BString &result, version_kind kind)
{
	BFile file(EntryRef(), O_RDONLY);
	status_t error = file.InitCheck();
	if (error != B_OK)
		return error;

	BAppFileInfo info(&file);
	error = info.InitCheck();
	if (error != B_OK)
		return error;

	version_info version;
	error = info.GetVersionInfo(&version, kind);
	if (error != B_OK)
		return error;

	result.SetToFormat("%" B_PRId32 ".%" B_PRId32 ".%" B_PRId32, version.major,
		version.middle, version.minor);

	return B_OK;
}


