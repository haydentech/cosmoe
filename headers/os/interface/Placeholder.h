/*
 * Copyright 2005-2015 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _BOX_H
#define _BOX_H


#include <View.h>


class BPlaceholder : public BView {
	public:
							BPlaceholder(BRect frame, const char* name = NULL,
								uint32 resizingMode = B_FOLLOW_ALL,
								uint32 flags = B_WILL_DRAW | B_FRAME_EVENTS
									| B_NAVIGABLE_JUMP);

		virtual				~BPlaceholder();

		virtual	void		Draw(BRect updateRect);

	private:

		void				_InitObject(BMessage* data = NULL);

		BRect				fBounds;
};

#endif	// _BOX_H
