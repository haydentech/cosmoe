/*
 * Copyright 2005-2008, Jérôme Duval, jerome.duval@free.fr.
 * Distributed under the terms of the MIT License.
 */
#ifndef _WINDOW_PRIVATE_H
#define _WINDOW_PRIVATE_H


#include <CosmoeBackendAPI.h>
#include <Window.h>


/* Private window looks */

const window_look kDesktopWindowLook = window_look(4);
const window_look kLeftTitledWindowLook = window_look(25);

/* Private window feels */

const window_feel kDesktopWindowFeel = window_feel(1024);
const window_feel kMenuWindowFeel = window_feel(1025);
const window_feel kWindowScreenFeel = window_feel(1026);
const window_feel kPasswordWindowFeel = window_feel(1027);
const window_feel kOffscreenWindowFeel = window_feel(1028);

/* Private window types */

const window_type kWindowScreenWindow = window_type(1026);

/* Private window flags */

const uint32 kWindowScreenFlag = 0x10000;
const uint32 kAcceptKeyboardFocusFlag = 0x40000;
	// Accept keyboard input even if B_AVOID_FOCUS is set

typedef enum cosmoe_private_window_panel_placement private_window_panel_placement;

const uint32 kWindowPanelFlag = COSMOE_PRIVATE_WINDOW_PANEL_FLAG;
const uint32 kWindowPanelPlacementShift = COSMOE_PRIVATE_WINDOW_PANEL_PLACEMENT_SHIFT;
const uint32 kWindowPanelPlacementMask = COSMOE_PRIVATE_WINDOW_PANEL_PLACEMENT_MASK;

const private_window_panel_placement kWindowPanelTop = COSMOE_PANEL_PLACEMENT_TOP;
const private_window_panel_placement kWindowPanelBottom = COSMOE_PANEL_PLACEMENT_BOTTOM;
const private_window_panel_placement kWindowPanelLeft = COSMOE_PANEL_PLACEMENT_LEFT;
const private_window_panel_placement kWindowPanelRight = COSMOE_PANEL_PLACEMENT_RIGHT;
const private_window_panel_placement kWindowPanelLeftTop = COSMOE_PANEL_PLACEMENT_LEFT_TOP;
const private_window_panel_placement kWindowPanelRightTop = COSMOE_PANEL_PLACEMENT_RIGHT_TOP;
const private_window_panel_placement kWindowPanelLeftBottom = COSMOE_PANEL_PLACEMENT_LEFT_BOTTOM;
const private_window_panel_placement kWindowPanelRightBottom = COSMOE_PANEL_PLACEMENT_RIGHT_BOTTOM;

inline uint32
WindowPanelFlags(private_window_panel_placement placement)
{
	return kWindowPanelFlag
		| ((uint32)placement << kWindowPanelPlacementShift);
}

inline bool
WindowIsPanel(uint32 flags)
{
	return (flags & kWindowPanelFlag) != 0;
}

inline private_window_panel_placement
WindowPanelPlacement(uint32 flags)
{
	return (private_window_panel_placement)((flags & kWindowPanelPlacementMask)
		>> kWindowPanelPlacementShift);
}

inline uint32
SetWindowPanelFlags(uint32 flags, private_window_panel_placement placement)
{
	flags &= ~(kWindowPanelFlag | kWindowPanelPlacementMask);
	return flags | WindowPanelFlags(placement);
}

#endif // _WINDOW_PRIVATE_H
