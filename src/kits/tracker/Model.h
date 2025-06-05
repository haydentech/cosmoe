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
#ifndef _NU_MODEL_H
#define _NU_MODEL_H


#include <AppFileInfo.h>
#include <Debug.h>
#include <Mime.h>
#include <StorageDefs.h>
#include <String.h>

#include "IconCache.h"
#include "ObjectList.h"


class BPath;
class BHandler;
class BEntry;



#if __GNUC__ && __GNUC__ < 3
// using std::stat instead of just stat here because of what
// seems to be a gcc bug involving namespace and struct stat interaction
typedef struct std::stat StatStruct;
#else
// on mwcc std isn't turned on but there is no bug either.
// Also seems to be fixed in gcc 3.
typedef struct stat StatStruct;
#endif


namespace BPrivate {

enum {
	kDoesNotSupportType,
	kSuperhandlerModel,
	kModelSupportsSupertype,
	kModelSupportsType,
	kModelSupportsFile
};


class Model {
public:
	Model();
	Model(const Model& other);
	Model(const BEntry* entry, bool open = false, bool writable = false);
	Model(const entry_ref*, bool traverse = false, bool open = false,
		bool writable = false);
	~Model();

	Model& operator=(const Model&);

	status_t InitCheck() const;

	status_t SetTo(const BEntry*, bool open = false,
		bool writable = false);
	status_t SetTo(const entry_ref*, bool traverse = false,
		bool open = false, bool writable = false);
	status_t SetTo(const node_ref* dirNode, const node_ref* node,
		const char* name, bool open = false, bool writable = false);

	// basic getters
	const char* Name() const;
	const entry_ref* EntryRef() const;
	const StatStruct* StatBuf() const;

	void GetPath(BPath*) const;
	void GetEntry(BEntry*) const;

	// type getters
	bool IsContainer() const;
	bool IsDesktop() const;
	bool IsDirectory() const;
	bool IsFile() const;
	bool IsQuery() const;
	bool IsQueryTemplate() const;
	bool IsExecutable() const;
	bool IsPrintersDir() const;
	bool IsSymLink() const;
	bool InRoot() const;
	bool IsRoot() const;
	bool InTrash() const;
	bool IsTrash() const;
	bool IsVolume() const;
	bool IsVirtualDirectory() const;

	// symlink handling calls, mainly used by the IconCache
	const Model* ResolveIfLink() const;
	Model* ResolveIfLink();
		// works on anything
	Model* LinkTo() const;
		// fast, works only on symlinks
	void SetLinkTo(Model*);

private:
	void SetupBaseType();

	enum NodeType {
		kPlainNode,
		kExecutableNode,
		kDirectoryNode,
		kLinkNode,
		kQueryNode,
		kQueryTemplateNode,
		kVolumeNode,
		kRootNode,
		kTrashNode,
		kDesktopNode,
		kVirtualDirectoryNode,
		kUnknownNode
	};

	entry_ref fEntryRef;
	StatStruct fStatBuf;

	// bit of overloading hackery here to save on footprint
	union {
		char* fPreferredAppName;	// used if we are neither a volume
									// nor a symlink
		char* fVolumeName;			// used if we are a volume
		Model* fLinkTo;				// used if we are a symlink
	};

	uint8 fBaseType;
	status_t fStatus;
};



// inlines follow -----------------------------------



inline const entry_ref*
Model::EntryRef() const
{
	return &fEntryRef;
}


inline Model*
Model::LinkTo() const
{
	ASSERT(IsSymLink());
	return fLinkTo;
}


inline bool
Model::IsContainer() const
{
	// I guess as in should show container window -
	// volumes show the volume window
	return IsQuery() || IsDirectory() || IsVirtualDirectory();
}


inline bool
Model::IsDesktop() const
{
	return fBaseType == kDesktopNode;
}


inline bool
Model::IsDirectory() const
{
	switch (fBaseType) {
		case kDirectoryNode:
		case kVolumeNode:
		case kRootNode:
		case kTrashNode:
		case kDesktopNode:
			return true;
	}

	return false;
}


inline bool
Model::IsFile() const
{
	switch (fBaseType) {
		case kPlainNode:
		case kQueryNode:
		case kQueryTemplateNode:
		case kExecutableNode:
		case kVirtualDirectoryNode:
			return true;
	}

	return false;
}


inline bool
Model::IsExecutable() const
{
	return fBaseType == kExecutableNode;
}


inline bool
Model::IsQuery() const
{
	return fBaseType == kQueryNode;
}


inline bool
Model::IsQueryTemplate() const
{
	return fBaseType == kQueryTemplateNode;
}


inline bool
Model::IsSymLink() const
{
	return fBaseType == kLinkNode;
}


inline bool
Model::IsRoot() const
{
	return fBaseType == kRootNode;
}


inline bool
Model::IsTrash() const
{
	return fBaseType == kTrashNode;
}


inline bool
Model::IsVirtualDirectory() const
{
	return fBaseType == kVirtualDirectoryNode;
}


inline bool
Model::IsVolume() const
{
	return fBaseType == kVolumeNode;
}

} // namespace BPrivate


#endif	// _NU_MODEL_H
