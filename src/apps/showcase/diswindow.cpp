/*This is diswindow.cpp*/

#include <Application.h>
#include "diswindow.h"
#include "disview.h"
#include "IconTab.h"
#include "IconTabView.h"
#include "IconOutlineListView.h"
#include "VectorImageButton.h"

#include <iostream>
#include <stdio.h>
#include <string>
#include <string.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <String.h>

#if defined(__linux__) || defined(__APPLE__)
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

#include "Placeholder.h"

#include <Box.h>
#include <Button.h>
#include <MenuItem.h>
#include <CheckBox.h>
#include <ColorControl.h>
#include <RadioButton.h>
#include <GroupView.h>
#include <GroupLayout.h>
#include <SplitView.h>
#include <LayoutBuilder.h>
#include <StringView.h>
#include <TextControl.h>
#include <AppFileInfo.h>
#include <Mime.h>
#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <Message.h>
#include <MessageRunner.h>
#include <Path.h>
#include <Resources.h>
#include <NodeInfo.h>
#include <Slider.h>
#include <TabView.h>
#include <ScrollBar.h>
#include <Alert.h>
#include <InterfaceDefs.h>
#include <InterfacePrivate.h>
#include <Cursor.h>
#include <PopUpMenu.h>

#include <ChannelSlider.h>
#include <DecimalSpinner.h>
#include <ColumnListView.h>
#include <ColumnTypes.h>
#include <OutlineListView.h>
#include <StringItem.h>

#include <IconUtils.h>
#include <ControlLook.h>
#include <TranslationUtils.h>
#include <TranslatorFormats.h>
#include <BitmapButton.h>
#include <ScrollView.h>
#include <Gradient.h>
#include <GradientLinear.h>
#include <Screen.h>


#include <agg_blur.h>
#include <agg_pixfmt_rgba.h>
#include <agg_rendering_buffer.h>



const int CHECK_ONE = 'chk1';
const int CHECK_TWO = 'chk2';
const int RADIO_ONE = 'rad1';
const int RADIO_TWO = 'rad2';
const int SHOW_ALERT = 'SHWA';
const int SHOW_ALERT_ASYNC = 'SHAA';
const int SHOW_HIDE_VIEW = 'SHVi';
const int RESIZE_VIEW = 'RSVi';
const int SHOW_FILE_PANEL = 'SHFP';
const int SHOW_SAVE_PANEL = 'SHSP';
const int MOVE_WINDOW = 'MOVW';
const int CENTER_WINDOW = 'CENW';
const int MOVE_LEFT = 'MLFT';
const int MOVE_UP = 'MUP_';
const int MOVE_RIGHT = 'MRGT';
const int MOVE_DOWN = 'MDWN';
const int EXPAND_WINDOW = 'EXPW';
const int SHRINK_WINDOW = 'SHRW';
const int UPDATE_SYSINFO = 'UPSI';
const int TEXT_CHANGED = 'TXCH';
const int LAYOUT_TOGGLE_MIDDLE = 'LYTM';
const int LAYOUT_ROTATE_SPLIT = 'LYRS';
const int LAYOUT_SPACING_CHANGED = 'LYSP';

const uint8 kCursorPointingFinger[] = {
	0x6e, 0x63, 0x69, 0x66, 0x02, 0x05, 0x00, 0x02, 0x00, 0x16, 0x02, 0x00,
	0x00, 0x00, 0x3b, 0x80, 0x00, 0xbe, 0x20, 0x00, 0x00, 0x00, 0x00, 0x4a,
	0x20, 0x00, 0x48, 0x60, 0x00, 0x00, 0xff, 0xff, 0xfb, 0x02, 0x02, 0x11,
	0xb5, 0xd2, 0xb7, 0x91, 0xb5, 0x35, 0xb6, 0x8e, 0xb6, 0x6f, 0x2e, 0x2f,
	0x38, 0x2f, 0x38, 0x2f, 0x38, 0x25, 0x34, 0xb5, 0xe9, 0x34, 0xb4, 0x88,
	0x34, 0x22, 0xbb, 0x9a, 0x22, 0xbb, 0x25, 0x22, 0x37, 0xb5, 0x2d, 0xbc,
	0x68, 0x24, 0xbc, 0x1e, 0xb6, 0x18, 0xbc, 0xde, 0xb9, 0xac, 0xbf, 0x83,
	0xb8, 0x7a, 0xbd, 0x85, 0xb9, 0xac, 0xbf, 0x83, 0xbd, 0x04, 0xbf, 0x63,
	0xbb, 0xd2, 0xbf, 0xc9, 0xbe, 0xbb, 0xbe, 0xd1, 0x40, 0xbe, 0x5e, 0x40,
	0xbe, 0x5e, 0x40, 0xbe, 0x5e, 0x40, 0x31, 0x40, 0xba, 0xb1, 0x40, 0xb9,
	0x0b, 0x3c, 0x2c, 0xbe, 0x84, 0xb7, 0xbe, 0xbd, 0x85, 0xb7, 0xd8, 0xbe,
	0x5e, 0xb9, 0x30, 0xbe, 0xd4, 0xb9, 0x30, 0xbd, 0xe9, 0xb9, 0x30, 0xbc,
	0x10, 0x2a, 0xbc, 0xfb, 0x2a, 0xbb, 0x25, 0x2a, 0xbc, 0x12, 0xb8, 0xbb,
	0xbc, 0x88, 0xb8, 0xbb, 0xbb, 0x9c, 0xb8, 0xbb, 0x30, 0x2a, 0xba, 0x4b,
	0x2a, 0x2e, 0x2a, 0x31, 0x2f, 0xba, 0x3b, 0x2f, 0xb9, 0x86, 0x2f, 0xb7,
	0x7b, 0xb7, 0x39, 0xb8, 0x66, 0xb8, 0x8e, 0xb6, 0xd5, 0xb6, 0x49, 0xb4,
	0xed, 0x24, 0xb5, 0xf0, 0x24, 0xb3, 0xf2, 0x24, 0x00, 0x10, 0x40, 0x3c,
	0x40, 0x3c, 0x40, 0x3c, 0x40, 0x31, 0x40, 0xba, 0xb1, 0x40, 0xb9, 0x0b,
	0x3c, 0x2c, 0xbe, 0x84, 0xb7, 0xbe, 0xbd, 0x85, 0xb7, 0xd8, 0xbe, 0x5e,
	0xb9, 0x30, 0xbe, 0xd4, 0xb9, 0x30, 0xbd, 0xe9, 0xb9, 0x30, 0xbc, 0x10,
	0x2a, 0xbc, 0xfb, 0x2a, 0xbb, 0x25, 0x2a, 0xbc, 0x12, 0xb8, 0xbb, 0xbc,
	0x88, 0xb8, 0xbb, 0xbb, 0x9c, 0xb8, 0xbb, 0x30, 0x2a, 0xba, 0x4b, 0x2a,
	0x2e, 0x2a, 0x31, 0x2f, 0xba, 0x3b, 0x2f, 0xb9, 0x86, 0x2f, 0xb7, 0x7b,
	0xb7, 0x39, 0xb8, 0x66, 0xb8, 0x8e, 0xb6, 0xd5, 0xb6, 0x49, 0xb4, 0xed,
	0x24, 0xb5, 0xf0, 0x24, 0xb3, 0xf2, 0x24, 0xb5, 0xd2, 0xb7, 0x91, 0xb5,
	0x35, 0xb6, 0x8e, 0xb6, 0x6f, 0x2e, 0x2f, 0x38, 0x2f, 0x38, 0x2f, 0x38,
	0x25, 0x34, 0xb5, 0xe9, 0x34, 0xb4, 0x88, 0x34, 0x22, 0xbb, 0x9a, 0x22,
	0xbb, 0x25, 0x22, 0x37, 0xb5, 0x2d, 0xbc, 0x68, 0x24, 0xbc, 0x1e, 0xb6,
	0x18, 0xbc, 0xde, 0x30, 0x3f, 0x2d, 0x3a, 0x30, 0x3f, 0x02, 0x0a, 0x00,
	0x01, 0x01, 0x10, 0x01, 0x17, 0x84, 0x02, 0x04, 0x0a, 0x01, 0x01, 0x00,
	0x00
};

BBitmap* RenderVectorCursor(uint32 size, const uint8* vector,
	uint32 vectorSize, float shadowStrength);


#ifdef __HAIKU__

status_t
GetAppIcon(const char* iconName, icon_size which, BBitmap* icon)
{
	// Check the icon bitmap
	if (icon == NULL || icon->InitCheck() < B_OK) {
		return B_BAD_DATA;
	}

	// Load the raw icon data
	size_t size = 0;
	const uint8* rawIcon;

	// Try to load vector icon
	rawIcon = (const uint8*)be_app->AppResources()->LoadResource(B_VECTOR_ICON_TYPE, iconName, &size);
	if (rawIcon != NULL && BIconUtils::GetVectorIcon(rawIcon, size, icon) == B_OK) {
		return B_OK;
	}

	// Fall back to bitmap icon, then mini bitmap icon
	rawIcon = (const uint8*)be_app->AppResources()->LoadResource(B_LARGE_ICON_TYPE, iconName, &size);

	if (rawIcon == NULL) {
		rawIcon = (const uint8*)be_app->AppResources()->LoadResource(B_MINI_ICON_TYPE, iconName, &size);
	}

	if (rawIcon == NULL) {
		delete icon;
		return B_ENTRY_NOT_FOUND;
	}

	// Handle color space conversion
	if (icon->ColorSpace() != B_CMAP8) {
		BIconUtils::ConvertFromCMAP8(rawIcon, which, which, which, icon);
	}

	return B_OK;
}
#endif


class AppEntry {
public:
	AppEntry()
		: name(), path(), description(), icon(NULL)
	{
	}

	std::string name;
	std::string path;
	std::string description;
	BBitmap* icon;
};

class IconView : public BView {
	public:
							IconView(BRect rect, uint32 followFlags);
		virtual				~IconView();

		virtual void		Draw(BRect updateRect);
		virtual void		AttachedToWindow();
		virtual void		MouseMoved(BPoint where, uint32 code, const BMessage* dragMessage);
		virtual void		MouseDown(BPoint where);
		virtual void		MouseUp(BPoint where);

	private:
				std::vector<AppEntry> fApps;
				BPoint			fMousePos;
				bool			fMouseInView;
				bool			fMouseDown;
				int				fHoveredAppIndex;
				BCursor*		fViewCursor;
};

class SystemInfoView : public BView {
	public:
								SystemInfoView(BRect rect, uint32 followFlags);
		virtual					~SystemInfoView();
		
		virtual void			Draw(BRect updateRect);
		virtual void			MessageReceived(BMessage* message);
		virtual void			AttachedToWindow();
		virtual void			Pulse();
		virtual void			MouseMoved(BPoint where, uint32 code, const BMessage* dragMessage);
		
				void			UpdateInfo();
	
	private:
				void			_SyncThemeState();

				BStringView*	fWindowPosLabel;
				BStringView*	fMousePosLabel;
				BStringView*	fWindowSizeLabel;
				BStringView*	fScreenSizeLabel;
				BStringView*	fBackendLabel;
				BStringView*	fScaleLabel;
				BStringView*	fControlLookLabel;
				BPoint			fLastMousePos;
};

class BitmapView : public BView {
	public:
								BitmapView(BRect rect, const char* name, uint32 followFlags);
		virtual					~BitmapView();
	
		virtual void			Draw(BRect updateRect);
		virtual void			MouseDown(BPoint pos);
		virtual void			MouseMoved(BPoint where, uint32 code, const BMessage* dragMessage);
	
	private:
				BBitmap*		mBitmap;
				BPoint			fMousePos;
				bool			fMouseInView;
};


class SampleDataRow : public BRow
{
	public:
								SampleDataRow();
};



DisWindow::DisWindow(BRect aRect)
	: BWindow ( aRect, "Cosmoe Showcase", B_TITLED_WINDOW, /*B_NOT_V_RESIZABLE |*/ B_CLOSE_ON_ESCAPE),
	fFilePanel(new BFilePanel(B_OPEN_PANEL)),
	fSavePanel(new BFilePanel(B_SAVE_PANEL))
{
	fIcon = new(std::nothrow) BBitmap(BRect(BPoint(0, 0), be_control_look->ComposeIconSize(32)), 0, B_RGBA32);
#ifndef __HAIKU__
	BIconUtils::GetAppIcon("BEOS:ICON", B_LARGE_ICON, fIcon);
#else
	GetAppIcon("BEOS:ICON", B_LARGE_ICON, fIcon);
#endif
	if (fIcon == NULL) {
		fprintf(stderr, "Failed to load BEOS:ICON icon\n");
	}
}

bool DisWindow::QuitRequested()
{
	be_app->PostMessage(B_QUIT_REQUESTED);
	return (true);
}


void DisWindow::Populate()
{
	const float kInset = 8.0f;
	const float kTopInsetExtra = 4.0f;

	SetupMenus();

	BRect r;
	BTabView *tabView;
	BTab *tab;
	auto createTab = [](const char* label,
		const char* iconName = NULL) -> BTab* {
		IconTab* iconTab = new IconTab();
		iconTab->SetLabel(label);

		if (iconName != NULL && iconName[0] != '\0') {
			
			BBitmap tabIcon(BRect(0, 0, 31, 31), 0, B_RGBA32);
			status_t iconStatus = B_ERROR;

#if !defined(__HAIKU__)
			iconStatus = BIconUtils::GetAppIcon(iconName, B_LARGE_ICON, &tabIcon);
#else
			iconStatus = GetAppIcon(iconName, B_LARGE_ICON, &tabIcon);
#endif

			if (iconStatus == B_OK)
				iconTab->SetIcon(&tabIcon);
		}

		return iconTab;
	};

	r = Bounds();

	tabView = new BIconTabView(r, "tab_view", B_WIDTH_FROM_LABEL);
	// Uncomment to test BView affine rotation of tabs
	//tabView->SetTabSide(BTabView::kLeftSide);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0.0f)
		.Add(mMenuBar)
		.Add(tabView);
	
	// Size the tabs using the tabview container area
	r = tabView->ContainerView()->Bounds();
	// Launcher Tab
	tab = createTab("Launcher", "rocket_icon");
	BView* launcherTabView = new BView(r, "Tab (Launcher)", B_FOLLOW_ALL, B_WILL_DRAW);
	tabView->AddTab(launcherTabView, tab);

	// Controls Tab
	tab = createTab("Controls", "controls_icon");
	BView* controlsTabView = new BView(r, "Tab (Controls)", B_FOLLOW_ALL, B_WILL_DRAW);
	controlsTabView->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	tabView->AddTab(controlsTabView, tab);

	// GUI Elements Tab
	tab = createTab("GUI Elements", "gui_icon");
	BView* guiElementsTabView = new BView(r, "Tab (GUI Elements)", B_FOLLOW_ALL, B_WILL_DRAW);
	guiElementsTabView->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	tabView->AddTab(guiElementsTabView, tab);

	// Draw Testing Tab
	tab = createTab("Draw Testing", "drawtesting_icon");
	BView* testingTabView = new BView(r, "Tab (Testing)", B_FOLLOW_ALL, B_WILL_DRAW);
	testingTabView->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	tabView->AddTab(testingTabView, tab);

	// System Info Tab
	tab = createTab("System Info", "sysinfo_icon");
	SystemInfoView* systemInfoTabView = new SystemInfoView(r, B_FOLLOW_ALL);
	tabView->AddTab(systemInfoTabView, tab);

	// Layout Tab
	tab = createTab("Layout", "layout_icon");
	BView* layoutTabView = new BView(r, "Tab (Layout)", B_FOLLOW_ALL,
		B_WILL_DRAW | B_SUPPORTS_LAYOUT);
	layoutTabView->SetLayout(new BGroupLayout(B_VERTICAL,
		B_USE_DEFAULT_SPACING));
	layoutTabView->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	tabView->AddTab(layoutTabView, tab);

	BStringView* layoutIntro = new BStringView("layout_intro",
		"Live layout playground: BGroupView, BSplitView, BLayoutBuilder");
	layoutIntro->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));

	BGroupView* leftColumn = new BGroupView("layout_left", B_VERTICAL,
		B_USE_DEFAULT_SPACING);
	leftColumn->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	leftColumn->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNLIMITED));

	BGroupView* leftControls = new BGroupView("layout_left_controls", B_VERTICAL,
		B_USE_DEFAULT_SPACING);
	leftControls->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

	BStringView* groupLabel = new BStringView("layout_group_label",
		"BGroupView + BLayoutBuilder");
	groupLabel->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
	BLayoutBuilder::Group<>(leftControls, B_VERTICAL, B_USE_DEFAULT_SPACING)
		.Add(groupLabel)
		.Add(new BTextControl("layout_text", "Name:", "Cosmoe", NULL));

	BLayoutBuilder::Group<>(leftColumn, B_VERTICAL, B_USE_DEFAULT_SPACING)
		.Add(leftControls);

	BGroupView* rightColumn = new BGroupView("layout_right", B_VERTICAL,
		B_USE_DEFAULT_SPACING);
	rightColumn->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	rightColumn->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNLIMITED));

	BStringView* splitLabel = new BStringView("layout_split_label",
		"BSplitView with drag dividers");
	splitLabel->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));

	BSplitView* verticalSplit = new BSplitView(B_VERTICAL, B_USE_DEFAULT_SPACING);

	BGroupView* splitTop = new BGroupView("split_top", B_VERTICAL,
		B_USE_DEFAULT_SPACING);
	splitTop->SetViewColor(233, 244, 255);
	splitTop->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNLIMITED));
	BLayoutBuilder::Group<>(splitTop, B_VERTICAL, B_USE_DEFAULT_SPACING)
		.SetInsets(B_USE_SMALL_SPACING, B_USE_SMALL_SPACING,
			B_USE_SMALL_SPACING, B_USE_SMALL_SPACING)
		.Add(new BStringView("split_top_text", "Top Pane"));

	BGroupView* splitMiddle = new BGroupView("split_middle", B_VERTICAL,
		B_USE_DEFAULT_SPACING);
	splitMiddle->SetViewColor(224, 238, 224);
	splitMiddle->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNLIMITED));
	BLayoutBuilder::Group<>(splitMiddle, B_VERTICAL, B_USE_DEFAULT_SPACING)
		.SetInsets(B_USE_SMALL_SPACING, B_USE_SMALL_SPACING,
			B_USE_SMALL_SPACING, B_USE_SMALL_SPACING)
		.Add(new BStringView("split_middle_text", "Middle Pane"));

	BGroupView* splitBottom = new BGroupView("split_bottom", B_VERTICAL,
		B_USE_DEFAULT_SPACING);
	splitBottom->SetViewColor(247, 232, 220);
	splitBottom->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNLIMITED));
	BLayoutBuilder::Group<>(splitBottom, B_VERTICAL, B_USE_DEFAULT_SPACING)
		.SetInsets(B_USE_SMALL_SPACING, B_USE_SMALL_SPACING,
			B_USE_SMALL_SPACING, B_USE_SMALL_SPACING)
		.Add(new BStringView("split_bottom_text", "Bottom Pane"));

	BLayoutBuilder::Split<>(verticalSplit)
		.Add(splitTop)
		.Add(splitMiddle)
		.Add(splitBottom);

	BLayoutBuilder::Group<>(rightColumn, B_VERTICAL, B_USE_DEFAULT_SPACING)
		.Add(splitLabel)
		.Add(verticalSplit);

	fLayoutVerticalSplit = verticalSplit;
	fLayoutMiddlePane = splitMiddle;



	BSplitView* horizontalSplit = new BSplitView(B_HORIZONTAL,
		B_USE_DEFAULT_SPACING);
	BLayoutBuilder::Split<>(horizontalSplit)
		.Add(leftColumn)
		.Add(rightColumn);
	fLayoutHorizontalSplit = horizontalSplit;

	BButton* toggleMiddleButton = new BButton("layout_toggle_middle",
		"Toggle Middle Pane", new BMessage(LAYOUT_TOGGLE_MIDDLE));
	toggleMiddleButton->SetTarget(this);

	BButton* rotateSplitButton = new BButton("layout_rotate_split",
		"Rotate Panes", new BMessage(LAYOUT_ROTATE_SPLIT));
	rotateSplitButton->SetTarget(this);

	BGroupView* layoutButtonRow = new BGroupView("layout_button_row",
		B_HORIZONTAL, B_USE_DEFAULT_SPACING);
	layoutButtonRow->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	BLayoutBuilder::Group<>(layoutButtonRow, B_HORIZONTAL,
		B_USE_DEFAULT_SPACING)
		.Add(toggleMiddleButton)
		.Add(rotateSplitButton);

	fLayoutSpacingSlider = new BSlider("layout_spacing", "Spacing",
		new BMessage(LAYOUT_SPACING_CHANGED), 0, 16, B_HORIZONTAL);
	fLayoutSpacingSlider->SetModificationMessage(
		new BMessage(LAYOUT_SPACING_CHANGED));
	fLayoutSpacingSlider->SetValue(8);
	fLayoutSpacingSlider->SetHashMarkCount(17);
	fLayoutSpacingSlider->SetHashMarks(B_HASH_MARKS_BOTTOM);
	fLayoutSpacingSlider->SetLimitLabels("0", "16");
	fLayoutSpacingSlider->SetTarget(this);

	BGroupView* layoutControls = new BGroupView("layout_controls", B_VERTICAL,
		B_USE_DEFAULT_SPACING);
	layoutControls->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	BLayoutBuilder::Group<>(layoutControls, B_VERTICAL, B_USE_DEFAULT_SPACING)
		.Add(layoutButtonRow)
		.Add(fLayoutSpacingSlider);

	BLayoutBuilder::Group<>(layoutTabView, B_VERTICAL, B_USE_DEFAULT_SPACING)
		.SetInsets(B_USE_WINDOW_SPACING, B_USE_WINDOW_SPACING,
			B_USE_WINDOW_SPACING, B_USE_WINDOW_SPACING)
		.Add(layoutIntro)
		.Add(layoutControls)
		.Add(horizontalSplit);

	// Content for Controls Tab

	// Checkbox box
	BBox* aBox1 = new BBox(BRect(15, 10, 200, 90), "Box 1 (Check Boxes)");
	aBox1->SetLabel("Check Boxes");

	BGroupLayout* checkBoxLayout = new BGroupLayout(B_VERTICAL, kInset);
	checkBoxLayout->SetInsets(kInset + kTopInsetExtra, kInset + aBox1->TopBorderOffset() + kTopInsetExtra, kInset, kInset);
	aBox1->SetLayout(checkBoxLayout);

	BCheckBox* aCheckBox1 = new BCheckBox("check box 1", "Check Box 1", new BMessage(CHECK_ONE));
	BCheckBox* aCheckBox2 = new BCheckBox("check box 2", "Check Box 2", new BMessage(CHECK_TWO));
	checkBoxLayout->AddView(aCheckBox1);
	checkBoxLayout->AddView(aCheckBox2);

	controlsTabView->AddChild(aBox1);

	// Radio button box
	BBox* aBox2 = new BBox(BRect(15, 100, 200, 180), "Box 2 (Radio Buttons)");
	aBox2->SetLabel("Radio Buttons");
	
	BGroupLayout* radioBoxLayout = new BGroupLayout(B_VERTICAL, kInset);
	radioBoxLayout->SetInsets(kInset + kTopInsetExtra, kInset + aBox2->TopBorderOffset() + kTopInsetExtra, kInset, kInset);
	aBox2->SetLayout(radioBoxLayout);

	BRadioButton* aRadioBut1 = new BRadioButton("radio button 1", "Radio Button 1", new BMessage(RADIO_ONE));
	BRadioButton* aRadioBut2 = new BRadioButton("radio button 2", "Radio Button 2", new BMessage(RADIO_TWO));
	aRadioBut1->SetValue(B_CONTROL_ON);
	radioBoxLayout->AddView(aRadioBut1);
	radioBoxLayout->AddView(aRadioBut2);

	controlsTabView->AddChild(aBox2);

	// BSlider demo
	rgb_color fillColor = (rgb_color){ 255, 115, 0, 255 };
	BBox* aBox3 = new BBox(BRect(15, 195, 200, 330), "Box 5 (Slider)");
	aBox3->SetLabel("Sliders");

	BGroupLayout* sliderBoxLayout = new BGroupLayout(B_VERTICAL, kInset);
	sliderBoxLayout->SetInsets(kInset, kInset + aBox3->TopBorderOffset() + kTopInsetExtra, kInset, kInset);
	aBox3->SetLayout(sliderBoxLayout);

	BSlider* aSlider1 = new BSlider("slider", "Volume",
									new BMessage(B_PULSE), 0, 100, B_HORIZONTAL, B_BLOCK_THUMB, B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE);
	aSlider1->SetHashMarkCount(10);
	aSlider1->SetHashMarks(B_HASH_MARKS_BOTTOM);
	aSlider1->SetValue(20);
	sliderBoxLayout->AddView(aSlider1);
	
	BSlider* aSlider2 = new BSlider("slider", "Balance",
									new BMessage(B_PULSE), 0, 100, B_HORIZONTAL, B_TRIANGLE_THUMB, B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE);
	aSlider2->SetHashMarkCount(5);
	aSlider2->SetHashMarks(B_HASH_MARKS_BOTTOM);
	aSlider2->SetLimitLabels("L", "R");
	aSlider2->UseFillColor(true, &fillColor);
	aSlider2->SetValue(50);
	sliderBoxLayout->AddView(aSlider2);

	controlsTabView->AddChild(aBox3);

	// Button box
	BBox* aButtonBox = new BBox(BRect(215, 10, 400, 120), "Button Box");
	aButtonBox->SetLabel("Buttons");
	
	BGroupLayout* buttonBoxLayout = new BGroupLayout(B_VERTICAL, kInset);
	buttonBoxLayout->SetInsets(kInset + kTopInsetExtra, kInset + aButtonBox->TopBorderOffset() + kTopInsetExtra, kInset, kInset);
	aButtonBox->SetLayout(buttonBoxLayout);

	// Add a button which brings up a BAlert
	BButton* anAlertButton = new BButton("Alert Button", "Show Alert (sync)", new BMessage(SHOW_ALERT));
	anAlertButton->SetToolTip("Click me to show an alert");
	buttonBoxLayout->AddView(anAlertButton);

	BButton* anAsyncAlertButton = new BButton("Alert Button 2", "Show Alert (async)", new BMessage(SHOW_ALERT_ASYNC));
	anAsyncAlertButton->SetToolTip("Click me to show an alert asynchronously");
	buttonBoxLayout->AddView(anAsyncAlertButton);

	controlsTabView->AddChild(aButtonBox);

	// Icon Button box
	BBox* anIconButtonBox = new BBox(BRect(215, 140, 400, 225), "Icon Button Box");
	anIconButtonBox->SetLabel("Icon Buttons");

	BGroupLayout* iconButtonBoxLayout = new BGroupLayout(B_HORIZONTAL, kInset);
	iconButtonBoxLayout->SetInsets(kInset, kInset + anIconButtonBox->TopBorderOffset(), kInset, kInset);
	anIconButtonBox->SetLayout(iconButtonBoxLayout);

	status_t iconStatus = B_ERROR;
	BBitmap trackerIcon(BRect(0, 0, 31, 31), 0, B_RGBA32);

#if !defined(__HAIKU__)
	iconStatus = BIconUtils::GetAppIcon("tracker_icon", B_LARGE_ICON, &trackerIcon);
#else
	iconStatus = GetAppIcon("tracker_icon", B_LARGE_ICON, &trackerIcon);
#endif

	if (iconStatus == B_OK) {
		BBitmapButton* anIconButton = new BBitmapButton(reinterpret_cast<const uint8*>(trackerIcon.Bits()), 32, 32, B_RGBA32, new BMessage(SHOW_FILE_PANEL));
		anIconButton->ResizeTo(36, 36);
		anIconButton->MoveTo(450, 92);
		iconButtonBoxLayout->AddView(anIconButton);
	}

	BVectorImageButton* vectorButton = new(std::nothrow) BVectorImageButton(
		"tracker_icon", BSize(32, 32), new BMessage(SHOW_FILE_PANEL));
	if (vectorButton != NULL && vectorButton->InitCheck() == B_OK) {
		vectorButton->ResizeTo(36, 36);
		vectorButton->MoveTo(496, 92);
		vectorButton->SetAutoscale(true);
		iconButtonBoxLayout->AddView(vectorButton);
	} else {
		status_t vectorStatus = vectorButton != NULL ? vectorButton->InitCheck() : B_NO_MEMORY;
		fprintf(stderr, "Failed to create tracker vector button: %s\n",
			strerror(vectorStatus));
		delete vectorButton;
	}

	controlsTabView->AddChild(anIconButtonBox);

	// Add a box for a scrollbar sample
	BBox* aBox4 = new BBox(BRect(415, 10, 600, 75), "Box 4 (Scrollbar)", B_FOLLOW_LEFT_RIGHT);
	BStringView* scrollString = new BStringView(BRect(10, 15, 170, 34), "scrolling string view", "Use the horizontal scrollbar below to scroll this string of text.", B_FOLLOW_LEFT_RIGHT);
	BScrollBar* horizScroll = new BScrollBar(BRect(10, 35, 170, 35 + B_H_SCROLL_BAR_HEIGHT), "horizontal scrollbar", scrollString, 0, 170, B_HORIZONTAL);
	//horizScroll->SetProportion( 0.5 );
	aBox4->AddChild(scrollString);
	aBox4->AddChild(horizScroll);
	aBox4->SetLabel("Horizontal ScrollBar");
	controlsTabView->AddChild(aBox4);

	BTextControl* aTextControl = new BTextControl(BRect(215, 245, 480, 270), "a text control",
										 "Window Name:",
										 "Cosmoe Showcase", NULL, B_FOLLOW_LEFT_RIGHT);
	controlsTabView->AddChild(aTextControl);
	aTextControl->SetModificationMessage(new BMessage(TEXT_CHANGED));
	aTextControl->SetTarget(this);

	BMenu* colorMenu = new BPopUpMenu("color");
	colorMenu->AddItem(new BMenuItem("Red", NULL));
	colorMenu->AddItem(new BMenuItem("Yellow", NULL));
	colorMenu->AddItem(new BMenuItem("Green", NULL));
	colorMenu->AddItem(new BMenuItem("Blue", NULL));
	colorMenu->AddItem(new BMenuItem("Purple", NULL));
	colorMenu->AddItem(new BMenuItem("White", NULL));
	colorMenu->AddItem(new BMenuItem("Black", NULL));
	colorMenu->AddItem(new BMenuItem("Gray", NULL));
	colorMenu->ItemAt(0)->SetMarked(true);
	colorMenu->SetLabelFromMarked(true);

	BMenuField*		menuField = new BMenuField(BRect(215, 285, 480, 320), "menu field", "Favorite Color:", colorMenu);
	controlsTabView->AddChild(menuField);

	// ChannelSlider demo

	BChannelSlider* channelSlider = new BChannelSlider(BRect(415, 100, 585, 140),
		"channel slider", "Channel Slider", NULL, 1);
	controlsTabView->AddChild(channelSlider);

	BDecimalSpinner* spinner = new BDecimalSpinner(BRect(415, 175, 585, 198), "spinner", "Spinner", NULL);
	controlsTabView->AddChild(spinner);
	
	// Content for GUI Elements Tab

	mStatusBar = new BStatusBar(BRect(15, 15, 255, 75), "status bar", "Progress", "% Done");
	mStatusBar->SetResizingMode(B_FOLLOW_NONE);
	mStatusBar->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	guiElementsTabView->AddChild(mStatusBar);

	BColorControl* colorControl = new BColorControl(BPoint(290, 15), B_CELLS_32x8,
		8.0f, "color control");
	guiElementsTabView->AddChild(colorControl);

	// ColumnListView demo
	r = BRect(15, 115, 325, 339);
	BColumnListView* listView = new BColumnListView(r, "gridview", B_FOLLOW_ALL, B_WILL_DRAW, B_FANCY_BORDER);
	guiElementsTabView->AddChild(listView);
	
	float width = be_plain_font->StringWidth("00000") + 20;
	listView->AddColumn(new BStringColumn("ID", width, width, 100, B_TRUNCATE_END), 0);
	
	listView->AddColumn(new BStringColumn("Type", width, width, 100, B_TRUNCATE_END), 1);
	listView->AddColumn(new BStringColumn("Name", 70, 50, 300, B_TRUNCATE_END), 2);
	listView->AddColumn(new BSizeColumn("Size", 70, 50, 300), 3);

	for (int32 i = 0; i < 25; i++)
		listView->AddRow(new SampleDataRow());

	r = BRect(345, 117, 620 - B_V_SCROLL_BAR_WIDTH, 337 - B_H_SCROLL_BAR_HEIGHT);
	BIconOutlineListView* outlineView = new BIconOutlineListView(r, "outlineview",
		B_SINGLE_SELECTION_LIST, B_FOLLOW_ALL, B_WILL_DRAW);
	BScrollView* outlineScroller = new BScrollView("outline_scroller",
		outlineView, B_FOLLOW_TOP_BOTTOM | B_FOLLOW_RIGHT, 0, true, true, B_FANCY_BORDER);
	guiElementsTabView->AddChild(outlineScroller);

	BBitmap outlineIcon(BRect(0, 0, 31, 31), 0, B_RGBA32);
	const BBitmap* outlineIconPtr = NULL;
#if !defined(__HAIKU__)
	if (BIconUtils::GetAppIcon("BEOS:ICON", B_LARGE_ICON, &outlineIcon) == B_OK)
		outlineIconPtr = &outlineIcon;
#else
	if (GetAppIcon("BEOS:ICON", B_LARGE_ICON, &outlineIcon) == B_OK)
		outlineIconPtr = &outlineIcon;
#endif

	BIconStringItem* rootApplications = new BIconStringItem("Applications", outlineIconPtr);
	BIconStringItem* rootMedia = new BIconStringItem("Media", outlineIconPtr);
	BIconStringItem* rootSystem = new BIconStringItem("System", outlineIconPtr);

	outlineView->AddItem(rootApplications);
	outlineView->AddItem(rootMedia);
	outlineView->AddItem(rootSystem);

	outlineView->AddUnder(new BIconStringItem("Showcase", outlineIconPtr), rootApplications);
	outlineView->AddUnder(new BIconStringItem("StyledEdit", NULL), rootApplications);
	outlineView->AddUnder(new BIconStringItem("Terminal", outlineIconPtr), rootApplications);

	BIconStringItem* mediaAudio = new BIconStringItem("Audio", NULL);
	BIconStringItem* mediaImages = new BIconStringItem("Images", NULL);
	outlineView->AddUnder(mediaAudio, rootMedia);
	outlineView->AddUnder(mediaImages, rootMedia);
	outlineView->AddUnder(new BIconStringItem("Pulse", outlineIconPtr), mediaAudio);
	outlineView->AddUnder(new BIconStringItem("DeskCalc Notification Sound", NULL), mediaAudio);
	outlineView->AddUnder(new BIconStringItem("ShowImage", outlineIconPtr), mediaImages);
	outlineView->AddUnder(new BIconStringItem("Icon-O-Matic", NULL), mediaImages);

	BIconStringItem* systemDevices = new BIconStringItem("Devices", NULL);
	BIconStringItem* systemServices = new BIconStringItem("Services", NULL);
	outlineView->AddUnder(systemDevices, rootSystem);
	outlineView->AddUnder(systemServices, rootSystem);
	outlineView->AddUnder(new BIconStringItem("Display", outlineIconPtr), systemDevices);
	outlineView->AddUnder(new BIconStringItem("Input", NULL), systemDevices);
	outlineView->AddUnder(new BIconStringItem("Storage", outlineIconPtr), systemDevices);
	outlineView->AddUnder(new BIconStringItem("app_server", outlineIconPtr), systemServices);
	outlineView->AddUnder(new BIconStringItem("registrar", NULL), systemServices);
	outlineView->AddUnder(new BIconStringItem("media_server", outlineIconPtr), systemServices);

	outlineView->Expand(rootApplications);
	outlineView->Expand(rootMedia);
	outlineView->Expand(rootSystem);
	outlineView->Expand(mediaAudio);
	outlineView->Expand(mediaImages);
	outlineView->Expand(systemDevices);
	outlineView->Expand(systemServices);
	outlineView->Select(0);

	// Testing Tab content

	BitmapView* bitmapView = new BitmapView(BRect(310, 15, 530, 155), "bitmap view", B_FOLLOW_NONE);
	testingTabView->AddChild(bitmapView);

	// View clipping test - this view is intentionally larger than its parent and offset such that part of it
	// will be outside the bounds of its parent, to test that view clipping is working correctly.
	BitmapView* embeddedView = new BitmapView(BRect(-20, -20, 110, 110), "embedded view", B_FOLLOW_NONE);
	BView* embeddedParent = new BView(BRect(540, 40, 620, 130), "embedded parent", B_FOLLOW_NONE, B_WILL_DRAW);
	embeddedParent->SetViewUIColor(B_SHADOW_COLOR);
	embeddedParent->AddChild(embeddedView);
	testingTabView->AddChild(embeddedParent);

	// Add our pixel-accurate draw testing view
	DisView* aDisView = new DisView(BRect(15, 14, 296, 320), "DisView");
	testingTabView->AddChild(aDisView);

	BButton* ShowHideButton = new BButton(BRect(320, 175, 450, 190), "show-hide button", "Show / Hide View", new BMessage(SHOW_HIDE_VIEW));
	testingTabView->AddChild(ShowHideButton);

	BButton* ResizeButton = new BButton(BRect(470, 175, 570, 190), "resize button", "Resize View", new BMessage(RESIZE_VIEW));
	testingTabView->AddChild(ResizeButton);

	// Move bitmap placeholders to the bottom of the Draw Testing tab
	BPlaceholder* placeA = new BPlaceholder(BRect(310, 245, 410, 340), "Bitmap Placeholder 1", B_FOLLOW_NONE);
#ifndef __HAIKU__
	// SetViewBitmap crashes on Haiku
	placeA->SetViewBitmap(fIcon, 4626U, B_TILE_BITMAP_X);
#endif
	testingTabView->AddChild(placeA);

	BPlaceholder* placeB = new BPlaceholder(BRect(420, 245, 520, 340), "Bitmap Placeholder 2", B_FOLLOW_NONE);
#ifndef __HAIKU__
	placeB->SetViewBitmap(fIcon, 4626U, B_TILE_BITMAP_Y);
#endif
	testingTabView->AddChild(placeB);

	BPlaceholder* placeC = new BPlaceholder(BRect(530, 245, 630, 340), "Bitmap Placeholder 3", B_FOLLOW_LEFT_RIGHT);
#ifndef __HAIKU__
	placeC->SetViewBitmap(fIcon, 4626U, B_TILE_BITMAP);
#endif
	testingTabView->AddChild(placeC);

	// Content for Launcher Tab

	// Fill the launcher tab with the icon view
	BRect iconViewRect = launcherTabView->Bounds();

	IconView* iconView = new IconView(iconViewRect, B_FOLLOW_ALL);
	launcherTabView->AddChild(iconView);
}


void DisWindow::SetupMenus()
{
	mMenuBar = new BMenuBar("Menubar");

	BMenu* fileMenu = new BMenu( "File" );
	fileMenu->AddItem(new BMenuItem("Open" B_UTF8_ELLIPSIS, new BMessage(SHOW_FILE_PANEL), 'O'));
	fileMenu->AddItem(new BMenuItem("Save As" B_UTF8_ELLIPSIS, new BMessage(SHOW_SAVE_PANEL), 'S'));
	fileMenu->AddSeparatorItem();
	fileMenu->AddItem(new BMenuItem("Quit", new BMessage(B_QUIT_REQUESTED), 'Q'));
	mMenuBar->AddItem( fileMenu );

	BMenu* editMenu = new BMenu( "Edit" );
	editMenu->AddItem(new BMenuItem("Undo", new BMessage( B_UNDO ), 'Z'));
	editMenu->AddSeparatorItem();
	editMenu->AddItem(new BMenuItem("Cut", new BMessage( B_CUT ), 'X'));
	editMenu->AddItem(new BMenuItem("Copy", new BMessage( B_COPY ), 'C'));
	editMenu->AddItem(new BMenuItem("Paste", new BMessage( B_PASTE ), 'V'));
	mMenuBar->AddItem( editMenu );

	BMenu* testingMenu = new BMenu( "Menu Testing" );
	testingMenu->AddItem(new BMenuItem("Test Item 1", new BMessage(B_UNDO), '1'));
	testingMenu->AddItem(new BMenuItem("Test Item 2", new BMessage(B_UNDO), '2'));
	
	// Create a submenu for Test Item 3
	BMenu* subMenu1 = new BMenu("Test Item 3");
	subMenu1->AddItem(new BMenuItem("Sub Item 3.1", new BMessage(B_UNDO)));
	subMenu1->AddItem(new BMenuItem("Sub Item 3.2", new BMessage(B_UNDO)));
	
	// Create a deeper submenu for Sub Item 3.3
	BMenu* subMenu2 = new BMenu("Sub Item 3.3");
	subMenu2->AddItem(new BMenuItem("Deep Item 3.3.1", new BMessage(B_UNDO)));
	subMenu2->AddItem(new BMenuItem("Deep Item 3.3.2", new BMessage(B_UNDO)));
	
	// Create an even deeper submenu for Deep Item 3.3.3, with a disabled item
	BMenu* subMenu3 = new BMenu("Deep Item 3.3.3");
	subMenu3->AddItem(new BMenuItem("Deeper Item 3.3.3.1", new BMessage(B_UNDO)));
	BMenuItem* disabledItem = new BMenuItem("Deeper Item 3.3.3.2", new BMessage(B_UNDO));
	disabledItem->SetEnabled(false);
	subMenu3->AddItem(disabledItem);
	subMenu3->AddItem(new BMenuItem("Deeper Item 3.3.3.3", new BMessage(B_UNDO)));
	
	subMenu2->AddItem(subMenu3);
	subMenu1->AddItem(subMenu2);
	testingMenu->AddItem(subMenu1);
	
	mMenuBar->AddItem(testingMenu);
	mMenuBar->SetTargetForItems(this);
}


void DisWindow::MessageReceived(BMessage* message)
{
	switch(message->what)
	{
		case CHECK_ONE:
			printf("Checkbox #1 clicked\n");
			{
				if (mStatusBar) {
					mStatusBar->SetTo(0.0);
					printf("status bar reset to 0\n");
				} else {
					printf("Couldn't find status bar\n");
				}
			}

			BWindow::MessageReceived(message);
			break;

		case CHECK_TWO:
			printf("Checkbox #2 clicked\n");
			BWindow::MessageReceived(message);
			break;

		case SHOW_FILE_PANEL:
			{
				if (fFilePanel) {
					fFilePanel->Show();
				} else {
					printf("Warning: File panel not initialized\n");
				}
			}
			break;

		case SHOW_SAVE_PANEL:
			{
				if (fSavePanel) {
					fSavePanel->Show();
				} else {
					printf("Warning: Save panel not initialized\n");
				}
			}
			break;

		case SHOW_ALERT:
			{
				BAlert* alert = new BAlert("Alert", "This is a sample warning alert.", "OK");

				if (alert) {
					alert->SetType(B_WARNING_ALERT);
					alert->SetFlags(alert->Flags() | B_CLOSE_ON_ESCAPE);
					alert->Go(NULL);
				}
			}
			break;

		case SHOW_ALERT_ASYNC:
			{
				BAlert* alert = new BAlert("Async Alert", "This is a sample asynchronous info alert.", "Red", "Blue", "Green");

				if (alert) {
					alert->SetFlags(alert->Flags() | B_CLOSE_ON_ESCAPE);
					int result = alert->Go();
					printf("Asynchronous alert closed with result: %d\n", result);
				}
			}
			break;

		case MOVE_WINDOW:
			{
				BPoint newPos(4, 8);
				printf("Moving window to (%.1f, %.1f)\n", newPos.x, newPos.y);
				MoveTo(newPos);
				// Update system info display
				SystemInfoView* sysInfoView = dynamic_cast<SystemInfoView*>(FindView("system_info"));
				if (sysInfoView) {
					sysInfoView->UpdateInfo();
				}
			}
			break;

		case CENTER_WINDOW:
			{
				CenterOnScreen();
				
				// Update system info display
				SystemInfoView* sysInfoView = dynamic_cast<SystemInfoView*>(FindView("system_info"));
				if (sysInfoView) {
					sysInfoView->UpdateInfo();
				}
			}
			break;
		case MOVE_LEFT:
			{
				MoveBy(-20, 0);
				// Update system info display
				SystemInfoView* sysInfoView = dynamic_cast<SystemInfoView*>(FindView("system_info"));
				if (sysInfoView) {
					sysInfoView->UpdateInfo();
				}
			}
			break;

		case MOVE_UP:
			{
				MoveBy(0, -20);
				// Update system info display
				SystemInfoView* sysInfoView = dynamic_cast<SystemInfoView*>(FindView("system_info"));
				if (sysInfoView) {
					sysInfoView->UpdateInfo();
				}
			}
			break;

		case MOVE_RIGHT:
			{
				MoveBy(20, 0);
				// Update system info display
				SystemInfoView* sysInfoView = dynamic_cast<SystemInfoView*>(FindView("system_info"));
				if (sysInfoView) {
					sysInfoView->UpdateInfo();
				}
			}
			break;

		case MOVE_DOWN:
			{
				MoveBy(0, 20);
				// Update system info display
				SystemInfoView* sysInfoView = dynamic_cast<SystemInfoView*>(FindView("system_info"));
				if (sysInfoView) {
					sysInfoView->UpdateInfo();
				}
			}
			break;

		case EXPAND_WINDOW:
			{
				ResizeBy(32, 32);
				// Update system info display
				SystemInfoView* sysInfoView = dynamic_cast<SystemInfoView*>(FindView("system_info"));
				if (sysInfoView) {
					sysInfoView->UpdateInfo();
				}
			}
			break;

		case SHRINK_WINDOW:
			{
				ResizeBy(-32, -32);
				// Update system info display
				SystemInfoView* sysInfoView = dynamic_cast<SystemInfoView*>(FindView("system_info"));
				if (sysInfoView) {
					sysInfoView->UpdateInfo();
				}
			}
			break;

		case SHOW_HIDE_VIEW:
			{
				BView* view = FindView("Bitmap Placeholder 3");
				if (view) {
					if (view->IsHidden()) {
						view->Show();
					} else {
						view->Hide();
					}
				} else {
					printf("Warning: Couldn't find view to show/hide\n");
				}
			}
			break;

		case RESIZE_VIEW:
			{
				BView* view = FindView("Bitmap Placeholder 3");
				if (view) {
					if (fResized) {
						view->ResizeBy(20, 20);
					} else {
						view->ResizeBy(-20, -20);
					}
					fResized = !fResized;
				} else {
					printf("Warning: Couldn't find view to resize\n");
				}
			}
			break;

		case TEXT_CHANGED:
			{
				BTextControl* textControl = dynamic_cast<BTextControl*>(FindView("a text control"));
				if (textControl) {
					SetTitle(textControl->Text());
				}
			}
			break;

		case LAYOUT_TOGGLE_MIDDLE:
			{
				if (fLayoutVerticalSplit != NULL && fLayoutMiddlePane != NULL) {
					if (fLayoutMiddleVisible) {
						if (fLayoutMiddlePane->Parent() == fLayoutVerticalSplit)
							fLayoutMiddlePane->RemoveSelf();
						fLayoutMiddleVisible = false;
					} else {
						if (fLayoutMiddlePane->Parent() == NULL)
							fLayoutVerticalSplit->AddChild(1, fLayoutMiddlePane, 1.0f);
						fLayoutMiddlePane->Show();
						fLayoutMiddleVisible = true;
					}
				} else if (fLayoutMiddlePane != NULL) {
					fLayoutMiddleVisible = !fLayoutMiddleVisible;
					if (fLayoutMiddleVisible)
						fLayoutMiddlePane->Show();
					else
						fLayoutMiddlePane->Hide();
				}
			}
			break;

		case LAYOUT_ROTATE_SPLIT:
			{
				if (fLayoutVerticalSplit != NULL) {
					fLayoutVertical = !fLayoutVertical;
					fLayoutVerticalSplit->SetOrientation(
						fLayoutVertical ? B_VERTICAL : B_HORIZONTAL);
				}
			}
			break;

		case LAYOUT_SPACING_CHANGED:
			{
				if (fLayoutSpacingSlider != NULL) {
					int32 spacing = fLayoutSpacingSlider->Value();

					if (fLayoutVerticalSplit != NULL)
						fLayoutVerticalSplit->SetSpacing((float)spacing);
					if (fLayoutHorizontalSplit != NULL)
						fLayoutHorizontalSplit->SetSpacing((float)spacing);
				}
			}
			break;
		
		default:
			BWindow::MessageReceived(message);
			break;
	}
}


void DisWindow::IncrementBar()
{
	// Increment progress bar on the GUI Elements tab
	if (mStatusBar) {
		float currentValue = mStatusBar->CurrentValue();
		float newValue = currentValue + 1.0f;
		
		// Reset to 0 if we reach 100
		if (newValue > 100.0f) {
			newValue = 0.0f;
		}
		
		mStatusBar->SetTo(newValue);
	}
}


//	#pragma mark - IconView



IconView::IconView(BRect rect, uint32 followFlags)
	:
	BView(rect, "logo", followFlags, B_WILL_DRAW)
{
	// Allocate icons per entry; prefer 32x32 RGBA for vector/bitmap icons
	const int32 iconSize = 128;

	auto load_app_icon = [&](AppEntry& entry, BResources& res) {
		status_t iconErr = B_BAD_VALUE;

		// Try vector icon from embedded resources (res already initialized by caller)
		size_t size = 0;
		const void* data = res.LoadResource(B_VECTOR_ICON_TYPE, "BEOS:ICON", &size);

		if (data != NULL /* && size > 0*/) {
			iconErr = BIconUtils::GetVectorIcon(static_cast<const uint8*>(data), size, entry.icon);
		} else {
			printf("No vector icon resource in '%s'\n", entry.path.c_str());
		}

		// Last resort: system icon so we still render something
		if (iconErr != B_OK) {
			iconErr = BIconUtils::GetSystemIcon("dialog-information", entry.icon);
		}

		if (iconErr != B_OK) {
			printf("Error loading app icon from '%s': %s\n", entry.path.c_str(), strerror(iconErr));
		}
	};

	auto add_app = [&](const char* name, const char* description = "") {
		std::string fullPath = std::string("./") + name;
		std::string resourcePath = std::string("./") + name;
#ifdef __linux__
		fullPath = std::string("/usr/local/bin/") + name;
		resourcePath = fullPath;
#elif __APPLE__
		fullPath = std::string("/usr/local/Applications/") + name + ".app";
		resourcePath = fullPath;

		BEntry ent(fullPath.c_str(), true);
		if (ent.Exists()) {
			resourcePath += "/Contents/MacOS/";
			resourcePath += name;
		} else {
			// This might be a non-bundled app
			fullPath = std::string("/usr/local/bin") + name;
			ent.SetTo(fullPath.c_str(), true);
			if (ent.Exists() == false) {
				// This app isn't compiled for Mac
				return;
			}
		}
#elif _WIN32
		fullPath = std::string(name) + ".exe";
		resourcePath = fullPath;
#elif __HAIKU__
		BEntry ent(fullPath.c_str(), true);
		if (!ent.Exists()) {
			fullPath = std::string("/boot/system/apps/") + name;
			ent.SetTo(fullPath.c_str(), true);
		}
		if (!ent.Exists()) {
			fullPath = std::string("/boot/system/demos/") + name;
			ent.SetTo(fullPath.c_str(), true);
		}
		if (!ent.Exists()) {
			// App not found, skip it
			return;
		}

		resourcePath = fullPath;
#endif

		AppEntry entry;
		entry.name = name;
		entry.path = fullPath;
		entry.description = description;
		entry.icon = new(std::nothrow) BBitmap(BRect(0, 0, iconSize - 1, iconSize - 1), 0, B_RGBA32);
		
		// Load resources once and use for both icon and description
		BResources res;
		if (res.SetTo(resourcePath.c_str(), false) == B_OK) {
			// Try to load app_description string resource
			size_t size = 0;
			const void* data = res.LoadResource(B_STRING_TYPE, "app_description", &size);
			if (data != NULL && size > 0) {
				std::string appDescription(static_cast<const char*>(data), size);
				// Remove trailing null if present
				if (!appDescription.empty() && appDescription.back() == '\0') {
					appDescription.pop_back();
				}
				entry.description = appDescription;
			}
			
			// Load icon using the same resources object
			load_app_icon(entry, res);
		} else {
			// If we can't load resources, still try to load a fallback icon
			load_app_icon(entry, res);
		}
		
		fApps.push_back(entry);
	};

	add_app("DeskCalc", "Simple calculator application");
	add_app("Pairs", "Matching game");
#if defined(__linux__) || defined(__APPLE__) || defined(__HAIKU__)
	add_app("Terminal");
#endif
	add_app("StyledEdit");
	add_app("Showcase");
	add_app("Mandelbrot", "Fractal explorer");
	//add_app("ResEdit");	// Need fallback icon support
	add_app("Clock", "BeOS clock sample application");
	add_app("Sudoku", "Puzzle game");
	add_app("ShowImage", "Image viewer application");
	add_app("Pulse");
	add_app("Gradients", "Gradient viewer application");
	add_app("FontDemo", "Font effects application");
	add_app("Icon-O-Matic", "Vector icon editor");
	add_app("AboutSystem", "System information");

	// Initialize mouse tracking
	fMousePos.Set(-1000, -1000);  // Start offscreen
	fMouseInView = false;
	fMouseDown = false;
	fHoveredAppIndex = -1;  // No app hovered initially
	fViewCursor = NULL;
	SetEventMask(B_POINTER_EVENTS, 0);
}


void
IconView::AttachedToWindow()
{
	BView::AttachedToWindow();
	if (fViewCursor != NULL) {
		SetViewCursor(fViewCursor, false);
		return;
	}

	int32 cursorSize = 22;
	float shadow = 3 / 10.0;

	BBitmap* pointerCursorBitmap = RenderVectorCursor(cursorSize, kCursorPointingFinger, sizeof(kCursorPointingFinger), shadow);

	if (pointerCursorBitmap != NULL) {
		fViewCursor = new(std::nothrow) BCursor(pointerCursorBitmap, BPoint(0, 0));
		if (fViewCursor != NULL && fViewCursor->InitCheck() == B_OK)
			SetViewCursor(fViewCursor, false);
		else
			printf("Failed to create BCursor from rendered HVIF bitmap\n");

		delete pointerCursorBitmap;
	} else {
		printf("Failed to create custom cursor bitmap, using default cursor\n");
	}
}


IconView::~IconView()
{
	if (fViewCursor != NULL) {
		delete fViewCursor;
		fViewCursor = NULL;
	}

	for (auto& app : fApps) {
		if (app.icon != NULL)
			delete app.icon;
	}
}


void
IconView::Draw(BRect updateRect)
{
	if (fApps.empty())
		return;

	BRect bounds(Bounds());
	
	// Create a linear gradient from light gray at top to sky blue at bottom
	BGradientLinear gradient;
	gradient.SetStart(BPoint(0, bounds.top));
	gradient.SetEnd(BPoint(0, bounds.bottom));
	gradient.AddColor(rgb_color{255, 255, 255, 255}, 0.0f);    // Light gray at top
	gradient.AddColor(rgb_color{135, 206, 235, 255}, 255.0f);  // Sky blue at bottom
	
	FillRect(bounds, gradient);

	PushState();
	SetDrawingMode(B_OP_ALPHA);
	SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);

	// Draw centered text at the top
	BFont font;
	GetFont(&font);
	font.SetFace(B_BOLD_FACE);
	font.SetSize(36);
	SetFont(&font);
	
	const char* text = "Welcome to Cosmoe";
	float textWidth = font.StringWidth(text);
	float textX = (bounds.Width() - textWidth) / 2;
	float textY = 64;  // Position from top
	
	SetHighColor(50, 50, 50);  // Dark gray for good contrast
	DrawString(text, BPoint(textX, textY));

	// Draw a row of icons with magnification based on mouse proximity. When magnified,
	// spread the icons horizontally so they do not overlap (similar to the macOS dock).
	int padding_h = 64;
	int padding_v = 120;
	const float count = static_cast<float>(fApps.size());
	float baseWidth = (Bounds().Width() - padding_h) / count;
	const float maxScale = 3.0f;  // Maximum 3x magnification
	const float influenceRadius = baseWidth * 2.5f;  // Distance of influence

	// First pass: compute scale per icon using their unshifted base positions
	std::vector<float> scales(fApps.size(), 1.0f);
	for (size_t i = 0; i < fApps.size(); i++) {
		float iconCenterX = padding_h + (baseWidth * i) + baseWidth / 2;
		float iconCenterY = padding_v + baseWidth / 2;

		if (fMouseInView) {
			float dx = fMousePos.x - iconCenterX;
			float dy = fMousePos.y - iconCenterY;
			float distance = sqrt(dx * dx + dy * dy);

			if (distance < influenceRadius) {
				float normalizedDist = distance / influenceRadius;
				scales[i] = 1.0f + (maxScale - 1.0f) * (1.0f - normalizedDist);
			}
		}
	}

	// Second pass: lay out icons centered left-to-right using their scaled widths to avoid overlap
	float totalWidth = 0.0f;
	for (float s : scales)
		totalWidth += baseWidth * s;

	const float edgeBuffer = 20.0f;  // keep a small buffer from view edges when possible
	const float availableWidth = Bounds().Width() - 2.0f * edgeBuffer;
	float centered = (Bounds().Width() - totalWidth) / 2.0f;

	float currentX;
	if (totalWidth <= availableWidth) {
		float minX = edgeBuffer;
		float maxX = Bounds().Width() - edgeBuffer - totalWidth;
		currentX = std::max(minX, std::min(centered, maxX));
	} else {
		// Too wide: center and let it overflow symmetrically instead of pinning to the left.
		currentX = centered;
	}
	float baseline = padding_v + baseWidth;  // Bottom edge of base icon
	std::string hoverLabel;
	fHoveredAppIndex = -1;  // Reset at start of each draw
	for (size_t i = 0; i < fApps.size(); i++) {
		if (fApps[i].icon == NULL)
			continue;

		float scaledWidth = baseWidth * scales[i];
		float scaledHeight = baseWidth * scales[i];

		float iconCenterX = currentX + scaledWidth / 2;
		BRect r(
			iconCenterX - scaledWidth / 2,
			baseline - scaledHeight,
			iconCenterX + scaledWidth / 2,
			baseline
		);

		bool isHovered = fMouseInView && r.Contains(fMousePos);
		if (isHovered) {
			fHoveredAppIndex = static_cast<int>(i);
			hoverLabel = fApps[i].name + "\n" + fApps[i].description;
		}

		// Draw the icon
		
		// If mouse is down over this icon, draw it darker
		if (fMouseDown && isHovered) {
			// First draw the icon normally
			SetDrawingMode(B_OP_OVER);
			SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
			DrawBitmap(fApps[i].icon, fApps[i].icon->Bounds().OffsetToCopy(B_ORIGIN), r, B_FILTER_BITMAP_BILINEAR);
			
			// Then darken it by drawing it again with B_OP_MIN (takes minimum of each pixel)
			// This caps the brightness at 60%, respecting the alpha channel
			SetDrawingMode(B_OP_MIN);
			SetHighColor(153, 153, 153, 255);  // 60% gray (153/255 ≈ 0.6)
			DrawBitmap(fApps[i].icon, fApps[i].icon->Bounds().OffsetToCopy(B_ORIGIN), r, B_FILTER_BITMAP_BILINEAR);
			SetDrawingMode(B_OP_OVER);
		} else {
			// Normal drawing - slightly translucent
			SetDrawingMode(B_OP_OVER);
			SetBlendingMode(B_CONSTANT_ALPHA, B_ALPHA_OVERLAY);
			SetHighColor(255, 255, 255, 225);  // Just a touch of transparency
			DrawBitmap(fApps[i].icon, fApps[i].icon->Bounds().OffsetToCopy(B_ORIGIN), r, B_FILTER_BITMAP_BILINEAR);
		}
		
		currentX += scaledWidth;
	}

	if (!hoverLabel.empty()) {
		BFont labelFont;
		GetFont(&labelFont);
		labelFont.SetSize(16);
		SetFont(&labelFont);
		SetHighColor(30, 30, 30);
		float labelWidth = std::min(labelFont.StringWidth(hoverLabel.c_str()), Bounds().Width() - 32.0f);
		float labelX = (Bounds().Width() - labelWidth) / 2.0f;
		float labelY = baseline + 64.0f;
		DrawString(hoverLabel.c_str(), BPoint(labelX, labelY));
	}

	PopState();
}


void
IconView::MouseMoved(BPoint where, uint32 code, const BMessage* dragMessage)
{
	if (code == B_ENTERED_VIEW) {
		fMouseInView = true;
	} else if (code == B_EXITED_VIEW) {
		fMouseInView = false;
	}
	
	fMousePos = where;
	Invalidate();  // Redraw with updated mouse position
}


void
IconView::MouseDown(BPoint where)
{
	fMouseDown = true;
	Invalidate();  // Redraw to show darkened icon
}


void
IconView::MouseUp(BPoint where)
{
	fMouseDown = false;
	
	// Check if an app is hovered and launch it
	if (fHoveredAppIndex >= 0 && fHoveredAppIndex < static_cast<int>(fApps.size())) {
		const AppEntry& app = fApps[fHoveredAppIndex];
		printf("Launching: %s\n", app.name.c_str());
		
#if defined(__linux__) || defined(__HAIKU__) 
		// Linux: use fork/exec
		pid_t pid = fork();
		if (pid == 0) {
			// Child process
			execl(app.path.c_str(), app.name.c_str(), (char*)NULL);
			// If exec fails, exit
			exit(1);
		}
#elif defined(__APPLE__)
		// macOS: use 'open' command for .app bundles or direct exec for binaries
		if (app.path.find(".app") != std::string::npos) {
			// It's an app bundle, use 'open' command
			std::string command = "open \"" + app.path + "\" &";
			system(command.c_str());
		} else {
			// Regular binary
			pid_t pid = fork();
			if (pid == 0) {
				execl(app.path.c_str(), app.name.c_str(), (char*)NULL);
				exit(1);
			}
		}
#elif defined(_WIN32)
		// Windows: use start command
		std::string command = "start \"\" \"" + app.path + "\"";
		system(command.c_str());
#endif
	}
	
	Invalidate();  // Redraw to remove darkening
}

//	#pragma mark - SystemInfoView

SystemInfoView::SystemInfoView(BRect rect, uint32 followFlags)
	: BView(rect, "system_info", followFlags, B_WILL_DRAW | B_PULSE_NEEDED)
{
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	
	// Create labels for system information
	float yPos = 15;
	float xPos = 15;
	float labelHeight = 20;
	float spacing = 25;
	
	// Window position
	fWindowPosLabel = new BStringView(BRect(xPos, yPos, xPos + 400, yPos + labelHeight), 
		"window_pos", "Window Position: (0, 0)", B_FOLLOW_LEFT | B_FOLLOW_TOP);
	AddChild(fWindowPosLabel);
	yPos += spacing;
	
	// Mouse position
	fMousePosLabel = new BStringView(BRect(xPos, yPos, xPos + 400, yPos + labelHeight), 
		"mouse_pos", "TabView Mouse Position: (0, 0)", B_FOLLOW_LEFT | B_FOLLOW_TOP);
	AddChild(fMousePosLabel);
	yPos += spacing;
	
	// Window size
	fWindowSizeLabel = new BStringView(BRect(xPos, yPos, xPos + 400, yPos + labelHeight), 
		"window_size", "Window Size: 0 x 0", B_FOLLOW_LEFT | B_FOLLOW_TOP);
	AddChild(fWindowSizeLabel);
	yPos += spacing;
	
	// Screen size
	fScreenSizeLabel = new BStringView(BRect(xPos, yPos, xPos + 400, yPos + labelHeight), 
		"screen_size", "Screen Size: 0 x 0", B_FOLLOW_LEFT | B_FOLLOW_TOP);
	AddChild(fScreenSizeLabel);
	yPos += spacing;
	
	// Backend
	fBackendLabel = new BStringView(BRect(xPos, yPos, xPos + 400, yPos + labelHeight), 
		"backend", "Backend: Unknown", B_FOLLOW_LEFT | B_FOLLOW_TOP);
	AddChild(fBackendLabel);
	yPos += spacing;
	
	// Scale
	fScaleLabel = new BStringView(BRect(xPos, yPos, xPos + 400, yPos + labelHeight), 
		"scale", "Backend Scale: 1.0", B_FOLLOW_LEFT | B_FOLLOW_TOP);
	AddChild(fScaleLabel);
	yPos += spacing;

	// Backend
	fControlLookLabel = new BStringView(BRect(xPos, yPos, xPos + 600, yPos + labelHeight), 
		"control_look", "Control Look: Unknown", B_FOLLOW_LEFT | B_FOLLOW_TOP);
	AddChild(fControlLookLabel);
	yPos += spacing + 20;
	
	// Add window movement buttons
	BStringView* moveLabel = new BStringView(BRect(xPos, yPos, xPos + 200, yPos + labelHeight),
		"move_label", "Window Movement Controls:", B_FOLLOW_LEFT | B_FOLLOW_TOP);
	AddChild(moveLabel);
	yPos += spacing + 5;
	
	// Compass-style move buttons (diamond arrangement)
	const int COMPASS_CX = xPos + 36;
	const int COMPASS_CY = yPos + 40;
	const int BTN_HALF = 12; // half-width/height for square buttons
	
	BButton* aMoveButton = new BButton(BRect(COMPASS_CX + 55, COMPASS_CY - 27, COMPASS_CX + 155, COMPASS_CY -3), 
		"Move Origin Button", "Move to Origin", new BMessage(MOVE_WINDOW));
	AddChild(aMoveButton);
	aMoveButton->SetToolTip("Click me to move the window to (0, 0)");

	BButton* expandButton = new BButton(BRect(COMPASS_CX + 175, COMPASS_CY - 27, COMPASS_CX + 275, COMPASS_CY -3), 
		"Expand Button", "Expand (+32)", new BMessage(EXPAND_WINDOW));
	AddChild(expandButton);
	expandButton->SetToolTip("Expand window size by 32 pixels");

	BButton* aCenterButton = new BButton(BRect(COMPASS_CX + 55, COMPASS_CY + 3, COMPASS_CX + 155, COMPASS_CY + 27), 
		"Center Button", "Center", new BMessage(CENTER_WINDOW));
	AddChild(aCenterButton);

	BButton* shrinkButton = new BButton(BRect(COMPASS_CX + 175, COMPASS_CY + 3, COMPASS_CX + 275, COMPASS_CY + 27), 
		"Shrink Button", "Shrink (-32)", new BMessage(SHRINK_WINDOW));
	AddChild(shrinkButton);
	shrinkButton->SetToolTip("Shrink window size by 32 pixels");

	BButton* btnLeft = new BButton(BRect(COMPASS_CX - 36, COMPASS_CY - BTN_HALF, COMPASS_CX - 12, COMPASS_CY + BTN_HALF), 
		"btn_left", "<", new BMessage(MOVE_LEFT), B_FOLLOW_LEFT | B_FOLLOW_TOP);
	BButton* btnUp = new BButton(BRect(COMPASS_CX - BTN_HALF, COMPASS_CY - 27, COMPASS_CX + BTN_HALF, COMPASS_CY - 3), 
		"btn_up", "^", new BMessage(MOVE_UP), B_FOLLOW_LEFT | B_FOLLOW_TOP);
	BButton* btnRight = new BButton(BRect(COMPASS_CX + 12, COMPASS_CY - BTN_HALF, COMPASS_CX + 36, COMPASS_CY + BTN_HALF), 
		"btn_right", ">", new BMessage(MOVE_RIGHT), B_FOLLOW_LEFT | B_FOLLOW_TOP);
	BButton* btnDown = new BButton(BRect(COMPASS_CX - BTN_HALF, COMPASS_CY + 3, COMPASS_CX + BTN_HALF, COMPASS_CY + 27), 
		"btn_down", "V", new BMessage(MOVE_DOWN), B_FOLLOW_LEFT | B_FOLLOW_TOP);
	
	AddChild(btnLeft);
	AddChild(btnUp);
	AddChild(btnRight);
	AddChild(btnDown);
	
	// Initialize mouse position
	fLastMousePos.Set(0, 0);
}


SystemInfoView::~SystemInfoView()
{
}


void
SystemInfoView::AttachedToWindow()
{
	BView::AttachedToWindow();

	_SyncThemeState();
	UpdateInfo();
}


void
SystemInfoView::Draw(BRect updateRect)
{
	BView::Draw(updateRect);
}


void
SystemInfoView::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case UPDATE_SYSINFO:
			UpdateInfo();
			break;


		default:
			BView::MessageReceived(message);
			break;
	}
}


void
SystemInfoView::_SyncThemeState()
{
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	Invalidate();
}


void
SystemInfoView::Pulse()
{
	// Update info on each pulse to catch manual window moves/resizes
	UpdateInfo();
}


void
SystemInfoView::MouseMoved(BPoint where, uint32 code, const BMessage* dragMessage)
{
	BView::MouseMoved(where, code, dragMessage);
	fLastMousePos = where;
}


void
SystemInfoView::UpdateInfo()
{
	if (!Window())
		return;
	
	BWindow* window = Window();
	const char* isFront = window->IsFront() ? "Focused" : "Not Focused";

	// Window position - use backend API to get actual position
	char posText[100];
	snprintf(posText, sizeof(posText), "Window Position: (%.0f x %.0f) - %s", window->Frame().left, window->Frame().top, isFront);
	fWindowPosLabel->SetText(posText);
	
	// Mouse position
	char mousePosText[100];
	snprintf(mousePosText, sizeof(mousePosText), "TabView Mouse Position: (%.0f, %.0f)", fLastMousePos.x, fLastMousePos.y);
	fMousePosLabel->SetText(mousePosText);
	
	// Window size
	BRect frame = window->Frame();
	char sizeText[100];
	snprintf(sizeText, sizeof(sizeText), "Window Size: %.0f x %.0f", frame.Width(), frame.Height());
	fWindowSizeLabel->SetText(sizeText);
	
	// Screen size
	BScreen screen(window);
	BRect screenFrame = screen.Frame();
	char screenText[100];
	snprintf(screenText, sizeof(screenText), "Screen Size: %.0f x %.0f", screenFrame.Width(), screenFrame.Height());
	fScreenSizeLabel->SetText(screenText);
	
	// Backend - get from backend API
#ifdef __HAIKU__
	const char* backendName = "Haiku";
#else
	const char* backendName = cosmoe_backend_get_current_name();
#endif
	char backendText[100];
	if (backendName != NULL) {
		snprintf(backendText, sizeof(backendText), "Backend: %s", backendName);
	} else {
		snprintf(backendText, sizeof(backendText), "Backend: Unknown");
	}
	fBackendLabel->SetText(backendText);
	
	// Scale percent
#ifdef __HAIKU__
	int32 scalePercent = 100;
#else
	int32 scalePercent = cosmoe_window_get_display_scale(be_app->Display(), window->WindowToken());
#endif
	char scaleText[50];
	snprintf(scaleText, sizeof(scaleText), "Backend Scale: %d%%", (int)scalePercent);
	fScaleLabel->SetText(scaleText);

	BString controlLookText;
	if (BPrivate::get_control_look(controlLookText) == false) {
		controlLookText.SetTo("Control Look: Built-in Haiku Control Look");
	} else {
		controlLookText.Prepend("Control Look: ");
	}
	fControlLookLabel->SetText(controlLookText);
}

//	#pragma mark - BitmapView

BitmapView::BitmapView(BRect rect, const char* name, uint32 followFlags)
	: BView ( rect, name, followFlags, B_WILL_DRAW)
{
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	mBitmap = BTranslationUtils::GetBitmap(B_PNG_FORMAT, "cosmoe-logo.png");
	if (mBitmap == NULL) {
		fprintf(stderr, "Failed to load cosmoe-logo.png\n");
		return;
	}
	
	// Initialize mouse tracking
	fMousePos.Set(-1000, -1000);  // Start offscreen
	fMouseInView = false;
	SetEventMask(B_POINTER_EVENTS, 0);
}

void BitmapView::Draw(BRect updateRect)
{
	if (mBitmap) {
		// Calculate center of the view
		BRect bounds = Bounds();
		BPoint center(bounds.Width() / 2, bounds.Height() / 2);
		
		// Calculate maximum distance from center to edge
		float maxDistance = sqrt(center.x * center.x + center.y * center.y);
		
		// Calculate opacity based on mouse distance from center
		float opacity = 255;  // Default to full opacity
		
		if (fMouseInView) {
			// Calculate distance from mouse to center
			float dx = fMousePos.x - center.x;
			float dy = fMousePos.y - center.y;
			float distance = sqrt(dx * dx + dy * dy);
			
			// Normalize distance (0 = center, 1 = edge or beyond)
			float normalizedDist = distance / maxDistance;
			if (normalizedDist > 1.0f) normalizedDist = 1.0f;
			
			// Map normalized distance to opacity (10% at center, 100% at edge)
			// opacity = 10% + (90% * normalizedDist)
			opacity = 25.5f + (229.5f * normalizedDist);  // 10% to 100% of 255
		}
		
		SetDrawingMode(B_OP_ALPHA);
		SetBlendingMode(B_CONSTANT_ALPHA, B_ALPHA_OVERLAY);
		SetHighColor(255, 255, 255, (uint8)opacity);
		DrawBitmap(mBitmap, BPoint(0, 0));
	}
}

void BitmapView::MouseDown(BPoint where)
{
	// Allow us to walk this view around and check clipping
	if (where.y < Bounds().Height() / 4)
		this->MoveBy(0, -10);
	else if (where.y > 3 * Bounds().Height() / 4)
		this->MoveBy(0, 10);
	
	if (where.x < Bounds().Width() / 4) {
		this->MoveBy(-10, 0);
	} else if (where.x > 3 * Bounds().Width() / 4) {
		this->MoveBy(10, 0);
	}
}


void BitmapView::MouseMoved(BPoint where, uint32 code, const BMessage* dragMessage)
{
	if (code == B_ENTERED_VIEW) {
		fMouseInView = true;
	} else if (code == B_EXITED_VIEW) {
		fMouseInView = false;
	}
	
	fMousePos = where;
	Invalidate();  // Redraw with updated opacity
}


BitmapView::~BitmapView()
{
	if (mBitmap) {
		delete mBitmap;
		mBitmap = NULL;
	}
}


SampleDataRow::SampleDataRow()
{
	static int32 count = 1;
	SetField(new BStringField("id1234"), 0);
	SetField(new BStringField("ABCD"), 1);
	SetField(new BStringField("Fnord"), 2);
	SetField(new BSizeField(count++ * 1024), 3);
}


BBitmap* RenderVectorCursor(uint32 size, const uint8* vector,
	uint32 vectorSize, float shadowStrength)
{
	const uint32 flags = B_BITMAP_NO_SERVER_LINK;

	// The cursor HVIFs need to be rendered at an equivalent of 32x32,
	// with everything outside the 22x22 area discarded.
	const int32 renderRectSize = (int32)(size * (32.0f / 22.0f));

	BBitmap renderCursor(BRect(0, 0, renderRectSize - 1, renderRectSize - 1), flags, B_RGBA32);
	status_t status = BIconUtils::GetVectorIcon(vector, vectorSize, &renderCursor);
	if (status != B_OK) {
		return NULL;
	}

	const BRect rect(0, 0, size - 1, size - 1);
	BBitmap cursor(rect, flags, B_RGBA32);
	cursor.ImportBits(&renderCursor, B_ORIGIN, B_ORIGIN, rect.Size());

	BBitmap shadow(rect, flags, B_RGBA32);
	memset(shadow.Bits(), 0, shadow.BitsLength());

	{
		int32 offset = size / 32;
		if (offset == 0)
			offset = 1; // <32px cursors

		shadow.ImportBits(&cursor, BPoint(0, 0), BPoint(offset, offset),
			BSize(size - offset - 1, size - offset - 1));

		agg::rendering_buffer buffer((unsigned char*)shadow.Bits(),
			size, size, shadow.BytesPerRow());
		agg::pixfmt_rgba32 pixFmt(buffer);

		agg::recursive_blur<agg::rgba8, agg::recursive_blur_calc_rgba<> > blur;
		blur.blur(pixFmt, 1);

		for (int32 i = 0; i < shadow.BitsLength(); i += 4) {
			uint8* bits = (uint8*)shadow.Bits() + i;
			bits[0] = 0;
			bits[1] = 0;
			bits[2] = 0;
			bits[3] = (uint8)(bits[3] * shadowStrength);
		}
	}

	BBitmap* composite = new BBitmap(rect, flags, B_RGBA32);

	uint8* s = (uint8*)shadow.Bits();
	uint8* c = (uint8*)cursor.Bits();
	uint8* d = (uint8*)composite->Bits();
	for (uint32 y = 0; y < size; y++) {
		for (uint32 x = 0; x < size; x++) {
			uint8 a = (uint8)(c[3] + (255 - c[3]) * (s[3] / 255.0));
			d[3] = a;
			for (int32 i = 0; i < 3; ++i) {
				d[i] = ((s[i] * (255 - c[3]) + 255) >> 8) + c[i];

				// premultiply
				d[i] = (uint8)(d[i] * int32(a) / 255.0);
			}
			s += 4;
			c += 4;
			d += 4;
		}
	}

	return composite;
}