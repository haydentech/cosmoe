/*
 * Copyright 2005-2015, Haiku.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Stephan Aßmus <superstippi@gmx.de>
 *		Axel Dörfler, axeld@pinc-software.de
 *		Andrej Spielmann, <andrej.spielmann@seh.ox.ac.uk>
 *		Joseph Groover <looncraz@looncraz.net>
 */


#include "DesktopSettings.h"
#include "DesktopSettingsPrivate.h"

#include <Directory.h>
#include <File.h>
#include <Font.h>
#include <FindDirectory.h>
#include <Locker.h>
#include <Path.h>
#include <stdio.h>

#include <DefaultColors.h>
#include <InterfaceDefs.h>



static BLocker sDesktopSettingsLock("desktop settings");


static int32
_ColorToInt32(const rgb_color& color)
{
	int32 value = 0;
	uint8* bytes = reinterpret_cast<uint8*>(&value);
	bytes[0] = color.red;
	bytes[1] = color.green;
	bytes[2] = color.blue;
	bytes[3] = color.alpha;
	return value;
}


static rgb_color
_ColorFromInt32(int32 value)
{
	const uint8* bytes = reinterpret_cast<const uint8*>(&value);
	rgb_color color = {bytes[0], bytes[1], bytes[2], bytes[3]};
	return color;
}


static void
_GetFontFamilyAndStyle(const BFont& font, font_family* family,
	font_style* style)
{
	font.GetFamilyAndStyle(family, style);
}




DesktopSettingsPrivate::DesktopSettingsPrivate()
	:
	fFontSettingsLoadStatus(B_OK)
{
	// if the on-disk settings are not complete, the defaults will be kept
	_SetDefaults();
	_Load();
}


DesktopSettingsPrivate::~DesktopSettingsPrivate()
{
}


void
DesktopSettingsPrivate::_SetDefaults()
{
	fPlainFont = *be_plain_font;
	fBoldFont = *be_bold_font;
	fFixedFont = *be_fixed_font;

	fMouseMode = B_NORMAL_MOUSE;
	fFocusFollowsMouseMode = B_NORMAL_FOCUS_FOLLOWS_MOUSE;
	fAcceptFirstClick = true;
	fShowAllDraggers = true;

	// init scrollbar info
	fScrollBarInfo.proportional = true;
	fScrollBarInfo.double_arrows = false;
	fScrollBarInfo.knob = 0;
		// look of the knob (R5: (0, 1, 2), 1 = default)
		// change default = 0 (no knob) in Haiku
	fScrollBarInfo.min_knob_size = 15;

	// init menu info
	font_family family;
	font_style style;
	_GetFontFamilyAndStyle(fPlainFont, &family, &style);
	strlcpy(fMenuInfo.f_family, family, B_FONT_FAMILY_LENGTH);
	strlcpy(fMenuInfo.f_style, style, B_FONT_STYLE_LENGTH);
	fMenuInfo.font_size = fPlainFont.Size();
	fMenuInfo.background_color.set_to(216, 216, 216);

	fMenuInfo.separator = 0;
		// look of the separator (R5: (0, 1, 2), default 0)
	fMenuInfo.click_to_open = true; // always true
	fMenuInfo.triggers_always_shown = false;

	fWorkspacesColumns = 2;
	fWorkspacesRows = 2;

	memcpy((void*)&fShared.colormap, system_colors(),
		sizeof(color_map));
	memcpy((void*)fShared.colors, BPrivate::kDefaultColors,
		sizeof(rgb_color) * kColorWhichCount);
}


status_t
DesktopSettingsPrivate::_GetPath(BPath& path)
{
	status_t status = find_directory(B_USER_SETTINGS_DIRECTORY, &path);
	if (status < B_OK)
		return status;

	status = path.Append("system/app_server");
	if (status < B_OK)
		return status;

	return create_directory(path.Path(), 0755);
}


status_t
DesktopSettingsPrivate::_Load()
{
	// TODO: add support for old app_server_settings file as well

	BPath basePath;
	status_t status = _GetPath(basePath);
	if (status < B_OK)
		return status;

	// read workspaces settings

	BPath path(basePath);
	path.Append("workspaces");

	BFile file;
	status = file.SetTo(path.Path(), B_READ_ONLY);
	if (status == B_OK) {
		BMessage settings;
		status = settings.Unflatten(&file);
		if (status == B_OK) {
			int32 columns;
			int32 rows;
			if (settings.FindInt32("columns", &columns) == B_OK
				&& settings.FindInt32("rows", &rows) == B_OK) {
				_ValidateWorkspacesLayout(columns, rows);
				fWorkspacesColumns = columns;
				fWorkspacesRows = rows;
			}

			int32 i = 0;
			while (i < kMaxWorkspaces && settings.FindMessage("workspace",
					i, &fWorkspaceMessages[i]) == B_OK) {
				i++;
			}
		}
	}

	// read font settings

	path = basePath;
	path.Append("fonts");

	status = file.SetTo(path.Path(), B_READ_ONLY);
	if (status == B_OK) {
		BMessage settings;
		status = settings.Unflatten(&file);
		if (status != B_OK) {
			fFontSettingsLoadStatus = status;
		} else {
			const char* family;
			const char* style;
			float size;

			if (settings.FindString("plain family", &family) == B_OK
				&& settings.FindString("plain style", &style) == B_OK
				&& settings.FindFloat("plain size", &size) == B_OK) {
				fPlainFont.SetFamilyAndStyle(family, style);
				fPlainFont.SetSize(size);
			}

			if (settings.FindString("bold family", &family) == B_OK
				&& settings.FindString("bold style", &style) == B_OK
				&& settings.FindFloat("bold size", &size) == B_OK) {
				fBoldFont.SetFamilyAndStyle(family, style);
				fBoldFont.SetSize(size);
			}

			if (settings.FindString("fixed family", &family) == B_OK
				&& settings.FindString("fixed style", &style) == B_OK
				&& settings.FindFloat("fixed size", &size) == B_OK) {
				BFont font = fFixedFont;
				if (font.SetFamilyAndStyle(family, style) == B_OK
					&& (font.IsFixed() || font.IsFullAndHalfFixed())) {
					fFixedFont = font;
				}
				fFixedFont.SetSize(size);
			}
		}
	} else
		fFontSettingsLoadStatus = status;

	// read mouse settings

	path = basePath;
	path.Append("mouse");

	status = file.SetTo(path.Path(), B_READ_ONLY);
	if (status == B_OK) {
		BMessage settings;
		status = settings.Unflatten(&file);
		if (status == B_OK) {
			int32 mode;
			if (settings.FindInt32("mode", &mode) == B_OK)
				fMouseMode = (mode_mouse)mode;

			int32 focusFollowsMouseMode;
			if (settings.FindInt32("focus follows mouse mode",
					&focusFollowsMouseMode) == B_OK) {
				fFocusFollowsMouseMode
					= (mode_focus_follows_mouse)focusFollowsMouseMode;
			}

			bool acceptFirstClick;
			if (settings.FindBool("accept first click", &acceptFirstClick)
					== B_OK) {
				fAcceptFirstClick = acceptFirstClick;
			}
		}
	}

	// read appearance settings

	path = basePath;
	path.Append("appearance");

	status = file.SetTo(path.Path(), B_READ_ONLY);
	if (status == B_OK) {
		BMessage settings;
		status = settings.Unflatten(&file);
		if (status == B_OK) {
			// menus
			float fontSize;
			if (settings.FindFloat("font size", &fontSize) == B_OK)
				fMenuInfo.font_size = fontSize;

			const char* fontFamily;
			if (settings.FindString("font family", &fontFamily) == B_OK)
				strlcpy(fMenuInfo.f_family, fontFamily, B_FONT_FAMILY_LENGTH);

			const char* fontStyle;
			if (settings.FindString("font style", &fontStyle) == B_OK)
				strlcpy(fMenuInfo.f_style, fontStyle, B_FONT_STYLE_LENGTH);

			int32 bgColor;
			if (settings.FindInt32("bg color", &bgColor) == B_OK)
				fMenuInfo.background_color = _ColorFromInt32(bgColor);

			int32 separator;
			if (settings.FindInt32("separator", &separator) == B_OK)
				fMenuInfo.separator = separator;

			bool clickToOpen;
			if (settings.FindBool("click to open", &clickToOpen) == B_OK)
				fMenuInfo.click_to_open = clickToOpen;

			bool triggersAlwaysShown;
			if (settings.FindBool("triggers always shown", &triggersAlwaysShown)
					 == B_OK) {
				fMenuInfo.triggers_always_shown = triggersAlwaysShown;
			}

			// scrollbars
			bool proportional;
			if (settings.FindBool("proportional", &proportional) == B_OK)
				fScrollBarInfo.proportional = proportional;

			bool doubleArrows;
			if (settings.FindBool("double arrows", &doubleArrows) == B_OK)
				fScrollBarInfo.double_arrows = doubleArrows;

			int32 knob;
			if (settings.FindInt32("knob", &knob) == B_OK)
				fScrollBarInfo.knob = knob;

			int32 minKnobSize;
			if (settings.FindInt32("min knob size", &minKnobSize) == B_OK)
				fScrollBarInfo.min_knob_size = minKnobSize;

			const char* controlLook;
			if (settings.FindString("control look", &controlLook) == B_OK) {
				fControlLook = controlLook;
			}

			// colors
			for (int32 i = 0; i < kColorWhichCount; i++) {
				char colorName[12];
				snprintf(colorName, sizeof(colorName), "color%" B_PRId32,
					(int32)index_to_color_which(i));

				int32 colorValue;
				if (settings.FindInt32(colorName, &colorValue) != B_OK) {
					// Set obviously bad value so the Appearance app can detect it
					fShared.colors[i] = B_TRANSPARENT_COLOR;
				} else {
					fShared.colors[i] = _ColorFromInt32(colorValue);
				}
			}
		}
	}

	// read dragger settings

	path = basePath;
	path.Append("dragger");

	status = file.SetTo(path.Path(), B_READ_ONLY);
	if (status == B_OK) {
		BMessage settings;
		status = settings.Unflatten(&file);
		if (status == B_OK) {
			if (settings.FindBool("show", &fShowAllDraggers) != B_OK)
				fShowAllDraggers = true;
		}
	}

	return B_OK;
}


status_t
DesktopSettingsPrivate::Save(uint32 mask)
{
#if TEST_MODE
	return B_OK;
#endif

	BPath basePath;
	status_t status = _GetPath(basePath);
	if (status != B_OK)
		return status;

	if (mask & kWorkspacesSettings) {
		BPath path(basePath);
		if (path.Append("workspaces") == B_OK) {
			BMessage settings('asws');
			settings.AddInt32("columns", fWorkspacesColumns);
			settings.AddInt32("rows", fWorkspacesRows);

			for (int32 i = 0; i < kMaxWorkspaces; i++) {
				settings.AddMessage("workspace", &fWorkspaceMessages[i]);
			}

			BFile file;
			status = file.SetTo(path.Path(), B_CREATE_FILE | B_ERASE_FILE
				| B_READ_WRITE);
			if (status == B_OK) {
				status = settings.Flatten(&file, NULL);
			}
		}
	}

	if (mask & kFontSettings) {
		BPath path(basePath);
		if (path.Append("fonts") == B_OK) {
			BMessage settings('asfn');
			font_family family;
			font_style style;

			_GetFontFamilyAndStyle(fPlainFont, &family, &style);
			settings.AddString("plain family", family);
			settings.AddString("plain style", style);
			settings.AddFloat("plain size", fPlainFont.Size());

			_GetFontFamilyAndStyle(fBoldFont, &family, &style);
			settings.AddString("bold family", family);
			settings.AddString("bold style", style);
			settings.AddFloat("bold size", fBoldFont.Size());

			_GetFontFamilyAndStyle(fFixedFont, &family, &style);
			settings.AddString("fixed family", family);
			settings.AddString("fixed style", style);
			settings.AddFloat("fixed size", fFixedFont.Size());

			BFile file;
			status = file.SetTo(path.Path(), B_CREATE_FILE | B_ERASE_FILE
				| B_READ_WRITE);
			if (status == B_OK) {
				status = settings.Flatten(&file, NULL);
			}
		}
	}

	if (mask & kMouseSettings) {
		BPath path(basePath);
		if (path.Append("mouse") == B_OK) {
			BMessage settings('asms');
			settings.AddInt32("mode", (int32)fMouseMode);
			settings.AddInt32("focus follows mouse mode",
				(int32)fFocusFollowsMouseMode);
			settings.AddBool("accept first click", fAcceptFirstClick);

			BFile file;
			status = file.SetTo(path.Path(), B_CREATE_FILE | B_ERASE_FILE
				| B_READ_WRITE);
			if (status == B_OK) {
				status = settings.Flatten(&file, NULL);
			}
		}
	}

	if (mask & kDraggerSettings) {
		BPath path(basePath);
		if (path.Append("dragger") == B_OK) {
			BMessage settings('asdg');
			settings.AddBool("show", fShowAllDraggers);

			BFile file;
			status = file.SetTo(path.Path(), B_CREATE_FILE | B_ERASE_FILE
				| B_READ_WRITE);
			if (status == B_OK) {
				status = settings.Flatten(&file, NULL);
			}
		}
	}

	if (mask & kAppearanceSettings) {
		BPath path(basePath);
		if (path.Append("appearance") == B_OK) {
			BMessage settings('aslk');
			settings.AddFloat("font size", fMenuInfo.font_size);
			settings.AddString("font family", fMenuInfo.f_family);
			settings.AddString("font style", fMenuInfo.f_style);
			settings.AddInt32("bg color", _ColorToInt32(fMenuInfo.background_color));
			settings.AddInt32("separator", fMenuInfo.separator);
			settings.AddBool("click to open", fMenuInfo.click_to_open);
			settings.AddBool("triggers always shown",
				fMenuInfo.triggers_always_shown);

			settings.AddBool("proportional", fScrollBarInfo.proportional);
			settings.AddBool("double arrows", fScrollBarInfo.double_arrows);
			settings.AddInt32("knob", fScrollBarInfo.knob);
			settings.AddInt32("min knob size", fScrollBarInfo.min_knob_size);

			settings.AddString("control look", fControlLook);

			for (int32 i = 0; i < kColorWhichCount; i++) {
				char colorName[12];
				snprintf(colorName, sizeof(colorName), "color%" B_PRId32,
					(int32)index_to_color_which(i));
				settings.AddInt32(colorName, _ColorToInt32(fShared.colors[i]));
			}

			BFile file;
			status = file.SetTo(path.Path(), B_CREATE_FILE | B_ERASE_FILE
				| B_READ_WRITE);
			if (status == B_OK) {
				status = settings.Flatten(&file, NULL);
			}
		}
	}

	return status;
}


void
DesktopSettingsPrivate::SetDefaultPlainFont(const BFont& font)
{
	fPlainFont = font;
	Save(kFontSettings);
}


const BFont&
DesktopSettingsPrivate::DefaultPlainFont() const
{
	return fPlainFont;
}


void
DesktopSettingsPrivate::SetDefaultBoldFont(const BFont& font)
{
	fBoldFont = font;
	Save(kFontSettings);
}


const BFont&
DesktopSettingsPrivate::DefaultBoldFont() const
{
	return fBoldFont;
}


void
DesktopSettingsPrivate::SetDefaultFixedFont(const BFont& font)
{
	fFixedFont = font;
	Save(kFontSettings);
}


const BFont&
DesktopSettingsPrivate::DefaultFixedFont() const
{
	return fFixedFont;
}


void
DesktopSettingsPrivate::SetScrollBarInfo(const scroll_bar_info& info)
{
	fScrollBarInfo = info;
	Save(kAppearanceSettings);
}


const scroll_bar_info&
DesktopSettingsPrivate::ScrollBarInfo() const
{
	return fScrollBarInfo;
}


void
DesktopSettingsPrivate::SetMenuInfo(const menu_info& info)
{
	fMenuInfo = info;
	// Also update the ui_color
	SetUIColor(B_MENU_BACKGROUND_COLOR, info.background_color);
		// SetUIColor already saves the settings
}


const menu_info&
DesktopSettingsPrivate::MenuInfo() const
{
	return fMenuInfo;
}


void
DesktopSettingsPrivate::SetMouseMode(const mode_mouse mode)
{
	fMouseMode = mode;
	Save(kMouseSettings);
}


void
DesktopSettingsPrivate::SetFocusFollowsMouseMode(mode_focus_follows_mouse mode)
{
	fFocusFollowsMouseMode = mode;
	Save(kMouseSettings);
}


mode_mouse
DesktopSettingsPrivate::MouseMode() const
{
	return fMouseMode;
}


mode_focus_follows_mouse
DesktopSettingsPrivate::FocusFollowsMouseMode() const
{
	return fFocusFollowsMouseMode;
}


void
DesktopSettingsPrivate::SetAcceptFirstClick(const bool acceptFirstClick)
{
	fAcceptFirstClick = acceptFirstClick;
	Save(kMouseSettings);
}


bool
DesktopSettingsPrivate::AcceptFirstClick() const
{
	return fAcceptFirstClick;
}


void
DesktopSettingsPrivate::SetShowAllDraggers(bool show)
{
	fShowAllDraggers = show;
	Save(kDraggerSettings);
}


bool
DesktopSettingsPrivate::ShowAllDraggers() const
{
	return fShowAllDraggers;
}


void
DesktopSettingsPrivate::SetWorkspacesLayout(int32 columns, int32 rows)
{
	_ValidateWorkspacesLayout(columns, rows);
	fWorkspacesColumns = columns;
	fWorkspacesRows = rows;

	Save(kWorkspacesSettings);
}


int32
DesktopSettingsPrivate::WorkspacesCount() const
{
	return fWorkspacesColumns * fWorkspacesRows;
}


int32
DesktopSettingsPrivate::WorkspacesColumns() const
{
	return fWorkspacesColumns;
}


int32
DesktopSettingsPrivate::WorkspacesRows() const
{
	return fWorkspacesRows;
}


void
DesktopSettingsPrivate::SetWorkspacesMessage(int32 index, BMessage& message)
{
	if (index < 0 || index >= kMaxWorkspaces)
		return;

	fWorkspaceMessages[index] = message;
}


const BMessage*
DesktopSettingsPrivate::WorkspacesMessage(int32 index) const
{
	if (index < 0 || index >= kMaxWorkspaces)
		return NULL;

	return &fWorkspaceMessages[index];
}


void
DesktopSettingsPrivate::SetUIColor(color_which which, const rgb_color color,
									bool* changed)
{
	int32 index = color_which_to_index(which);
	if (index < 0 || index >= kColorWhichCount)
		return;

	if (changed != NULL)
		*changed = fShared.colors[index] != color;

	fShared.colors[index] = color;
	// TODO: deprecate the background_color member of the menu_info struct,
	// otherwise we have to keep this duplication...
	if (which == B_MENU_BACKGROUND_COLOR)
		fMenuInfo.background_color = color;

	Save(kAppearanceSettings);
}


void
DesktopSettingsPrivate::SetUIColors(const BMessage& colors, bool* changed)
{
	int32 count = colors.CountNames(B_RGB_32_BIT_TYPE);
	if (count <= 0)
		return;

	int32 index = 0;
	int32 colorIndex = 0;
	char* name = NULL;
	type_code type;
	rgb_color color;
	color_which which = B_NO_COLOR;

	while (colors.GetInfo(B_RGB_32_BIT_TYPE, index, &name, &type) == B_OK) {
		which = which_ui_color(name);
		colorIndex = color_which_to_index(which);
		if (colorIndex < 0 || colorIndex >= kColorWhichCount
			|| colors.FindColor(name, &color) != B_OK) {
			if (changed != NULL)
				changed[index] = false;

			++index;
			continue;
		}

		if (changed != NULL)
			changed[index] = fShared.colors[colorIndex] != color;

		fShared.colors[colorIndex] = color;

		if (which == (int32)B_MENU_BACKGROUND_COLOR)
			fMenuInfo.background_color = color;

		++index;
	}

	Save(kAppearanceSettings);
}


rgb_color
DesktopSettingsPrivate::UIColor(color_which which) const
{
	static const rgb_color invalidColor = {0, 0, 0, 0};
	int32 index = color_which_to_index(which);
	if (index < 0 || index >= kColorWhichCount)
		return invalidColor;

	return fShared.colors[index];
}


status_t
DesktopSettingsPrivate::SetControlLook(const char* path)
{
	fControlLook = path;
	return Save(kAppearanceSettings);
}


const BString&
DesktopSettingsPrivate::ControlLook() const
{
	return fControlLook;
}


void
DesktopSettingsPrivate::_ValidateWorkspacesLayout(int32& columns,
	int32& rows) const
{
	if (columns < 1)
		columns = 1;
	if (rows < 1)
		rows = 1;

	if (columns * rows > kMaxWorkspaces) {
		// Revert to defaults in case of invalid settings
		columns = 2;
		rows = 2;
	}
}


//	#pragma mark - read access


DesktopSettings::DesktopSettings()
	:
	fSettings(new DesktopSettingsPrivate())

{
}


DesktopSettings::~DesktopSettings()
{
	delete fSettings;
}


status_t
DesktopSettings::Save(uint32 mask)
{
	return fSettings->Save(mask);
}


void
DesktopSettings::GetDefaultPlainFont(BFont& font) const
{
	font = fSettings->DefaultPlainFont();
}


void
DesktopSettings::GetDefaultBoldFont(BFont& font) const
{
	font = fSettings->DefaultBoldFont();
}


void
DesktopSettings::GetDefaultFixedFont(BFont& font) const
{
	font = fSettings->DefaultFixedFont();
}


void
DesktopSettings::GetScrollBarInfo(scroll_bar_info& info) const
{
	info = fSettings->ScrollBarInfo();
}


void
DesktopSettings::GetMenuInfo(menu_info& info) const
{
	info = fSettings->MenuInfo();
}


mode_mouse
DesktopSettings::MouseMode() const
{
	return fSettings->MouseMode();
}


mode_focus_follows_mouse
DesktopSettings::FocusFollowsMouseMode() const
{
	return fSettings->FocusFollowsMouseMode();
}


bool
DesktopSettings::AcceptFirstClick() const
{
	return fSettings->AcceptFirstClick();
}


bool
DesktopSettings::ShowAllDraggers() const
{
	return fSettings->ShowAllDraggers();
}


int32
DesktopSettings::WorkspacesCount() const
{
	return fSettings->WorkspacesCount();
}


int32
DesktopSettings::WorkspacesColumns() const
{
	return fSettings->WorkspacesColumns();
}


int32
DesktopSettings::WorkspacesRows() const
{
	return fSettings->WorkspacesRows();
}


const BMessage*
DesktopSettings::WorkspacesMessage(int32 index) const
{
	return fSettings->WorkspacesMessage(index);
}


rgb_color
DesktopSettings::UIColor(color_which which) const
{
	return fSettings->UIColor(which);
}


const BString&
DesktopSettings::ControlLook() const
{
	return fSettings->ControlLook();
}

//	#pragma mark - write access


LockedDesktopSettings::LockedDesktopSettings()
{
	sDesktopSettingsLock.Lock();
}


LockedDesktopSettings::~LockedDesktopSettings()
{
	sDesktopSettingsLock.Unlock();
}


void
LockedDesktopSettings::SetDefaultPlainFont(const BFont& font)
{
	fSettings->SetDefaultPlainFont(font);
}


void
LockedDesktopSettings::SetDefaultBoldFont(const BFont& font)
{
	fSettings->SetDefaultBoldFont(font);
}


void
LockedDesktopSettings::SetDefaultFixedFont(const BFont& font)
{
	fSettings->SetDefaultFixedFont(font);
}


void
LockedDesktopSettings::SetScrollBarInfo(const scroll_bar_info& info)
{
	fSettings->SetScrollBarInfo(info);
}


void
LockedDesktopSettings::SetMenuInfo(const menu_info& info)
{
	fSettings->SetMenuInfo(info);
}


void
LockedDesktopSettings::SetMouseMode(const mode_mouse mode)
{
	fSettings->SetMouseMode(mode);
}


void
LockedDesktopSettings::SetFocusFollowsMouseMode(mode_focus_follows_mouse mode)
{
	fSettings->SetFocusFollowsMouseMode(mode);
}


void
LockedDesktopSettings::SetAcceptFirstClick(const bool acceptFirstClick)
{
	fSettings->SetAcceptFirstClick(acceptFirstClick);
}


void
LockedDesktopSettings::SetShowAllDraggers(bool show)
{
	fSettings->SetShowAllDraggers(show);
}


void
LockedDesktopSettings::SetWorkspacesLayout(int32 columns, int32 rows)
{
	fSettings->SetWorkspacesLayout(columns, rows);
}


void
LockedDesktopSettings::SetWorkspacesMessage(int32 index, BMessage& message)
{
	fSettings->SetWorkspacesMessage(index, message);
}


void
LockedDesktopSettings::SetUIColor(color_which which, rgb_color color,
	bool* changed)
{
	fSettings->SetUIColor(which, color, changed);
}


void
LockedDesktopSettings::SetUIColors(const BMessage& colors, bool* changed)
{
	fSettings->SetUIColors(colors, changed);
}


status_t
LockedDesktopSettings::SetControlLook(const char* path)
{
	return fSettings->SetControlLook(path);
}

