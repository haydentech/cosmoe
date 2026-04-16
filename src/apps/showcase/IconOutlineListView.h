#ifndef ICON_OUTLINE_LIST_VIEW_H
#define ICON_OUTLINE_LIST_VIEW_H

#include <Bitmap.h>
#include <OutlineListView.h>
#include <StringItem.h>


class BIconStringItem : public BStringItem {
public:
						BIconStringItem(const char* text,
							const BBitmap* icon = NULL,
							uint32 outlineLevel = 0,
							bool expanded = true);
	virtual				~BIconStringItem();

						void SetIcon(const BBitmap* icon);
						void ClearIcon();
	const BBitmap*			Icon() const;

	virtual	void			DrawItem(BView* owner, BRect frame,
							bool complete = false);
	virtual	void			Update(BView* owner, const BFont* font);

private:
						BBitmap* _CloneBitmap(const BBitmap* source) const;

private:
	BBitmap*				fIcon;
};


class BIconOutlineListView : public BOutlineListView {
public:
						BIconOutlineListView(BRect frame, const char* name,
							list_view_type type = B_SINGLE_SELECTION_LIST,
							uint32 resizingMode = B_FOLLOW_LEFT_TOP,
							uint32 flags = B_WILL_DRAW | B_FRAME_EVENTS
								| B_NAVIGABLE);
						BIconOutlineListView(const char* name,
							list_view_type type = B_SINGLE_SELECTION_LIST,
							uint32 flags = B_WILL_DRAW | B_FRAME_EVENTS
								| B_NAVIGABLE);
	virtual				~BIconOutlineListView();
};

#endif
