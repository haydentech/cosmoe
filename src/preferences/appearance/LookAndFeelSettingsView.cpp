/*
 *  Copyright 2010-2020 Haiku, Inc. All rights reserved.
 *  Distributed under the terms of the MIT license.
 *
 *	Authors:
 *		Stephan Aßmus <superstippi@gmx.de>
 *		Alexander von Gluck <kallisti5@unixzen.com>
 *		John Scipione <jscipione@gmail.com>
 *		Ryan Leavengood <leavengood@gmail.com>
 */


#include "LookAndFeelSettingsView.h"

#include <stdio.h>
#include <stdlib.h>

#include <Alert.h>
#include <Alignment.h>
#include <AppFileInfo.h>
#include <Box.h>
#include <Button.h>
#include <Catalog.h>
#include <CheckBox.h>
#include <Directory.h>
#include <DesktopSettings.h>
#include <File.h>
#include <FindDirectory.h>
#include <InterfaceDefs.h>
#include <InterfacePrivate.h>
#include <LayoutBuilder.h>
#include <Locale.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <Path.h>
#include <PathFinder.h>
#include <PopUpMenu.h>
#include <ScrollBar.h>
#include <StringView.h>
#include <Size.h>
#include <Slider.h>
#include <SpaceLayoutItem.h>
#include <StringView.h>
#include <TextView.h>

#include "AppearanceWindow.h"
#include "FakeScrollBar.h"


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "DecorSettingsView"
	// This was not renamed to keep from breaking translations


static const int32 kMsgSetDecor = 'deco';
static const int32 kMsgDecorInfo = 'idec';

static const int32 kMsgSetControlLook = 'ctlk';
static const int32 kMsgControlLookInfo = 'iclk';

static const int32 kMsgDoubleScrollBarArrows = 'dsba';

static const int32 kMsgArrowStyleSingle = 'mass';
static const int32 kMsgArrowStyleDouble = 'masd';

static const bool kDefaultDoubleScrollBarArrowsSetting = false;


//	#pragma mark - LookAndFeelSettingsView


LookAndFeelSettingsView::LookAndFeelSettingsView(const char* name)
	:
	BView(name, 0),
	fControlLookInfoButton(NULL),
	fControlLookMenuField(NULL),
	fControlLookMenu(NULL),
	fArrowStyleSingle(NULL),
	fArrowStyleDouble(NULL),
	fSavedControlLook(NULL),
	fCurrentControlLook(NULL),
	fSavedDoubleArrowsValue(_DoubleScrollBarArrows())
{
	BPrivate::get_control_look(fCurrentControlLook);
	fSavedControlLook = fCurrentControlLook;

	// ControlLook menu
	_BuildControlLookMenu();
	fControlLookMenuField = new BMenuField("controllook",
		B_TRANSLATE("ControlLook:"), fControlLookMenu);
	fControlLookMenuField->SetToolTip(
		B_TRANSLATE("No effect on other running applications"));

	fControlLookInfoButton = new BButton(B_TRANSLATE("About"),
		new BMessage(kMsgControlLookInfo));

	// scroll bar arrow style
	BBox* arrowStyleBox = new BBox("arrow style");
	arrowStyleBox->SetLabel(B_TRANSLATE("Arrow style"));

	fArrowStyleSingle = new FakeScrollBar(true, false,
		new BMessage(kMsgArrowStyleSingle));
	fArrowStyleDouble = new FakeScrollBar(true, true,
		new BMessage(kMsgArrowStyleDouble));

	BView* arrowStyleView;
	arrowStyleView = BLayoutBuilder::Group<>()
		.AddGroup(B_VERTICAL, 1)
			.Add(new BStringView("single", B_TRANSLATE("Single:")))
			.Add(fArrowStyleSingle)
			.AddStrut(B_USE_DEFAULT_SPACING)
			.Add(new BStringView("double", B_TRANSLATE("Double:")))
			.Add(fArrowStyleDouble)
			.SetInsets(B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING,
				B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING)
			.End()
		.View();
	arrowStyleBox->AddChild(arrowStyleView);
	arrowStyleBox->SetExplicitAlignment(BAlignment(B_ALIGN_LEFT,
		B_ALIGN_VERTICAL_CENTER));
	arrowStyleBox->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));

	BStringView* scrollBarLabel
		= new BStringView("scroll bar", B_TRANSLATE("Scroll bar:"));
	scrollBarLabel->SetExplicitAlignment(
		BAlignment(B_ALIGN_LEFT, B_ALIGN_TOP));

	// control layout
	BLayoutBuilder::Grid<>(this, B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING)
		.Add(fControlLookMenuField->CreateLabelLayoutItem(), 0, 0)
		.Add(fControlLookMenuField->CreateMenuBarLayoutItem(), 1, 0)
		.Add(fControlLookInfoButton, 2, 0)
		.Add(scrollBarLabel, 0, 1)
		.Add(arrowStyleBox, 1, 1)
		.AddGlue(0, 2)
		.SetInsets(B_USE_WINDOW_SPACING);

	// TODO : Decorator Preview Image?
}


LookAndFeelSettingsView::~LookAndFeelSettingsView()
{
}


void
LookAndFeelSettingsView::AttachedToWindow()
{
	AdoptParentColors();

	if (Parent() == NULL)
		SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

	fControlLookMenu->SetTargetForItems(this);
	fControlLookInfoButton->SetTarget(this);
	fArrowStyleSingle->SetTarget(this);
	fArrowStyleDouble->SetTarget(this);

	if (fSavedDoubleArrowsValue)
		fArrowStyleDouble->SetValue(B_CONTROL_ON);
	else
		fArrowStyleSingle->SetValue(B_CONTROL_ON);
}


void
LookAndFeelSettingsView::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgSetControlLook:
		{
			BString controlLook;
			if (message->FindString("control_look", &controlLook) == B_OK)
				_SetControlLook(controlLook);
			break;
		}

		case kMsgControlLookInfo:
		{
			BString infoText(B_TRANSLATE("Default Haiku ControlLook"));
			BString path;
			if (!BPrivate::get_control_look(path))
				break;

			if (path.Length() > 0) {
				BEntry entry(path.String());
				infoText = _ControlLookLabel(entry.Name());
				infoText << "\n" << path;

				BFile file(path.String(), B_READ_ONLY);
				if (file.InitCheck() == B_OK) {
					BAppFileInfo info(&file);
					struct version_info version;
					if (info.InitCheck() == B_OK
						&& info.GetVersionInfo(&version, B_APP_VERSION_KIND) == B_OK) {
						infoText = version.short_info;
						infoText << "\n" << version.long_info;
					}
				}
			}

			BAlert* infoAlert = new BAlert(B_TRANSLATE("About control look"),
				infoText.String(), B_TRANSLATE("OK"));
			infoAlert->SetFlags(infoAlert->Flags() | B_CLOSE_ON_ESCAPE);
			infoAlert->Go();

			break;
		}

		case kMsgArrowStyleSingle:
			_SetDoubleScrollBarArrows(false);
			break;

		case kMsgArrowStyleDouble:
			_SetDoubleScrollBarArrows(true);
			break;

		default:
			BView::MessageReceived(message);
			break;
	}
}


//	#pragma mark - LookAndFeelSettingsView private methods


void
LookAndFeelSettingsView::_SetControlLook(const BString& path)
{
	fCurrentControlLook = path;
	BPrivate::set_control_look(path);

	if (path.Length() > 0) {
		BEntry entry(path.String());
		BString label = _ControlLookLabel(entry.Name());
		fControlLookMenu->FindItem(label.String())->SetMarked(true);
	} else
		fControlLookMenu->FindItem(B_TRANSLATE("Default"))->SetMarked(true);

	Window()->PostMessage(kMsgUpdate);
}


void
LookAndFeelSettingsView::_BuildControlLookMenu()
{
	BPathFinder pathFinder;
	BStringList paths;
	BDirectory dir;

	fControlLookMenu = new BPopUpMenu(B_TRANSLATE("Choose ControlLook"));

	BMessage* message = new BMessage(kMsgSetControlLook);
	message->AddString("control_look", "");

	BMenuItem* item = new BMenuItem(B_TRANSLATE("Default"), message);
	if (fCurrentControlLook.Length() == 0)
		item->SetMarked(true);
	fControlLookMenu->AddItem(item);

	status_t error = pathFinder.FindPaths(B_FIND_PATH_ADD_ONS_DIRECTORY,
		"control_look", paths);

	int32 count = paths.CountStrings();
	for (int32 i = 0; i < count; ++i) {
		if (error != B_OK || dir.SetTo(paths.StringAt(i)) != B_OK)
			continue;

		BEntry entry;
		while (dir.GetNextEntry(&entry) == B_OK) {
			BPath path(paths.StringAt(i), entry.Name());
			message = new BMessage(kMsgSetControlLook);
			message->AddString("control_look", path.Path());

			BString label = _ControlLookLabel(entry.Name());
			item = new BMenuItem(label.String(), message);
			fControlLookMenu->AddItem(item);
			if (BString(path.Path()) == fCurrentControlLook)
				item->SetMarked(true);
		}
	}
}


BString
LookAndFeelSettingsView::_ControlLookLabel(const char* name)
{
	BString label(name);

#if defined(__HAIKU__) || defined(__linux__)
	label.RemoveLast(".so");
#elif defined(__APPLE__)
	label.RemoveLast(".dylib");
#elif defined(_WIN32)
	label.RemoveLast(".dll");
#endif
	label.RemoveLast("ControlLook");
	label.Trim();
	return label;
}


bool
LookAndFeelSettingsView::_DoubleScrollBarArrows()
{
	scroll_bar_info info;
	get_scroll_bar_info(&info);

	return info.double_arrows;
}


void
LookAndFeelSettingsView::_SetDoubleScrollBarArrows(bool doubleArrows)
{
	scroll_bar_info info;
	get_scroll_bar_info(&info);

	if (info.double_arrows == doubleArrows)
		return;

	info.double_arrows = doubleArrows;
	set_scroll_bar_info(&info);

	if (doubleArrows)
		fArrowStyleDouble->SetValue(B_CONTROL_ON);
	else
		fArrowStyleSingle->SetValue(B_CONTROL_ON);

	Window()->PostMessage(kMsgUpdate);
}


bool
LookAndFeelSettingsView::IsDefaultable()
{
	return fCurrentControlLook.Length() != 0
		|| _DoubleScrollBarArrows() != false;
}


void
LookAndFeelSettingsView::SetDefaults()
{
	_SetControlLook(BString(""));
	_SetDoubleScrollBarArrows(false);
}


bool
LookAndFeelSettingsView::IsRevertable()
{
	return fCurrentControlLook != fSavedControlLook
		|| _DoubleScrollBarArrows() != fSavedDoubleArrowsValue;
}


void
LookAndFeelSettingsView::Revert()
{
	if (IsRevertable()) {
		_SetControlLook(fSavedControlLook);
		_SetDoubleScrollBarArrows(fSavedDoubleArrowsValue);
	}
}
