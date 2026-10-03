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

			uint8				IndexForColor(rgb_color color);
			uint8				IndexForColor(uint8 red, uint8 green,
									uint8 blue, uint8 alpha = 255);
			rgb_color			ColorForIndex(uint8 index);
			uint8				InvertIndex(uint8 index);

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


inline uint8
BScreen::IndexForColor(rgb_color color)
{
	return IndexForColor(color.red, color.green, color.blue, color.alpha);
}

#endif // _SCREEN_H
