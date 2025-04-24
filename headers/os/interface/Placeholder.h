#ifndef _PLACEHOLDER_H
#define _PLACEHOLDER_H


#include <View.h>


class BPlaceholder : public BView {
	public:
							BPlaceholder(BRect frame, const char* name = NULL,
								uint32 resizingMode = B_FOLLOW_ALL,
								uint32 flags = B_WILL_DRAW | B_FRAME_EVENTS
									| B_NAVIGABLE_JUMP);

		virtual				~BPlaceholder();

		virtual	void		Draw(BRect updateRect);
};

#endif	// _PLACEHOLDER_H
