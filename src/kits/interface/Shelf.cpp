/*
 * Copyright 2001-2010, Haiku, Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Marc Flerackers (mflerackers@androme.be)
 *		Axel Dörfler, axeld@pinc-software.de
 *		Jérôme Duval
 *		René Gollent
 *		Alexandre Deckner, alex@zappotek.com
 */

/*!	BShelf stores replicant views that are dropped onto it */

#include <Shelf.h>



//	#pragma mark -


BShelf::BShelf(BView *view, bool allowDrags, const char *shelfType)
	: BHandler(shelfType)
{
}


BShelf::BShelf(const entry_ref *ref, BView *view, bool allowDrags,
	const char *shelfType)
	: BHandler(shelfType)
{
}


BShelf::BShelf(BDataIO *stream, BView *view, bool allowDrags,
	const char *shelfType)
	: BHandler(shelfType)
{
}


BShelf::BShelf(BMessage *data)
	: BHandler(data)
{
}


BShelf::~BShelf()
{
}


status_t
BShelf::Archive(BMessage *data, bool deep) const
{
	return B_ERROR;
}


BArchivable *
BShelf::Instantiate(BMessage *data)
{
	return NULL;
}


void
BShelf::MessageReceived(BMessage *msg)
{
}


status_t
BShelf::Save()
{
	return B_ERROR;
}


void
BShelf::SetDirty(bool state)
{
}


bool
BShelf::IsDirty() const
{
	return false;
}


BHandler *
BShelf::ResolveSpecifier(BMessage *msg, int32 index, BMessage *specifier,
						int32 form, const char *property)
{
	return NULL;
}


status_t
BShelf::GetSupportedSuites(BMessage *message)
{
	return B_ERROR;
}


status_t
BShelf::Perform(perform_code d, void *arg)
{
	return B_ERROR;
}


bool
BShelf::AllowsDragging() const
{
	return false;
}


void
BShelf::SetAllowsDragging(bool state)
{
}


bool
BShelf::AllowsZombies() const
{
	return false;
}


void
BShelf::SetAllowsZombies(bool state)
{
}


bool
BShelf::DisplaysZombies() const
{
	return false;
}


void
BShelf::SetDisplaysZombies(bool state)
{
}


bool
BShelf::IsTypeEnforced() const
{
	return false;
}


void
BShelf::SetTypeEnforced(bool state)
{
}


status_t
BShelf::SetSaveLocation(BDataIO *data_io)
{
	return B_OK;
}


status_t
BShelf::SetSaveLocation(const entry_ref *ref)
{
	return B_OK;
}


BDataIO *
BShelf::SaveLocation(entry_ref *ref) const
{
	return NULL;
}


status_t
BShelf::AddReplicant(BMessage *data, BPoint location)
{
	return B_BAD_VALUE;
}


status_t
BShelf::DeleteReplicant(BView *replicant)
{
	return B_BAD_VALUE;
}


status_t
BShelf::DeleteReplicant(BMessage *data)
{
	return B_BAD_VALUE;
}


status_t
BShelf::DeleteReplicant(int32 index)
{
	return B_BAD_INDEX;
}


int32
BShelf::CountReplicants() const
{
	return 0;
}


BMessage *
BShelf::ReplicantAt(int32 index, BView **_view, uint32 *_uniqueID,
	status_t *_error) const
{
	if (_error)
		*_error = B_BAD_INDEX;

	return NULL;
}


int32
BShelf::IndexOf(const BView* replicantView) const
{
	return 0;
}


int32
BShelf::IndexOf(const BMessage *archive) const
{
	return 0;
}


int32
BShelf::IndexOf(uint32 id) const
{
	return 0;
}


bool
BShelf::CanAcceptReplicantMessage(BMessage*) const
{
	return false;
}


bool
BShelf::CanAcceptReplicantView(BRect, BView*, BMessage*) const
{
	return false;
}


BPoint
BShelf::AdjustReplicantBy(BRect, BMessage*) const
{
	return B_ORIGIN;
}


void
BShelf::ReplicantDeleted(int32 index, const BMessage *archive,
	const BView *replicant)
{
}

// End of file
