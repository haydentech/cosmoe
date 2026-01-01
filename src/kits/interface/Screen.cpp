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


