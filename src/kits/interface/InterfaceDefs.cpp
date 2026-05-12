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
#include <ctype.h>
#if !defined(_WIN32)
#include <unistd.h>
#endif

#include <Application.h>
#include <Bitmap.h>
#include <Clipboard.h>
#include <ControlLook.h>
#include <Font.h>
#include <Menu.h>
#include <Point.h>
#include <Roster.h>
#include <ScrollBar.h>
#include <String.h>
#include <TextView.h>
#include <Window.h>

#include <ColorConversion.h>
#include <CosmoeBackendAPI.h>
#include <ServerReadOnlyMemory.h>
#include <DefaultColors.h>
#include <HaikuControlLook.h>
#include <InputServerTypes.h>

#include <PathFinder.h>
#include <StringList.h>
#include <FindDirectory.h>
#include <image.h>
#include <sys/stat.h>
#include <Path.h>
#include <Directory.h>
#include <Entry.h>

#ifdef HAVE_PANGO
#include <pango/pangocairo.h>
#endif
#include <cairo.h>
#include <input_globals.h>
#include <InterfacePrivate.h>
#include <MenuPrivate.h>
#include <WidthBuffer.h>
#include <WindowInfo.h>

#include <Palette.h>

#ifdef _WIN32
#include <windows.h>
#endif

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

menu_info *_menu_info_ptr_;

// ControlLook add-on image id (if loaded dynamically)
static image_id sControlLookAddon = 0;
typedef BControlLook* (*instantiate_control_look_func)(image_id id);

// Helper to find control look addon directories similar to TranslatorRoster
static void
AddControlLookDefaultPaths(BStringList& paths)
{
	const directory_which addons_dirs[] = {
		B_USER_NONPACKAGED_ADDONS_DIRECTORY,
		B_USER_ADDONS_DIRECTORY,
		B_SYSTEM_NONPACKAGED_ADDONS_DIRECTORY,
		B_SYSTEM_ADDONS_DIRECTORY,
	};

	for (size_t i = 0; i < sizeof(addons_dirs) / sizeof(addons_dirs[0]); i++) {
		BPath path;
		if (find_directory(addons_dirs[i], &path, false) != B_OK)
			continue;
		if (path.Append("ControlLook") != B_OK)
			continue;

		// For user directories, ensure ControlLook exists so local addons can be installed
		if (i == 0 || i == 1)
#ifdef _WIN32
			mkdir(path.Path());
#else
			mkdir(path.Path(), 0755);
#endif

		paths.Add(path.Path());
	}
}

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
	{222, 222, 222, 255},	// B_CONTROL_BACKGROUND_COLOR
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



namespace BPrivate {


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


void
get_workspaces_layout(uint32* _columns, uint32* _rows)
{
	int32 columns = 1;
	int32 rows = 1;

	if (_columns != NULL)
		*_columns = columns;
	if (_rows != NULL)
		*_rows = rows;
}


void
set_workspaces_layout(uint32 columns, uint32 rows)
{
}


}	// namespace BPrivate

color_map sColorMap;

// color_distance
/*!	\brief Returns the "distance" between two RGB colors.

	This functions defines an metric on the RGB color space. The distance
	between two colors is 0, if and only if the colors are equal.

	\param red1 Red component of the first color.
	\param green1 Green component of the first color.
	\param blue1 Blue component of the first color.
	\param red2 Red component of the second color.
	\param green2 Green component of the second color.
	\param blue2 Blue component of the second color.
	\return The distance between the given colors.
*/
static inline uint32
color_distance(uint8 red1, uint8 green1, uint8 blue1,
			   uint8 red2, uint8 green2, uint8 blue2)
{
	int rd = (int)red1 - (int)red2;
	int gd = (int)green1 - (int)green2;
	int bd = (int)blue1 - (int)blue2;

	// distance according to psycho-visual tests
	// algorithm taken from here:
	// http://www.stud.uni-hannover.de/~michaelt/juggle/Algorithms.pdf
	int rmean = ((int)red1 + (int)red2) / 2;
	return (((512 + rmean) * rd * rd) >> 8)
			+ 4 * gd * gd
			+ (((767 - rmean) * bd * bd) >> 8);
}


static inline uint8
FindClosestColor(const rgb_color &color, const rgb_color *palette)
{
	uint8 closestIndex = 0;
	unsigned closestDistance = UINT_MAX;
	for (int32 i = 0; i < 256; i++) {
		const rgb_color &c = palette[i];
		unsigned distance = color_distance(color.red, color.green, color.blue,
										   c.red, c.green, c.blue);
		if (distance < closestDistance) {
			closestIndex = (uint8)i;
			closestDistance = distance;
		}
	}
	return closestIndex;
}


static inline rgb_color
InvertColor(const rgb_color &color)
{
	// For some reason, Inverting (255, 255, 255) on beos
	// results in the same color.
	if (color.red == 255 && color.green == 255
		&& color.blue == 255)
		return color;

	rgb_color inverted;
	inverted.red = 255 - color.red;
	inverted.green = 255 - color.green;
	inverted.blue = 255 - color.blue;
	inverted.alpha = 255;

	return inverted;
}


static void
FillColorMap(const rgb_color *palette, color_map *map)
{
	memcpy((void*)map->color_list, palette, sizeof(map->color_list));

	// init index map
	for (int32 color = 0; color < 32768; color++) {
		// get components
		rgb_color rgbColor;
		rgbColor.red = (color & 0x7c00) >> 7;
		rgbColor.green = (color & 0x3e0) >> 2;
		rgbColor.blue = (color & 0x1f) << 3;

		map->index_map[color] = FindClosestColor(rgbColor, palette);
	}

	// init inversion map
	for (int32 index = 0; index < 256; index++) {
		rgb_color inverted = InvertColor(map->color_list[index]);
		map->inversion_map[index] = FindClosestColor(inverted, palette);
	}
}


/*!	\brief Initializes the system color_map.
*/
void
InitializeColorMap()
{
	FillColorMap(kSystemPalette, &sColorMap);
}


/*!	\brief Returns a pointer to the system palette.
	\return A pointer to the system palette.
*/
// const rgb_color *
// SystemPalette()
// {
// 	return sColorMap.color_list;
// }


const color_map *
system_colors()
{
	static bool colorMapInitialized = false;
	if (!colorMapInitialized) {
		InitializeColorMap();
		colorMapInitialized = true;
	}

	return &sColorMap;
}


status_t
get_scroll_bar_info(scroll_bar_info *info)
{
	if (info == NULL)
		return B_BAD_VALUE;

	info->proportional = true;
	info->double_arrows = false;
	info->knob = 0;
	info->min_knob_size = 15;

	return B_ERROR;
}


status_t
get_click_speed(bigtime_t *speed)
{
	*speed = 500000;

	return B_OK;
}


uint32 global_modifiers = 0;

uint32
modifiers()
{
	return global_modifiers;
}


void
set_modifiers(uint32 modifiers)
{
	global_modifiers = modifiers;
}


status_t
get_key_info(key_info *info)
{
	if (info == NULL)
		return B_BAD_VALUE;
	
	info->modifiers = global_modifiers;
	memset(info->key_states, 0, sizeof(info->key_states));	// TODO

	return B_OK;
}


void
get_key_map(key_map **map, char **key_buffer)
{
	_get_key_map(map, key_buffer, NULL);
}


void
_get_key_map(key_map **map, char **key_buffer, ssize_t *key_buffer_size)
{
	BMessage command(IS_GET_KEY_MAP);
	BMessage reply;
	ssize_t map_count, key_count;
	const void *map_array = 0, *key_array = 0;
	if (key_buffer_size == NULL)
		key_buffer_size = &key_count;

	_control_input_server_(&command, &reply);

	if (reply.FindData("keymap", B_ANY_TYPE, &map_array, &map_count) != B_OK) {
		*map = 0; *key_buffer = 0;
		return;
	}

	if (reply.FindData("key_buffer", B_ANY_TYPE, &key_array, key_buffer_size)
			!= B_OK) {
		*map = 0; *key_buffer = 0;
		return;
	}

	*map = (key_map *)malloc(map_count);
	memcpy(*map, map_array, map_count);
	*key_buffer = (char *)malloc(*key_buffer_size);
	memcpy(*key_buffer, key_array, *key_buffer_size);
}


status_t
get_modifier_key(uint32 modifier, uint32 *key)
{
	BMessage command(IS_GET_MODIFIER_KEY);
	BMessage reply;
	uint32 rkey;

	command.AddInt32("modifier", modifier);
	_control_input_server_(&command, &reply);

	status_t err = reply.FindInt32("key", (int32 *) &rkey);
	if (err != B_OK)
		return err;
	*key = rkey;

	return B_OK;
}

rgb_color
keyboard_navigation_color()
{
	// Queries the app_server
	return ui_color(B_KEYBOARD_NAVIGATION_COLOR);
}


int32
count_workspaces()
{
	return 1;
}


int32
current_workspace()
{
	int32 index = 0;

	return index;
}


void
activate_workspace(int32 workspace)
{
	// FIXME
}

void
run_be_about()
{
	const char* path = "/usr/local/bin/AboutSystem";

#ifdef _WIN32

    #include <windows.h>

    HINSTANCE result = ShellExecuteA(
        nullptr,
        "open",
        "AboutSystem.exe",
        nullptr,
        nullptr,
        SW_SHOWNORMAL
    );

#elif __APPLE__

    pid_t pid = fork();
    if (pid == 0)
    {
        execl("/usr/bin/open", "open", path, (char*)nullptr);
        _exit(1);
    }

#else // Linux

    pid_t pid = fork();
    if (pid == 0)
    {
        execl(path, path, (char*)nullptr);
        _exit(1);
    }

#endif
}


bool
focus_follows_mouse()
{
	return mouse_mode() == B_FOCUS_FOLLOWS_MOUSE;
}


mode_mouse
mouse_mode()
{
	// Gets the mouse focus style, such as activate to click,
	// focus to click, ...
	mode_mouse mode = B_NORMAL_MOUSE;

	return mode;
}


status_t
get_mouse(BPoint* screenWhere, uint32* buttons)
{
	if (screenWhere == NULL && buttons == NULL)
		return B_BAD_VALUE;

	// FIXME: implement

	return B_ERROR;
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


const char*
ui_color_name(color_which which)
{
	// Suppress warnings for B_NO_COLOR.
	if (which == B_NO_COLOR)
		return NULL;

	int32 index = color_which_to_index(which);
	if (index < 0 || index >= kColorWhichCount) {
		fprintf(stderr, "ui_color_name(): unknown color_which %d\n", which);
		return NULL;
	}

	return kColorNames[index];
}


color_which
which_ui_color(const char* name)
{
	if (name == NULL)
		return B_NO_COLOR;

	for (int32 index = 0; index < kColorWhichCount; ++index) {
		if (!strcmp(kColorNames[index], name))
			return index_to_color_which(index);
	}

	return B_NO_COLOR;
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


rgb_color shift_color(rgb_color color, float shift);

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

	// init global clipboard
	if (be_clipboard == NULL)
		be_clipboard = new BClipboard(NULL);

	// Attempt to load a control look add-on from one the add-ons/ControlLook paths.
	{
		BStringList addonPaths;
		AddControlLookDefaultPaths(addonPaths);
		for (int32 pathIndex = 0; pathIndex < addonPaths.CountStrings() && be_control_look == NULL; ++pathIndex) {
			BString path = addonPaths.StringAt(pathIndex);
			BDirectory dir(path.String());
			if (dir.InitCheck() != B_OK)
				continue;

			BEntry entry;
			while (dir.GetNextEntry(&entry) == B_OK && be_control_look == NULL) {
				if (!entry.IsFile())
					continue;

				BPath p(&entry);
				if (p.InitCheck() != B_OK)
					continue;

				BString leaf(entry.Name());
#if defined(__APPLE__)
			if (!leaf.EndsWith(".dylib"))
				continue;
#elif defined(_WIN32) || defined(WIN32)
			if (!leaf.EndsWith(".dll"))
				continue;
#else
			if (!leaf.EndsWith(".so"))
				continue;
#endif

				image_id addon = load_add_on(p.Path());
				if (addon == 0)
					continue;

				instantiate_control_look_func createFunc = NULL;
				if (get_image_symbol(addon, "instantiate_control_look",
						B_SYMBOL_TYPE_TEXT, (void**)&createFunc) == B_OK && createFunc != NULL) {
					be_control_look = createFunc(addon);
					sControlLookAddon = addon;
					printf("ControlLook add-on loaded from %s\n", p.Path());
					break;
				}

				unload_add_on(addon);
			}
		}
	}

	// Fallback to compiled-in control look if no add-on found
	if (be_control_look == NULL) {
		printf("No ControlLook add-on found.  Using built-in Haiku ControlLook\n");
		be_control_look = new HaikuControlLook();
	}

	_init_global_fonts_();

	BPrivate::gWidthBuffer = new BPrivate::WidthBuffer;
	status = BPrivate::MenuPrivate::CreateBitmaps();
	if (status != B_OK)
		return status;

	_menu_info_ptr_ = &BMenu::sMenuInfo;

	status = get_menu_info(&BMenu::sMenuInfo);
	if (status != B_OK)
		return status;

	general_info.background_color = ui_color(B_PANEL_BACKGROUND_COLOR);
	general_info.mark_color = ui_color(B_CONTROL_MARK_COLOR);
	general_info.highlight_color = ui_color(B_CONTROL_HIGHLIGHT_COLOR);
	general_info.window_frame_color = ui_color(B_WINDOW_TAB_COLOR);
	general_info.color_frame = true;

	// TODO: fill the other static members

	// Register cleanup function to be called at program exit
	// Use a wrapper function since atexit expects void(*)()
	struct CleanupHelper {
		static void cleanup_wrapper() { _fini_interface_kit_(); }
	};
	atexit(CleanupHelper::cleanup_wrapper);

	return status;
}


extern "C" status_t
_fini_interface_kit_()
{
	BPrivate::MenuPrivate::DeleteBitmaps();

	delete BPrivate::gWidthBuffer;
	BPrivate::gWidthBuffer = NULL;

	delete be_control_look;
	be_control_look = NULL;

	// Note: if we ever want to support live switching, we cannot just unload
	// the old one since some thread might still be in a method of the object.
	// maybe locking/unlocking all loopers around would ensure proper exit.
	if (sControlLookAddon != 0) {
		unload_add_on(sControlLookAddon);
		sControlLookAddon = 0;
	}

	// Shutdown Pango/Cairo font subsystem to prevent GTK hash table assertion
	// This must be done to properly clean up the default font map singleton
#ifdef HAVE_PANGO
	// Force cleanup of all Pango cached objects
	pango_cairo_font_map_set_default(NULL);
	// Clean up Cairo's static data including font caches
	//cairo_debug_reset_static_data();
#endif

	return B_OK;
}



namespace BPrivate {

status_t
get_application_order(int32 workspace, team_id** _applications,
	int32* _count)
{
	return B_UNSUPPORTED;
}


status_t
get_window_order(int32 workspace, int32** _tokens, int32* _count)
{
	return B_UNSUPPORTED;
}


}	// namespace BPrivate

// These methods were marked with "Danger, will Robinson!" in
// the OpenTracker source, so we might not want to be compatible
// here.
// In any way, we would need to update Deskbar to use our
// replacements, so we could as well just implement them...

void
do_window_action(int32 windowToken, int32 action, BRect zoomRect, bool zoom)
{
	(void)zoomRect;
	(void)zoom;

	if (be_app == NULL || be_app->Display() == NULL || windowToken == 0)
		return;

	switch (action) {
		case B_MINIMIZE_WINDOW:
			cosmoe_display_minimize_window(be_app->Display(), windowToken, true);
			break;

		case B_BRING_TO_FRONT:
			cosmoe_display_activate_window(be_app->Display(), windowToken);
			break;
	}
}


static status_t
normalize_window_match_key(const char* input, char* output,
	size_t outputSize)
{
	if (output == NULL || outputSize == 0)
		return B_BAD_VALUE;

	output[0] = '\0';
	if (input == NULL || input[0] == '\0')
		return B_BAD_VALUE;

	const char* normalized = input;
	if (strncasecmp(normalized, "application/", 12) == 0)
		normalized += 12;

	size_t out = 0;
	for (size_t i = 0; normalized[i] != '\0' && out + 1 < outputSize; i++) {
		unsigned char c = (unsigned char)normalized[i];
		if (isalnum(c))
			output[out++] = (char)tolower(c);
		else if (c == '.' || c == '-' || c == '_' || isspace(c))
			output[out++] = '-';
	}

	output[out] = '\0';
	return output[0] != '\0' ? B_OK : B_BAD_VALUE;
}


static bool
window_matches_team(team_id team, const app_info* appInfo,
	const cosmoe_backend_window_info& backendInfo)
{
	if (team < 0)
		return backendInfo.team_id == team;

	if (backendInfo.team_id == team)
		return true;

	if (appInfo == NULL || appInfo->signature[0] == '\0'
		|| backendInfo.identifier[0] == '\0') {
		return false;
	}

	char appKey[B_MIME_TYPE_LENGTH];
	char windowKey[sizeof(backendInfo.identifier)];
	if (normalize_window_match_key(appInfo->signature, appKey,
			sizeof(appKey)) != B_OK) {
		return false;
	}
	if (normalize_window_match_key(backendInfo.identifier, windowKey,
			sizeof(windowKey)) != B_OK) {
		return false;
	}

	return strcmp(appKey, windowKey) == 0;
}


client_window_info*
get_window_info(int32 serverToken)
{
	if (be_app == NULL || be_app->Display() == NULL || serverToken == 0)
		return NULL;

	cosmoe_backend_window_info backendInfo;
	if (cosmoe_display_get_window_info(be_app->Display(), serverToken,
			&backendInfo) != B_OK) {
		return NULL;
	}

	const char* name = backendInfo.name[0] != '\0'
		? backendInfo.name : backendInfo.identifier;
	if (name == NULL || name[0] == '\0')
		name = "Window";

	size_t nameLength = strlen(name) + 1;
	client_window_info* info = (client_window_info*)malloc(
		sizeof(client_window_info) + nameLength);
	if (info == NULL)
		return NULL;

	memset(info, 0, sizeof(client_window_info) + nameLength);
	info->team = backendInfo.team_id;
	info->server_token = backendInfo.window_id;
	info->thread = -1;
	info->client_token = backendInfo.window_id;
	info->client_port = -1;
	info->workspaces = backendInfo.workspaces != 0
		? backendInfo.workspaces : 0xffffffffu;
	info->layer = 3;
	info->feel = backendInfo.feel;
	info->flags = 0;
	info->window_left = 0;
	info->window_top = 0;
	info->window_right = 0;
	info->window_bottom = 0;
	info->show_hide_level = backendInfo.show_hide_level;
	info->is_mini = backendInfo.is_mini != 0;
	info->tab_height = 0.0f;
	info->border_size = 0.0f;
	memcpy(info->name, name, nameLength);
	return info;
}


int32*
get_token_list(team_id team, int32* _count)
{
	if (_count != NULL)
		*_count = 0;

	if (be_app == NULL || be_app->Display() == NULL)
		return NULL;

	const int32 kMaxWindows = 1024;
	int32_t allWindowIDs[kMaxWindows];
	int32_t totalCount = cosmoe_display_get_window_list(be_app->Display(),
		allWindowIDs, kMaxWindows);
	if (totalCount <= 0)
		return NULL;

	app_info appInfo;
	appInfo = app_info();
	if (team >= 0)
		be_roster->GetRunningAppInfo(team, &appInfo);

	int32_t* tokens = (int32_t*)malloc(totalCount * sizeof(int32_t));
	if (tokens == NULL)
		return NULL;

	int32_t count = 0;
	for (int32_t i = 0; i < totalCount; i++) {
		cosmoe_backend_window_info backendInfo;
		if (cosmoe_display_get_window_info(be_app->Display(), allWindowIDs[i],
				&backendInfo) != B_OK) {
			continue;
		}

		if (team == -1 || window_matches_team(team, &appInfo, backendInfo))
			tokens[count++] = allWindowIDs[i];
	}

	if (count == 0) {
		free(tokens);
		return NULL;
	}

	if (_count != NULL)
		*_count = count;
	return tokens;
}


void
do_bring_to_front_team(BRect zoomRect, team_id team, bool zoom)
{
	(void)zoomRect;
	(void)zoom;

	if (be_app == NULL || be_app->Display() == NULL)
		return;

	int32 count = 0;
	int32* tokens = get_token_list(team, &count);
	if (tokens == NULL)
		return;

	for (int32 i = 0; i < count; i++)
		cosmoe_display_activate_window(be_app->Display(), tokens[i]);

	free(tokens);
}


void
do_minimize_team(BRect zoomRect, team_id team, bool zoom)
{
	(void)zoomRect;
	(void)zoom;

	if (be_app == NULL || be_app->Display() == NULL)
		return;

	int32 count = 0;
	int32* tokens = get_token_list(team, &count);
	if (tokens == NULL)
		return;

	for (int32 i = 0; i < count; i++)
		cosmoe_display_minimize_window(be_app->Display(), tokens[i], true);

	free(tokens);
}


void
do_close_team(BRect zoomRect, team_id team, bool zoom)
{
	(void)zoomRect;
	(void)zoom;

	if (be_app == NULL || be_app->Display() == NULL)
		return;

	int32 count = 0;
	int32* tokens = get_token_list(team, &count);
	if (tokens == NULL)
		return;

	for (int32 i = 0; i < count; i++)
		cosmoe_display_close_window(be_app->Display(), tokens[i]);

	free(tokens);
}


//	#pragma mark - truncate string


void
truncate_string(BString& string, uint32 mode, float width,
	const float* escapementArray, float fontSize, float ellipsisWidth,
	int32 charCount)
{
	// add a tiny amount to the width to make floating point inaccuracy
	// not drop chars that would actually fit exactly
	width += 1.f / 128;

	switch (mode) {
		case B_TRUNCATE_BEGINNING:
		{
			float totalWidth = 0;
			for (int32 i = charCount - 1; i >= 0; i--) {
				float charWidth = escapementArray[i] * fontSize;
				if (totalWidth + charWidth > width) {
					// we need to truncate
					while (totalWidth + ellipsisWidth > width) {
						// remove chars until there's enough space for the
						// ellipsis
						if (++i == charCount) {
							// we've reached the end of the string and still
							// no space, so return an empty string
							string.Truncate(0);
							return;
						}

						totalWidth -= escapementArray[i] * fontSize;
					}

					string.RemoveChars(0, i + 1);
					string.PrependChars(B_UTF8_ELLIPSIS, 1);
					return;
				}

				totalWidth += charWidth;
			}

			break;
		}

		case B_TRUNCATE_END:
		{
			float totalWidth = 0;
			for (int32 i = 0; i < charCount; i++) {
				float charWidth = escapementArray[i] * fontSize;
				if (totalWidth + charWidth > width) {
					// we need to truncate
					while (totalWidth + ellipsisWidth > width) {
						// remove chars until there's enough space for the
						// ellipsis
						if (i-- == 0) {
							// we've reached the start of the string and still
							// no space, so return an empty string
							string.Truncate(0);
							return;
						}

						totalWidth -= escapementArray[i] * fontSize;
					}

					string.RemoveChars(i, charCount - i);
					string.AppendChars(B_UTF8_ELLIPSIS, 1);
					return;
				}

				totalWidth += charWidth;
			}

			break;
		}

		case B_TRUNCATE_MIDDLE:
		case B_TRUNCATE_SMART:
		{
			float leftWidth = 0;
			float rightWidth = 0;
			int32 leftIndex = 0;
			int32 rightIndex = charCount - 1;
			bool left = true;

			for (int32 i = 0; i < charCount; i++) {
				float charWidth
					= escapementArray[left ? leftIndex : rightIndex] * fontSize;

				if (leftWidth + rightWidth + charWidth > width) {
					// we need to truncate
					while (leftWidth + rightWidth + ellipsisWidth > width) {
						// remove chars until there's enough space for the
						// ellipsis
						if (leftIndex == 0 && rightIndex == charCount - 1) {
							// we've reached both ends of the string and still
							// no space, so return an empty string
							string.Truncate(0);
							return;
						}

						if (leftIndex > 0 && (rightIndex == charCount - 1
								|| leftWidth > rightWidth)) {
							// remove char on the left
							leftWidth -= escapementArray[--leftIndex]
								* fontSize;
						} else {
							// remove char on the right
							rightWidth -= escapementArray[++rightIndex]
								* fontSize;
						}
					}

					string.RemoveChars(leftIndex, rightIndex + 1 - leftIndex);
					string.InsertChars(B_UTF8_ELLIPSIS, 1, leftIndex);
					return;
				}

				if (left) {
					leftIndex++;
					leftWidth += charWidth;
				} else {
					rightIndex--;
					rightWidth += charWidth;
				}

				left = rightWidth > leftWidth;
			}

			break;
		}
	}

	// we've run through without the need to truncate, leave the string as it is
}
