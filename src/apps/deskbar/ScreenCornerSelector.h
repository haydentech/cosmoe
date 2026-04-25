/*
 * Copyright 2003-2013 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Michael Phipps
 *		Axel Dörfler, axeld@pinc-software.de
 */
#ifndef SCREEN_CORNER_SELECTOR_H
#define SCREEN_CORNER_SELECTOR_H


#include <Control.h>
#include <Deskbar.h>


class ScreenCornerSelector : public BControl {
public:
								ScreenCornerSelector(BRect frame,
									const char *name, BMessage* message,
									uint32 resizingMode);

	virtual	void				Draw(BRect updateRect);
	virtual	void				MouseDown(BPoint point);
	virtual	void				MouseUp(BPoint point);
	virtual	void				MouseMoved(BPoint where, uint32 transit,
									const BMessage* dragMessage);
	virtual	void				KeyDown(const char* bytes, int32 numBytes);

	virtual	void				SetValue(int32 value);
	virtual	int32				Value();
	virtual status_t			Invoke(BMessage* message);

			void				SetCorner(deskbar_location corner);
			deskbar_location	Corner() const;

private:
			BRect				_MonitorFrame() const;
			BRect				_InnerFrame(BRect monitorFrame) const;
			void				_DrawArrow(BRect innerFrame);
			int32				_ScreenCorner(BPoint point) const;

			int32				fCurrentCorner;
			bool				fDragging;
};


#endif	// SCREEN_CORNER_SELECTOR_H
