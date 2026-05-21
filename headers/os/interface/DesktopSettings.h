/*
 * Copyright 2001-2015, Haiku.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Axel Dorfler, axeld@pinc-software.de
 *		Andrej Spielmann, <andrej.spielmann@seh.ox.ac.uk>
 *		Joseph Groover <looncraz@looncraz.net>
 */
#ifndef DESKTOP_SETTINGS_H
#define DESKTOP_SETTINGS_H


#include <Font.h>
#include <InterfaceDefs.h>
#include <Menu.h>
#include <Message.h>


class DesktopSettingsPrivate;


static const int32 kMaxWorkspaces = 32;

enum {
	kAllSettings		= 0xff,
	kWorkspacesSettings	= 0x01,
	kFontSettings		= 0x02,
	kAppearanceSettings	= 0x04,
	kMouseSettings		= 0x08,
	kDraggerSettings	= 0x10,
};


class DesktopSettings {
public:
							DesktopSettings();
	virtual					~DesktopSettings();

			status_t			Save(uint32 mask = kAllSettings);

			void				GetDefaultPlainFont(BFont& font) const;
			void				GetDefaultBoldFont(BFont& font) const;
			void				GetDefaultFixedFont(BFont& font) const;

			void				GetScrollBarInfo(scroll_bar_info& info) const;
			void				GetMenuInfo(menu_info& info) const;

			mode_mouse			MouseMode() const;
			mode_focus_follows_mouse FocusFollowsMouseMode() const;

			bool				NormalMouse() const
									{ return MouseMode() == B_NORMAL_MOUSE; }
			bool				FocusFollowsMouse() const
									{ return MouseMode() == B_FOCUS_FOLLOWS_MOUSE; }
			bool				ClickToFocusMouse() const
									{ return MouseMode() == B_CLICK_TO_FOCUS_MOUSE; }

			bool				AcceptFirstClick() const;

			bool				ShowAllDraggers() const;

			int32				WorkspacesCount() const;
			int32				WorkspacesColumns() const;
			int32				WorkspacesRows() const;
			const BMessage*		WorkspacesMessage(int32 index) const;

			rgb_color			UIColor(color_which which) const;
			const BString&		ControlLook() const;

protected:
			DesktopSettingsPrivate*	fSettings;

private:
							DesktopSettings(const DesktopSettings&) = delete;
	DesktopSettings&		operator=(const DesktopSettings&) = delete;
};


class LockedDesktopSettings : public DesktopSettings {
public:
							LockedDesktopSettings();
							~LockedDesktopSettings();

			void				SetDefaultPlainFont(const BFont& font);
			void				SetDefaultBoldFont(const BFont& font);
			void				SetDefaultFixedFont(const BFont& font);

			void				SetScrollBarInfo(const scroll_bar_info& info);
			void				SetMenuInfo(const menu_info& info);

			void				SetMouseMode(mode_mouse mode);
			void				SetFocusFollowsMouseMode(
									mode_focus_follows_mouse mode);
			void				SetAcceptFirstClick(bool acceptFirstClick);

			void				SetShowAllDraggers(bool show);
			void				SetWorkspacesLayout(int32 columns, int32 rows);
			void				SetWorkspacesMessage(int32 index, BMessage& message);

			void				SetUIColor(color_which which, rgb_color color,
									bool* changed = NULL);
			void				SetUIColors(const BMessage& colors,
									bool* changed = NULL);

			status_t			SetControlLook(const char* path);

private:
							LockedDesktopSettings(
								const LockedDesktopSettings&) = delete;
	LockedDesktopSettings&	operator=(const LockedDesktopSettings&) = delete;
};


#endif	/* DESKTOP_SETTINGS_H */
