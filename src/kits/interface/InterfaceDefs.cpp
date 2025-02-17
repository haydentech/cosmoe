/*
 * Copyright 2001-2015, Haiku, Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		DarkWyrm <bpmagic@columbus.rr.com>
 *		Caz <turok2@currantbun.com>
 *		Axel Dörfler, axeld@pinc-software.de
 *		Michael Lotz <mmlr@mlotz.ch>
 *		Wim van der Meer <WPJvanderMeer@gmail.com>
 *		Joseph Groover <looncraz@looncraz.net>
 */


/*!	Global functions and variables for the Interface Kit */


#include <InterfaceDefs.h>

#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <Point.h>
#include <Window.h>

#include <ColorConversion.h>
#include <ServerReadOnlyMemory.h>
#include <DefaultColors.h>

using namespace BPrivate;

// some other weird struct exported by BeOS, it's not initialized, though
struct general_ui_info {
	rgb_color	background_color;
	rgb_color	mark_color;
	rgb_color	highlight_color;
	bool		color_frame;
	rgb_color	window_frame_color;
};

struct general_ui_info general_info;


extern "C" const char B_NOTIFICATION_SENDER[] = "be:sender";

static const rgb_color _kDefaultColors[kColorWhichCount] = {
	{216, 216, 216, 255},	// B_PANEL_BACKGROUND_COLOR
	{216, 216, 216, 255},	// B_MENU_BACKGROUND_COLOR
	{255, 203, 0, 255},		// B_WINDOW_TAB_COLOR
	{0, 0, 229, 255},		// B_KEYBOARD_NAVIGATION_COLOR
	{51, 102, 152, 255},	// B_DESKTOP_COLOR
	{153, 153, 153, 255},	// B_MENU_SELECTED_BACKGROUND_COLOR
	{0, 0, 0, 255},			// B_MENU_ITEM_TEXT_COLOR
	{0, 0, 0, 255},			// B_MENU_SELECTED_ITEM_TEXT_COLOR
	{0, 0, 0, 255},			// B_MENU_SELECTED_BORDER_COLOR
	{0, 0, 0, 255},			// B_PANEL_TEXT_COLOR
	{255, 255, 255, 255},	// B_DOCUMENT_BACKGROUND_COLOR
	{0, 0, 0, 255},			// B_DOCUMENT_TEXT_COLOR
	{245, 245, 245, 255},	// B_CONTROL_BACKGROUND_COLOR
	{0, 0, 0, 255},			// B_CONTROL_TEXT_COLOR
	{172, 172, 172, 255},	// B_CONTROL_BORDER_COLOR
	{102, 152, 203, 255},	// B_CONTROL_HIGHLIGHT_COLOR
	{0, 0, 0, 255},			// B_NAVIGATION_PULSE_COLOR
	{255, 255, 255, 255},	// B_SHINE_COLOR
	{0, 0, 0, 255},			// B_SHADOW_COLOR
	{255, 255, 216, 255},	// B_TOOLTIP_BACKGROUND_COLOR
	{0, 0, 0, 255},			// B_TOOLTIP_TEXT_COLOR
	{0, 0, 0, 255},			// B_WINDOW_TEXT_COLOR
	{232, 232, 232, 255},	// B_WINDOW_INACTIVE_TAB_COLOR
	{80, 80, 80, 255},		// B_WINDOW_INACTIVE_TEXT_COLOR
	{224, 224, 224, 255},	// B_WINDOW_BORDER_COLOR
	{232, 232, 232, 255},	// B_WINDOW_INACTIVE_BORDER_COLOR
	{27, 82, 140, 255},     // B_CONTROL_MARK_COLOR
	{255, 255, 255, 255},	// B_LIST_BACKGROUND_COLOR
	{190, 190, 190, 255},	// B_LIST_SELECTED_BACKGROUND_COLOR
	{0, 0, 0, 255},			// B_LIST_ITEM_TEXT_COLOR
	{0, 0, 0, 255},			// B_LIST_SELECTED_ITEM_TEXT_COLOR
	{216, 216, 216, 255},	// B_SCROLL_BAR_THUMB_COLOR
	{51, 102, 187, 255},	// B_LINK_TEXT_COLOR
	{102, 152, 203, 255},	// B_LINK_HOVER_COLOR
	{145, 112, 155, 255},	// B_LINK_VISITED_COLOR
	{121, 142, 203, 255},	// B_LINK_ACTIVE_COLOR
	{50, 150, 255, 255},	// B_STATUS_BAR_COLOR
	// 100...
	{46, 204, 64, 255},		// B_SUCCESS_COLOR
	{255, 65, 54, 255},		// B_FAILURE_COLOR
	{}
};
const rgb_color* BPrivate::kDefaultColors = &_kDefaultColors[0];


static const rgb_color _kDefaultColorsDark[kColorWhichCount] = {
	{43, 43, 43, 255},		// B_PANEL_BACKGROUND_COLOR
	{28, 28, 28, 255},		// B_MENU_BACKGROUND_COLOR
	{227, 73, 17, 255},		// B_WINDOW_TAB_COLOR
	{0, 0, 229, 255},		// B_KEYBOARD_NAVIGATION_COLOR
	{51, 102, 152, 255},	// B_DESKTOP_COLOR
	{90, 90, 90, 255},		// B_MENU_SELECTED_BACKGROUND_COLOR
	{255, 255, 255, 255},	// B_MENU_ITEM_TEXT_COLOR
	{255, 255, 255, 255},	// B_MENU_SELECTED_ITEM_TEXT_COLOR
	{0, 0, 0, 255},			// B_MENU_SELECTED_BORDER_COLOR
	{253, 253, 253, 255},	// B_PANEL_TEXT_COLOR
	{0, 0, 0, 255},			// B_DOCUMENT_BACKGROUND_COLOR
	{234, 234, 234, 255},	// B_DOCUMENT_TEXT_COLOR
	{29, 29, 29, 255},		// B_CONTROL_BACKGROUND_COLOR
	{230, 230, 230, 255},	// B_CONTROL_TEXT_COLOR
	{195, 195, 195, 255},	// B_CONTROL_BORDER_COLOR
	{75, 124, 168, 255},	// B_CONTROL_HIGHLIGHT_COLOR
	{0, 0, 0, 255},			// B_NAVIGATION_PULSE_COLOR
	{255, 255, 255, 255},	// B_SHINE_COLOR
	{0, 0, 0, 255},			// B_SHADOW_COLOR
	{76, 68, 79, 255},		// B_TOOLTIP_BACKGROUND_COLOR
	{255, 255, 255, 255},	// B_TOOLTIP_TEXT_COLOR
	{255, 255, 255, 255},	// B_WINDOW_TEXT_COLOR
	{203, 32, 9, 255},		// B_WINDOW_INACTIVE_TAB_COLOR
	{255, 255, 255, 255},	// B_WINDOW_INACTIVE_TEXT_COLOR
	{227, 73, 17, 255},		// B_WINDOW_BORDER_COLOR
	{203, 32, 9, 255},		// B_WINDOW_INACTIVE_BORDER_COLOR
	{27, 82, 140, 255},     // B_CONTROL_MARK_COLOR
	{0, 0, 0, 255},			// B_LIST_BACKGROUND_COLOR
	{90, 90, 90, 255},		// B_LIST_SELECTED_BACKGROUND_COLOR
	{255, 255, 255, 255},	// B_LIST_ITEM_TEXT_COLOR
	{255, 255, 255, 255},	// B_LIST_SELECTED_ITEM_TEXT_COLOR
	{39, 39, 39, 255},		// B_SCROLL_BAR_THUMB_COLOR
	{106, 112, 212, 255},	// B_LINK_TEXT_COLOR
	{102, 152, 203, 255},	// B_LINK_HOVER_COLOR
	{145, 112, 155, 255},	// B_LINK_VISITED_COLOR
	{121, 142, 203, 255},	// B_LINK_ACTIVE_COLOR
	{50, 150, 255, 255},	// B_STATUS_BAR_COLOR
	// 100...
	{46, 204, 64, 255},		// B_SUCCESS_COLOR
	{255, 40, 54, 255},		// B_FAILURE_COLOR
	{}
};


static const char* kColorNames[kColorWhichCount] = {
	"B_PANEL_BACKGROUND_COLOR",
	"B_MENU_BACKGROUND_COLOR",
	"B_WINDOW_TAB_COLOR",
	"B_KEYBOARD_NAVIGATION_COLOR",
	"B_DESKTOP_COLOR",
	"B_MENU_SELECTED_BACKGROUND_COLOR",
	"B_MENU_ITEM_TEXT_COLOR",
	"B_MENU_SELECTED_ITEM_TEXT_COLOR",
	"B_MENU_SELECTED_BORDER_COLOR",
	"B_PANEL_TEXT_COLOR",
	"B_DOCUMENT_BACKGROUND_COLOR",
	"B_DOCUMENT_TEXT_COLOR",
	"B_CONTROL_BACKGROUND_COLOR",
	"B_CONTROL_TEXT_COLOR",
	"B_CONTROL_BORDER_COLOR",
	"B_CONTROL_HIGHLIGHT_COLOR",
	"B_NAVIGATION_PULSE_COLOR",
	"B_SHINE_COLOR",
	"B_SHADOW_COLOR",
	"B_TOOLTIP_BACKGROUND_COLOR",
	"B_TOOLTIP_TEXT_COLOR",
	"B_WINDOW_TEXT_COLOR",
	"B_WINDOW_INACTIVE_TAB_COLOR",
	"B_WINDOW_INACTIVE_TEXT_COLOR",
	"B_WINDOW_BORDER_COLOR",
	"B_WINDOW_INACTIVE_BORDER_COLOR",
	"B_CONTROL_MARK_COLOR",
	"B_LIST_BACKGROUND_COLOR",
	"B_LIST_SELECTED_BACKGROUND_COLOR",
	"B_LIST_ITEM_TEXT_COLOR",
	"B_LIST_SELECTED_ITEM_TEXT_COLOR",
	"B_SCROLL_BAR_THUMB_COLOR",
	"B_LINK_TEXT_COLOR",
	"B_LINK_HOVER_COLOR",
	"B_LINK_VISITED_COLOR",
	"B_LINK_ACTIVE_COLOR",
	"B_STATUS_BAR_COLOR",
	// 100...
	"B_SUCCESS_COLOR",
	"B_FAILURE_COLOR",
	NULL
};



//namespace BPrivate {


/*!	Fills the \a width, \a height, and \a colorSpace parameters according
	to the window screen's mode.
	Returns \c true if the mode is known.
*/
bool
get_mode_parameter(uint32 mode, int32& width, int32& height,
	uint32& colorSpace)
{
	switch (mode) {
		case B_8_BIT_640x480:
		case B_8_BIT_800x600:
		case B_8_BIT_1024x768:
		case B_8_BIT_1152x900:
		case B_8_BIT_1280x1024:
		case B_8_BIT_1600x1200:
			colorSpace = B_CMAP8;
			break;

		case B_15_BIT_640x480:
		case B_15_BIT_800x600:
		case B_15_BIT_1024x768:
		case B_15_BIT_1152x900:
		case B_15_BIT_1280x1024:
		case B_15_BIT_1600x1200:
			colorSpace = B_RGB15;
			break;

		case B_16_BIT_640x480:
		case B_16_BIT_800x600:
		case B_16_BIT_1024x768:
		case B_16_BIT_1152x900:
		case B_16_BIT_1280x1024:
		case B_16_BIT_1600x1200:
			colorSpace = B_RGB16;
			break;

		case B_32_BIT_640x480:
		case B_32_BIT_800x600:
		case B_32_BIT_1024x768:
		case B_32_BIT_1152x900:
		case B_32_BIT_1280x1024:
		case B_32_BIT_1600x1200:
			colorSpace = B_RGB32;
			break;

		default:
			return false;
	}

	switch (mode) {
		case B_8_BIT_640x480:
		case B_15_BIT_640x480:
		case B_16_BIT_640x480:
		case B_32_BIT_640x480:
			width = 640; height = 480;
			break;

		case B_8_BIT_800x600:
		case B_15_BIT_800x600:
		case B_16_BIT_800x600:
		case B_32_BIT_800x600:
			width = 800; height = 600;
			break;

		case B_8_BIT_1024x768:
		case B_15_BIT_1024x768:
		case B_16_BIT_1024x768:
		case B_32_BIT_1024x768:
			width = 1024; height = 768;
			break;

		case B_8_BIT_1152x900:
		case B_15_BIT_1152x900:
		case B_16_BIT_1152x900:
		case B_32_BIT_1152x900:
			width = 1152; height = 900;
			break;

		case B_8_BIT_1280x1024:
		case B_15_BIT_1280x1024:
		case B_16_BIT_1280x1024:
		case B_32_BIT_1280x1024:
			width = 1280; height = 1024;
			break;

		case B_8_BIT_1600x1200:
		case B_15_BIT_1600x1200:
		case B_16_BIT_1600x1200:
		case B_32_BIT_1600x1200:
			width = 1600; height = 1200;
			break;
	}

	return true;
}


rgb_color
ui_color(color_which which)
{
	int32 index = color_which_to_index(which);
	if (index < 0 || index >= kColorWhichCount) {
		fprintf(stderr, "ui_color(): unknown color_which %d\n", which);
		return make_color(0, 0, 0);
	}

	// // Cosmoe bug: this always returns black
	// if (false && be_app != NULL) {
	// 	server_read_only_memory* shared
	// 		= BApplication::Private::ServerReadOnlyMemory();
	// 	if (shared != NULL) {
	// 		// check for unset colors
	// 		if (shared->colors[index] == B_TRANSPARENT_COLOR)
	// 			shared->colors[index] = _kDefaultColors[index];

	// 		return shared->colors[index];
	// 	}
	// }

	return _kDefaultColors[index];
}


rgb_color
BPrivate::GetSystemColor(color_which colorConstant, bool darkVariant) {
	if (darkVariant) {
		return _kDefaultColorsDark[color_which_to_index(colorConstant)];
	} else {
		return _kDefaultColors[color_which_to_index(colorConstant)];
	}
}


rgb_color
tint_color(rgb_color color, float tint)
{
	rgb_color result;

	#define LIGHTEN(x) ((uint8)(255.0f - (255.0f - x) * tint))
	#define DARKEN(x)  ((uint8)(x * (2 - tint)))

	if (tint < 1.0f) {
		result.red   = LIGHTEN(color.red);
		result.green = LIGHTEN(color.green);
		result.blue  = LIGHTEN(color.blue);
		result.alpha = color.alpha;
	} else {
		result.red   = DARKEN(color.red);
		result.green = DARKEN(color.green);
		result.blue  = DARKEN(color.blue);
		result.alpha = color.alpha;
	}

	#undef LIGHTEN
	#undef DARKEN

	return result;
}


// rgb_color shift_color(rgb_color color, float shift);

rgb_color
shift_color(rgb_color color, float shift)
{
	return tint_color(color, shift);
}


extern "C" status_t
_init_interface_kit_()
{
	status_t status = BPrivate::PaletteConverter::InitializeDefault(false);
	if (status < B_OK)
		return status;

// 	// init global clipboard
// 	if (be_clipboard == NULL)
// 		be_clipboard = new BClipboard(NULL);

// 	// TODO: Could support different themes here in the future.
// 	be_control_look = new HaikuControlLook();

// 	_init_global_fonts_();

// 	BPrivate::gWidthBuffer = new BPrivate::WidthBuffer;
// 	status = BPrivate::MenuPrivate::CreateBitmaps();
// 	if (status != B_OK)
// 		return status;

// 	_menu_info_ptr_ = &BMenu::sMenuInfo;

// 	status = get_menu_info(&BMenu::sMenuInfo);
// 	if (status != B_OK)
// 		return status;

// 	general_info.background_color = ui_color(B_PANEL_BACKGROUND_COLOR);
// 	general_info.mark_color = ui_color(B_CONTROL_MARK_COLOR);
// 	general_info.highlight_color = ui_color(B_CONTROL_HIGHLIGHT_COLOR);
// 	general_info.window_frame_color = ui_color(B_WINDOW_TAB_COLOR);
// 	general_info.color_frame = true;

// 	// TODO: fill the other static members

	return status;
}


// extern "C" status_t
// _fini_interface_kit_()
// {
// 	BPrivate::MenuPrivate::DeleteBitmaps();

// 	delete BPrivate::gWidthBuffer;
// 	BPrivate::gWidthBuffer = NULL;

// 	delete be_control_look;
// 	be_control_look = NULL;

// 	// Note: if we ever want to support live switching, we cannot just unload
// 	// the old one since some thread might still be in a method of the object.
// 	// maybe locking/unlocking all loopers around would ensure proper exit.
// 	if (sControlLookAddon != NULL)
// 		unload_add_on(sControlLookAddon);
// 	sControlLookAddon = NULL;

// 	// TODO: Anything else?

// 	return B_OK;
// }



//}