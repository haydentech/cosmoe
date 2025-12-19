/*
 * Copyright 2001-2009, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _SHELF_H
#define _SHELF_H

#include <Handler.h>

class BDataIO;
class BPoint;
class BView;
class BEntry;

class BShelf : public BHandler {
public:
								BShelf(BView* view, bool allowDrags = true,
									const char* shelfType = NULL);
								BShelf(const entry_ref* ref, BView* view,
									bool allowDrags = true,
									const char* shelfType = NULL);
								BShelf(BDataIO* stream, BView* view,
									bool allowDrags = true,
									const char* shelfType = NULL);
								BShelf(BMessage* archive);
	virtual						~BShelf();

	static BArchivable*			Instantiate(BMessage* archive);
	virtual	status_t			Archive(BMessage* archive,
									bool deep = true) const;

	virtual	void				MessageReceived(BMessage* message);
			status_t			Save();
	virtual	void				SetDirty(bool state);
			bool				IsDirty() const;

	virtual	BHandler*			ResolveSpecifier(BMessage* message,
									int32 index, BMessage* specifier,
									int32 form, const char* property);
	virtual	status_t			GetSupportedSuites(BMessage* data);

	virtual	status_t			Perform(perform_code code, void* data);

			bool				AllowsDragging() const;
			void				SetAllowsDragging(bool state);
			bool				AllowsZombies() const;
			void				SetAllowsZombies(bool state);
			bool				DisplaysZombies() const;
			void				SetDisplaysZombies(bool state);
			bool				IsTypeEnforced() const;
			void				SetTypeEnforced(bool state);

			status_t			SetSaveLocation(BDataIO* stream);
			status_t			SetSaveLocation(const entry_ref* ref);
			BDataIO*			SaveLocation(entry_ref* ref) const;

			status_t			AddReplicant(BMessage* archive,
									BPoint location);
			status_t			DeleteReplicant(BView* replicant);
			status_t			DeleteReplicant(BMessage* archive);
			status_t			DeleteReplicant(int32 index);
			int32				CountReplicants() const;
			BMessage*			ReplicantAt(int32 index, BView** view = NULL,
									uint32* uid = NULL,
									status_t* perr = NULL) const;
			int32				IndexOf(const BView* replicantView) const;
			int32				IndexOf(const BMessage* archive) const;
			int32				IndexOf(uint32 id) const;

protected:
	virtual	bool				CanAcceptReplicantMessage(
									BMessage* archive) const;
	virtual	bool				CanAcceptReplicantView(BRect,
									BView*, BMessage*) const;
	virtual	BPoint				AdjustReplicantBy(BRect, BMessage*) const;

	virtual	void				ReplicantDeleted(int32 index,
									const BMessage* archive,
									const BView *replicant);

};

#endif	/* _SHELF_H */
