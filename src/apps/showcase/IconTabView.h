#ifndef ICON_TAB_VIEW_H
#define ICON_TAB_VIEW_H

#include <TabView.h>

class BIconTabView : public BTabView {
public:
						BIconTabView(const char* name,
							button_width width = B_WIDTH_FROM_WIDEST,
							uint32 flags = B_FULL_UPDATE_ON_RESIZE
								| B_WILL_DRAW | B_NAVIGABLE_JUMP
								| B_FRAME_EVENTS | B_NAVIGABLE);
						BIconTabView(BRect frame, const char* name,
							button_width width = B_WIDTH_AS_USUAL,
							uint32 resizeMask = B_FOLLOW_ALL,
							uint32 flags = B_FULL_UPDATE_ON_RESIZE
								| B_WILL_DRAW | B_NAVIGABLE_JUMP
								| B_FRAME_EVENTS | B_NAVIGABLE);

	virtual	BRect			TabFrame(int32 index) const;

private:
						float _TabContentWidth(int32 index,
							float padding) const;
};

#endif
