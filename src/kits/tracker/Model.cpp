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


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Model"



//	#pragma mark - Model()


Model::Model()
	:
	fBaseType(kUnknownNode),
	fStatus(B_NO_INIT)
{
}


Model::Model(const Model& other)
	:
	fEntryRef(other.fEntryRef),
	fBaseType(other.fBaseType)
{
}


Model::Model(const BEntry* entry, bool open, bool writable)
{
	SetTo(entry, open, writable);
}


Model::Model(const entry_ref* ref, bool traverse, bool open, bool writable)
	:
	fBaseType(kUnknownNode)
{
	BEntry entry(ref, traverse);
	fStatus = entry.InitCheck();
	if (fStatus == B_OK)
		SetTo(&entry, open, writable);
}


Model::~Model()
{
}


status_t
Model::SetTo(const BEntry* entry, bool open, bool writable)
{
	fBaseType = kUnknownNode;

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
	fBaseType = kUnknownNode;

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

		case S_IFLNK:
			// symlink
			fBaseType = kLinkNode;
			break;

		default:
			fBaseType = kUnknownNode;
			break;
	}
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


