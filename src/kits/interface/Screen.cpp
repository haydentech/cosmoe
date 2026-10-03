/*
 * Copyright 2003-2009, Haiku Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Stefano Ceccherini (burton666@libero.it)
 *		Axel Dörfler, axeld@pinc-software.de
 */


#include <Screen.h>

#include <Application.h>
#include <Window.h>

//#include <PrivateScreen.h>


using namespace BPrivate;


/*!	\brief Creates a BScreen object which represents the main display with the given
		screen_id
	\param id The screen_id of the screen to get.

	In the current simplified implementation, there is only one display (B_MAIN_SCREEN_ID).
*/
BScreen::BScreen(screen_id id)
{
}


/*!	\brief Creates a BScreen object which represents the display which contains
	the given BWindow.
	\param window A BWindow.
*/
BScreen::BScreen(BWindow* window)
{
}


/*!	\brief Releases the resources allocated by the constructor.
*/ 
BScreen::~BScreen()
{
}


/*! \brief Checks if the BScreen object represents a real screen connected to
		the computer.
	\return \c true if the BScreen object is valid, \c false if not.
*/
bool
BScreen::IsValid()
{
	return true;
}


/*!	\brief Returns the color space of the screen display.
	\return \c B_CMAP8, \c B_RGB15, or \c B_RGB32, or \c B_NO_COLOR_SPACE
		if the screen object is invalid.
*/
color_space
BScreen::ColorSpace()
{
	return B_RGB32;
}


/*!	\brief Returns the rectangle that locates the screen in the screen
		coordinate system.
	\return a BRect that locates the screen in the screen coordinate system.
*/
BRect
BScreen::Frame()
{
	struct rectangle allocation;
	cosmoe_display_get_screen_dimensions(be_app->Display(), &allocation);

	return BRect(allocation.x, allocation.y, allocation.x + allocation.width, allocation.y + allocation.height);
}


/*!	\brief Returns the identifier for the screen.
	\return A screen_id struct that identifies the screen.

	In the current implementation, this function always returns
	\c B_MAIN_SCREEN_ID, even if the object is invalid.
*/
screen_id
BScreen::ID()
{
	return B_MAIN_SCREEN_ID;
}


uint8
BScreen::IndexForColor(uint8 red, uint8 green, uint8 blue, uint8 alpha)
{
	if (red == B_TRANSPARENT_32_BIT.red
		&& green == B_TRANSPARENT_32_BIT.green
		&& blue == B_TRANSPARENT_32_BIT.blue
		&& alpha == B_TRANSPARENT_32_BIT.alpha) {
		return B_TRANSPARENT_8_BIT;
	}

	const color_map* colorMap = ColorMap();
	uint16 index = ((red & 0xf8) << 7) | ((green & 0xf8) << 2) | (blue >> 3);
	return colorMap != NULL ? colorMap->index_map[index] : 0;
}


rgb_color
BScreen::ColorForIndex(const uint8 index)
{
	const color_map* colorMap = ColorMap();
	if (colorMap != NULL)
		return colorMap->color_list[index];

	return rgb_color();
}


uint8
BScreen::InvertIndex(uint8 index)
{
	const color_map* colorMap = ColorMap();
	if (colorMap != NULL)
		return colorMap->inversion_map[index];

	return 0;
}


const color_map*
BScreen::ColorMap()
{
	return system_colors();
}


rgb_color
BScreen::DesktopColor()
{
	rgb_color color = { 51, 102, 152, 255 };

	return color;
}


rgb_color
BScreen::DesktopColor(uint32 workspace)
{
	rgb_color color = { 51, 102, 152, 255 };

	return color;
}


void
BScreen::SetDesktopColor(rgb_color color, bool stick)
{
}


void
BScreen::SetDesktopColor(rgb_color color, uint32 workspace, bool stick)
{
}
