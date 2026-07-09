/*
 * Copyright 2007-2009, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _SCREEN_H
#define _SCREEN_H


#include <GraphicsDefs.h>
#include <Rect.h>
#include <OS.h>


class BBitmap;
class BWindow;


class BScreen {
public:  
								BScreen(screen_id id = B_MAIN_SCREEN_ID);
								BScreen(BWindow* window);
								~BScreen();

			bool				IsValid();
			status_t			SetToNext();

			color_space			ColorSpace();
			BRect				Frame();
			screen_id			ID();

			const color_map*	ColorMap();

			rgb_color			DesktopColor();
			rgb_color			DesktopColor(uint32 workspace);
			void				SetDesktopColor(rgb_color color,
									bool stick = true);
			void				SetDesktopColor(rgb_color color,
									uint32 workspace, bool stick = true);

private:
	// Forbidden and deprecated methods
								BScreen(const BScreen& other);
			BScreen&			operator=(const BScreen& other);
};


#endif // _SCREEN_H
