/*This is diswindow.cpp*/

#include <Application.h>
#include "diswindow.h"
#include "disview.h"

#include <iostream>
#include <stdio.h>
#include <string>
#include <string.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <String.h>

#if defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
#include <Placeholder.h>
#endif

#include <Box.h>
#include <Button.h>
#include <MenuItem.h>
#include <CheckBox.h>
#include <RadioButton.h>
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

#if defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
// Haven't figured out how to use these on Haiku yet.  The includes are there,
// but the implementation is not in libbe or libtracker AFAICT
#include <DecimalSpinner.h>
#include <ChannelSlider.h>
#include <ColumnListView.h>
#include <ColumnTypes.h>
#endif

#include <IconUtils.h>
#include <ControlLook.h>
#include <TranslationUtils.h>
#include <TranslatorFormats.h>
#include <Bitmaps.h>
#include <BitmapButton.h>
#include <ScrollView.h>
#include <Gradient.h>
#include <GradientLinear.h>


const int CHECK_ONE = 'chk1';
const int CHECK_TWO = 'chk2';
const int RADIO_ONE = 'rad1';
const int RADIO_TWO = 'rad2';
const int SHOW_ALERT = 'SHWA';
const int SHOW_ALERT_ASYNC = 'SHAA';
const int SHOW_HIDE_VIEW = 'SHVi';
const int SHOW_FILE_PANEL = 'SHFP';
const int MOVE_WINDOW = 'MOVW';
const int MOVE_LEFT = 'MLFT';
const int MOVE_UP = 'MUP_';
const int MOVE_RIGHT = 'MRGT';
const int MOVE_DOWN = 'MDWN';


class BStringViewDebug : public BStringView
{
public:
	BStringViewDebug(BRect frame, const char* name, const char* text, uint32 resizingMode = B_FOLLOW_LEFT | B_FOLLOW_TOP, uint32 flags = B_WILL_DRAW)
		: BStringView(frame, name, text, resizingMode, flags)
	{
	}

	virtual void MouseDown(BPoint where)
	{
		printf("BStringViewDebug::MouseDown at (%.1f, %.1f), Bounds=(%f,%f,%f,%f)\n", 
			where.x, where.y, Bounds().left, Bounds().top, Bounds().right, Bounds().bottom);
		BMessage* msg = Window()->CurrentMessage();
		if (msg) {
			printf("Current message:\n");
			msg->PrintToStream();
		}
		BStringView::MouseDown(where);
	}
};

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
		virtual void		MouseMoved(BPoint where, uint32 code, const BMessage* dragMessage);

	private:
				std::vector<AppEntry> fApps;
				BPoint			fMousePos;
				bool			fMouseInView;
};

class BitmapView : public BView {
	public:
								BitmapView(BRect rect, const char* name, uint32 followFlags);
		virtual					~BitmapView();
	
		virtual void			Draw(BRect updateRect);
		virtual void			MouseDown(BPoint pos);
	
	private:
				BBitmap*		mBitmap;
};

#if defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
class SampleDataRow : public BRow
{
	public:
								SampleDataRow();
};
#endif



DisWindow::DisWindow(BRect aRect)
	: BWindow ( aRect, "Cosmoe Showcase", B_TITLED_WINDOW, /*B_NOT_V_RESIZABLE |*/ B_CLOSE_ON_ESCAPE),
	fFilePanel(new BFilePanel(B_OPEN_PANEL))
{
#if defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
	fIcon = new(std::nothrow) BBitmap(BRect(BPoint(0, 0), be_control_look->ComposeIconSize(32)), 0, B_RGBA32);
	BIconUtils::GetAppIcon("BEOS:ICON", B_LARGE_ICON, fIcon);
	if (fIcon == NULL) {
		fprintf(stderr, "Failed to load BEOS:ICON icon\n");
	}
#endif
}

bool DisWindow::QuitRequested()
{
	be_app->PostMessage(B_QUIT_REQUESTED);
	return (true);
}


void DisWindow::Populate()
{
	SetupMenus();

	#if 1
	BRect r;
	BTabView *tabView;
	BTab *tab;

	r = Bounds();
	r.top += mMenuBar->Bounds().Height();	// make room for the BMenuBar
	//r.InsetBy(5,5);

	tabView = new BTabView(r, "tab_view");
	
	Lock();
	AddChild(tabView);
	Unlock();
	
	//tabView->SetViewColor(216,216,216,0);

	r = tabView->Bounds();
	//r.InsetBy(1,1);
	r.bottom -= tabView->TabHeight();

	tab = new BTab();
	BView* launcherTabView = new BView(r, "Tab (Launcher)", B_FOLLOW_ALL, 0);
	tabView->AddTab(launcherTabView, tab);
	tab->SetLabel("Launcher");

	tab = new BTab();
	BView* controlsTabView = new BView(r, "Tab (Controls)", B_FOLLOW_ALL, 0);
	tabView->AddTab(controlsTabView, tab);
	tab->SetLabel("Controls");

	tab = new BTab();
	BView* guiElementsTabView = new BView(r, "Tab (GUI Elements)", B_FOLLOW_ALL, 0);
	tabView->AddTab(guiElementsTabView, tab);
	tab->SetLabel("GUI Elements");

	tab = new BTab();
	BView* testingTabView = new BView(r, "Tab (Testing)", B_FOLLOW_ALL, 0);
	tabView->AddTab(testingTabView, tab);
	tab->SetLabel("Draw Testing");

	// Add a box
	BBox* aBox1 = new BBox(BRect(15, 15, 200, 75), "Box 1 (Check Boxes)");
	aBox1->SetLabel("Check Boxes");
	BCheckBox* aCheckBox1 = new BCheckBox(BRect(10, 12, 160, 32), "check box 1", "Check Box 1", new BMessage(CHECK_ONE));
	BCheckBox* aCheckBox2 = new BCheckBox(BRect(10, 35, 160, 55), "check box 2", "Check Box 2", new BMessage(CHECK_TWO));
	aBox1->AddChild(aCheckBox1);
	aBox1->AddChild(aCheckBox2);
	controlsTabView->AddChild(aBox1);

	// Add another box
	BBox* aBox2 = new BBox(BRect(15, 95, 200, 155), "Box 2 (Radio Buttons)");
	aBox2->SetLabel("Radio Buttons");
	BRadioButton* aRadioBut1 = new BRadioButton(BRect(10, 12, 160, 32), "radio button 1", "Radio Button 1", new BMessage(RADIO_ONE));
	BRadioButton* aRadioBut2 = new BRadioButton(BRect(10, 35, 160, 55), "radio button 2", "Radio Button 2", new BMessage(RADIO_TWO));
	aRadioBut1->SetValue(B_CONTROL_ON);
	aBox2->AddChild(aRadioBut1);
	aBox2->AddChild(aRadioBut2);
	controlsTabView->AddChild(aBox2);

	// Add yet another box
	BBox* aBox3 = new BBox(BRect(15, 175, 200, 270), "Box 3 (Button)", B_FOLLOW_TOP_BOTTOM);
	BButton* aBoxButton = new BButton(BRect(0, 0, 72, 24), "a button", "Open...", new BMessage(SHOW_FILE_PANEL));
	BStringView* aStringView = new BStringView(BRect(10, 26, 155, 66), "string view", "A button as a box label");
	aBox3->AddChild(aStringView);
	aBox3->SetLabel(aBoxButton);
	controlsTabView->AddChild(aBox3);

	// Add a box for a scrollbar sample
	BBox* aBox4 = new BBox(BRect(210, 15, 380, 75), "Box 4 (Scrollbar)", B_FOLLOW_LEFT_RIGHT);
	BStringViewDebug* scrollString = new BStringViewDebug(BRect(10, 15, 155, 34), "scrolling string view", "Use the horizontal scrollbar below to scroll this string of text.", B_FOLLOW_LEFT_RIGHT);
	BScrollBar* horizScroll = new BScrollBar(BRect(10, 35, 155, 35 + B_H_SCROLL_BAR_HEIGHT), "horizontal scrollbar", scrollString, 0, 170, B_HORIZONTAL);
	//horizScroll->SetProportion( 0.5 );
	aBox4->AddChild(scrollString);
	aBox4->AddChild(horizScroll);
	aBox4->SetLabel("Horizontal ScrollBar");
	controlsTabView->AddChild(aBox4);

	// Add a button which brings up a BAlert
	BButton* anAlertButton = new BButton(BRect(210, 96, 320, 114), "Alert Button", "Alert (sync)", new BMessage(SHOW_ALERT));
	controlsTabView->AddChild(anAlertButton);
	anAlertButton->SetToolTip("Click me to show an alert");

	BButton* anAsyncAlertButton = new BButton(BRect(330, 96, 440, 114), "Alert Button 2", "Alert (async)", new BMessage(SHOW_ALERT_ASYNC));
	controlsTabView->AddChild(anAsyncAlertButton);
	anAsyncAlertButton->SetToolTip("Click me to show an alert asynchronously");

	// Compass-style move buttons (diamond arrangement) - small size
	const int COMPASS_CX = 486;
	const int COMPASS_CY = 108;
	const int BTN_HALF = 12; // half-width/height for square buttons

	BButton* aMoveButton = new BButton(BRect(COMPASS_CX - 50, 20, COMPASS_CX + 50, 38), "Move Button", "Move Window", new BMessage(MOVE_WINDOW));
	controlsTabView->AddChild(aMoveButton);
	aMoveButton->SetToolTip("Click me to move the window to the origin");

	BButton* btnLeft = new BButton(BRect(COMPASS_CX - 36, COMPASS_CY - BTN_HALF, COMPASS_CX - 12, COMPASS_CY + BTN_HALF), "btn_left", "<", new BMessage(MOVE_LEFT), B_FOLLOW_LEFT | B_FOLLOW_TOP);
	BButton* btnUp = new BButton(BRect(COMPASS_CX - BTN_HALF, COMPASS_CY - 27, COMPASS_CX + BTN_HALF, COMPASS_CY - 3), "btn_up", "^", new BMessage(MOVE_UP), B_FOLLOW_LEFT | B_FOLLOW_TOP);
	BButton* btnRight = new BButton(BRect(COMPASS_CX + 12, COMPASS_CY - BTN_HALF, COMPASS_CX + 36, COMPASS_CY + BTN_HALF), "btn_right", ">", new BMessage(MOVE_RIGHT), B_FOLLOW_LEFT | B_FOLLOW_TOP);
	BButton* btnDown = new BButton(BRect(COMPASS_CX - BTN_HALF, COMPASS_CY + 3, COMPASS_CX + BTN_HALF, COMPASS_CY + 27), "btn_down", "V", new BMessage(MOVE_DOWN), B_FOLLOW_LEFT | B_FOLLOW_TOP);

	controlsTabView->AddChild(btnLeft);
	controlsTabView->AddChild(btnUp);
	controlsTabView->AddChild(btnRight);
	controlsTabView->AddChild(btnDown);

	BTextControl* aTextControl = new BTextControl(BRect(210, 145, 380, 180), "a text control",
										 "Type here:",
										 "Some sample text", NULL, B_FOLLOW_LEFT_RIGHT);
	controlsTabView->AddChild(aTextControl);

	// BSlider demo
	BBox* aBox5 = new BBox(BRect(210, 190, 380, 240), "Box 5 (Slider)", B_FOLLOW_LEFT_RIGHT);
	BSlider* aSlider = new BSlider(BRect(10, 6, 160, 26), "slider", "Volume",
									new BMessage(B_PULSE), 0, 100, B_HORIZONTAL, B_BLOCK_THUMB, B_FOLLOW_LEFT_RIGHT);
	aBox5->AddChild(aSlider);
	controlsTabView->AddChild(aBox5);

	// Testing Tab content

	BitmapView* bitmapView = new BitmapView(BRect(10, 80, 140, 210), "bitmap view", B_FOLLOW_ALL);
	testingTabView->AddChild(bitmapView);

	
	mStatusBar = new BStatusBar(BRect(15, 15, 255, 75), "status bar", "Progress", "% Done");
	mStatusBar->SetTo(50.0);
	mStatusBar->SetResizingMode(B_FOLLOW_LEFT_RIGHT);
	guiElementsTabView->AddChild(mStatusBar);

#if defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
	BDecimalSpinner* spinner = new BDecimalSpinner(BRect(15, 85, 205, 109), "spinner", "Spinner", NULL);
	guiElementsTabView->AddChild(spinner);

#if 0
	BChannelSlider* channelSlider = new BChannelSlider(BRect(205, 75, 505, 110), "channel slider", "Channel Slider", NULL);
	guiElementsTabView->AddChild(channelSlider);
#endif

	r = BRect(15, 115, 505, 339);
	BColumnListView* listView = new BColumnListView(r, "gridview", B_FOLLOW_ALL, B_WILL_DRAW, B_FANCY_BORDER);
	guiElementsTabView->AddChild(listView);
	
	float width = be_plain_font->StringWidth("00000") + 20;
	listView->AddColumn(new BStringColumn("ID", width, width, 100, B_TRUNCATE_END), 0);
	
	listView->AddColumn(new BStringColumn("Type", width, width, 100, B_TRUNCATE_END), 1);
	listView->AddColumn(new BStringColumn("Name", 150, 50, 300, B_TRUNCATE_END), 2);
	listView->AddColumn(new BSizeColumn("Data", 150, 50, 300), 3);

	for (int32 i = 0; i < 25; i++)
		listView->AddRow(new SampleDataRow());

	BPlaceholder* place1 = new BPlaceholder(BRect(215, 15, 300, 55), "Placeholder 1", B_FOLLOW_NONE);
	BPlaceholder* place2 = new BPlaceholder(BRect(215, 57, 300, 107), "Placeholder 2", B_FOLLOW_NONE);
	BPlaceholder* place3 = new BPlaceholder(BRect(302, 15, 350, 55), "Placeholder 3", B_FOLLOW_NONE);
	BPlaceholder* place4 = new BPlaceholder(BRect(302, 57, 350, 107), "Placeholder 4", B_FOLLOW_ALL_SIDES);
	testingTabView->AddChild(place1);
	testingTabView->AddChild(place2);
	testingTabView->AddChild(place3);
	testingTabView->AddChild(place4);
#endif

	// Add our pixel-accurate draw testing view
	DisView* aDisView = new DisView(BRect(15, 15, 200, 61), "DisView");
	testingTabView->AddChild(aDisView);

#if defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
	BButton* ShowHideButton = new BButton(BRect(215, 127, 350, 141), "show-hide button", "Show / Hide View", new BMessage(SHOW_HIDE_VIEW));
	testingTabView->AddChild(ShowHideButton);

	// Move bitmap placeholders to the bottom of the Draw Testing tab
	BPlaceholder* placeA = new BPlaceholder(BRect(15, 210, 115, 310), "Bitmap Placeholder 1", B_FOLLOW_NONE);
	placeA->SetViewBitmap(fIcon, 4626U, B_TILE_BITMAP_X);
	testingTabView->AddChild(placeA);

	BPlaceholder* placeB = new BPlaceholder(BRect(120, 210, 220, 310), "Bitmap Placeholder 2", B_FOLLOW_NONE);
	placeB->SetViewBitmap(fIcon, 4626U, B_TILE_BITMAP_Y);
	testingTabView->AddChild(placeB);

	BPlaceholder* placeC = new BPlaceholder(BRect(225, 210, 580, 310), "Bitmap Placeholder 3", B_FOLLOW_LEFT_RIGHT);
	placeC->SetViewBitmap(fIcon, 4626U, B_TILE_BITMAP);
	testingTabView->AddChild(placeC);

	// Bitmap Tab content (icon grid remains here)
	// Fill the launcher tab with the icon view
	BRect iconViewRect = launcherTabView->Bounds();

	// This shouldn't be necessary, but the tab view have an issue with clipping (or not clipping)
	iconViewRect.right -= 5;
	iconViewRect.bottom -= 5;

	IconView* iconView = new IconView(iconViewRect, B_FOLLOW_ALL);
	launcherTabView->AddChild(iconView);
#endif

	#endif

	// Note: SetPulseRate is called in the app after Show()
}


void DisWindow::SetupMenus()
{
	BRect cMenuFrame = Bounds();
	cMenuFrame.bottom = 16;

	mMenuBar = new BMenuBar( cMenuFrame, "Menubar" );

	BMenu* fileMenu = new BMenu( "File" );
	fileMenu->AddItem(new BMenuItem("Quit", new BMessage(B_QUIT_REQUESTED), 'Q'));
	mMenuBar->AddItem( fileMenu );

	BMenu* editMenu = new BMenu( "Edit" );
	editMenu->AddItem(new BMenuItem("Undo", new BMessage( B_UNDO ), 'Z'));
	editMenu->AddSeparatorItem();
	editMenu->AddItem(new BMenuItem("Cut", new BMessage( B_CUT ), 'X'));
	editMenu->AddItem(new BMenuItem("Copy", new BMessage( B_COPY ), 'C'));
	editMenu->AddItem(new BMenuItem("Paste", new BMessage( B_PASTE ), 'V'));
	mMenuBar->AddItem( editMenu );

	mMenuBar->SetTargetForItems( this );

	Lock();
	AddChild(mMenuBar);
	Unlock();
}


void DisWindow::MessageReceived(BMessage* message)
{
	switch(message->what)
	{
		case CHECK_ONE:
			printf("Checkbox #1 clicked\n");
			{
				if (mStatusBar) {
					float value = mStatusBar->CurrentValue() + 1.0f;
					mStatusBar->SetTo(value);
					printf("value = %f\n", value);
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
					printf("File panel not initialized\n");
				}
			}
			break;

		case SHOW_ALERT:
			{
				BAlert* alert = new BAlert("Alert", "This is a sample alert.", "OK");

				if (alert) {
					alert->SetFlags(alert->Flags() | B_CLOSE_ON_ESCAPE);
					alert->Go(NULL);
				}
			}
			break;

		case SHOW_ALERT_ASYNC:
			{
				BAlert* alert = new BAlert("Async Alert", "This is a sample asynchronous alert.", "Red", "Blue", "Green");

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
			}
			break;

			case MOVE_LEFT:
			{
				MoveBy(-20, 0);
			}
			break;

			case MOVE_UP:
			{
				MoveBy(0, -20);
			}
			break;

			case MOVE_RIGHT:
			{
				MoveBy(20, 0);
			}
			break;

			case MOVE_DOWN:
			{
				MoveBy(0, 20);
			}
			break;

		case SHOW_HIDE_VIEW:
			{
				BView* view = FindView("Placeholder 4");
				if (view) {
					if (view->IsHidden()) {
						printf("Showing view\n");
						view->Show();
					} else {
						printf("Hiding view\n");
						view->Hide();
					}
				} else {
					printf("Warning: Couldn't find view to show/hide\n");
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
	const int32 iconSize = 96;

	auto load_app_icon = [&](AppEntry& entry) {
		status_t iconErr = B_BAD_VALUE;

		// Try vector icon from embedded resources first
		{
			BResources res;
			status_t resErr = res.SetTo(entry.path.c_str(), false);
			if (resErr == B_OK) {
				size_t size = 0;
				const void* data = res.LoadResource(B_VECTOR_ICON_TYPE, "BEOS:ICON", &size);

				if (data != NULL /* && size > 0*/) {
					printf("Loaded vector icon resource from '%s', size %zu bytes\n", entry.path.c_str(), size);
					iconErr = BIconUtils::GetVectorIcon(static_cast<const uint8*>(data), size, entry.icon);
				} else {
					printf("No vector icon resource in '%s'\n", entry.path.c_str());
				}
			} 
		}

		// Last resort: system icon so we still render something
		if (iconErr != B_OK) {
			iconErr = BIconUtils::GetSystemIcon("dialog-information", entry.icon);
		}

		if (iconErr != B_OK) {
			printf("Error loading app icon from '%s': %s\n", entry.path.c_str(), strerror(iconErr));
		}
	};

	auto add_app = [&](const char* fullPath,
	const char* description = "application/x-vnd.unknown") {
		BFile appFile(fullPath, B_READ_ONLY);
		if (appFile.InitCheck() != B_OK)
			return;

		BAppFileInfo appInfo(&appFile);
		if (appInfo.InitCheck() != B_OK)
			return;

		BPath path(fullPath);
		AppEntry entry;
		entry.name = path.Leaf();
		entry.path = fullPath;
		entry.description = description;
		entry.icon = new(std::nothrow) BBitmap(BRect(0, 0, iconSize - 1, iconSize - 1), 0, B_RGBA32);
		load_app_icon(entry);
		fApps.push_back(entry);
	};

	add_app("/usr/local/bin/DeskCalc", "Simple calculator application");
	add_app("/usr/local/bin/Pairs", "Matching game");
	add_app("/usr/local/bin/Terminal", "Terminal emulator application");
	add_app("/usr/local/bin/StyledEdit", "Text editor application");
	add_app("/usr/local/bin/Showcase", "This app");
	add_app("/usr/local/bin/Mandelbrot", "Fractal explorer");
	add_app("/usr/local/bin/ResEdit", "Resource editor application");
	add_app("/usr/local/bin/Sudoku", "Puzzle game");
	add_app("/usr/local/bin/ShowImage", "Image viewer application");
	add_app("/usr/local/bin/Pulse", "System monitor application");
	add_app("/usr/local/bin/Gradients", "Gradient viewer application");
	add_app("/usr/local/bin/FontDemo", "Font effects application");
	add_app("/usr/local/bin/Icon-O-Matic", "Vector icon editor");
	add_app("/usr/local/bin/AboutSystem", "System information");
	add_app("/usr/local/bin/Pulse", "The classic BeOS system monitor");

	// Initialize mouse tracking
	fMousePos.Set(-1000, -1000);  // Start offscreen
	fMouseInView = false;
	SetEventMask(B_POINTER_EVENTS, 0);
}


IconView::~IconView()
{
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

	SetDrawingMode(B_OP_OVER);

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
	int padding_h = 10;
	int padding_v = 108;
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

		if (fMouseInView && r.Contains(fMousePos)) {
			hoverLabel = fApps[i].name + "\n" + fApps[i].description;
		}

		DrawBitmap(fApps[i].icon, r);
		currentX += scaledWidth;
	}

	if (!hoverLabel.empty()) {
		BFont labelFont;
		GetFont(&labelFont);
		labelFont.SetSize(16);
		SetFont(&labelFont);
		SetHighColor(30, 30, 30);
		float labelWidth = labelFont.StringWidth(hoverLabel.c_str());
		float labelX = (Bounds().Width() - labelWidth) / 2.0f;
		float labelY = baseline + 24.0f;
		DrawString(hoverLabel.c_str(), BPoint(labelX, labelY));
	}

	SetDrawingMode(B_OP_COPY);
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

//	#pragma mark - BitmapView

BitmapView::BitmapView(BRect rect, const char* name, uint32 followFlags)
	: BView ( rect, name, followFlags, B_WILL_DRAW)
{
	mBitmap = BTranslationUtils::GetBitmap(B_PNG_FORMAT, "walter_logo.png");
	if (mBitmap == NULL) {
		fprintf(stderr, "Failed to load walter_logo.png\n");
		return;
	}
}

void BitmapView::Draw(BRect updateRect)
{
	if (mBitmap) {
		SetDrawingMode(B_OP_OVER);
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

	Invalidate();
}


BitmapView::~BitmapView()
{
	if (mBitmap) {
		delete mBitmap;
		mBitmap = NULL;
	}
}


#if defined(__linux__) || defined(__APPLE__) || defined(_WIN32)

SampleDataRow::SampleDataRow()
{
	static int32 count = 1;
	SetField(new BStringField("id1234"), 0);
	SetField(new BStringField("ABCD"), 1);
	SetField(new BStringField("Fnord"), 2);
	SetField(new BSizeField(count++ * 1024), 3);
}

#endif