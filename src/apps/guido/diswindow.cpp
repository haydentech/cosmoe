/*This is diswindow.cpp*/

#include <Application.h>
#include "diswindow.h"
#include "disview.h"

#include <iostream>
#include <stdio.h>
#include <cmath>
#include <String.h>

#if defined(__linux__) || defined(__APPLE__)
#include <Placeholder.h>
#endif

#include <Box.h>
#include <Button.h>
#include <MenuItem.h>
#include <CheckBox.h>
#include <RadioButton.h>
#include <StringView.h>
#include <TextControl.h>
#include <Message.h>
#include <MessageRunner.h>
#include <Slider.h>
#include <TabView.h>
#include <ScrollBar.h>
#include <Alert.h>

#if defined(__linux__) || defined(__APPLE__)
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

class IconView : public BView {
	public:
								IconView(BRect rect, uint32 followFlags);
		virtual					~IconView();
	
		virtual void			Draw(BRect updateRect);
		virtual void			MouseMoved(BPoint where, uint32 code, const BMessage* dragMessage);
	
	private:
				static const int32		fIconCount = 15;
				BBitmap*		fIcons[fIconCount];
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

#if defined(__linux__) || defined(__APPLE__)
class SampleDataRow : public BRow
{
	public:
								SampleDataRow();
};
#endif



DisWindow::DisWindow(BRect aRect)
	: BWindow ( aRect, "Guido - Test the Cosmoe GUI", B_TITLED_WINDOW, /*B_NOT_V_RESIZABLE |*/ B_CLOSE_ON_ESCAPE),
	fFilePanel(new BFilePanel(B_OPEN_PANEL))
{
#if defined(__linux__) || defined(__APPLE__)
	fIcon = new(std::nothrow) BBitmap(BRect(BPoint(0, 0), be_control_look->ComposeIconSize(32)), 0, B_RGBA32);
	BIconUtils::GetAppIcon("BEOS:ICON", B_LARGE_ICON, fIcon);
	if (fIcon == NULL) {
		fprintf(stderr, "Failed to load icon\n");
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

	tab = new BTab();
	BView* bitmapTabView = new BView(r, "Tab (Bitmaps)", B_FOLLOW_ALL, 0);
	tabView->AddTab(bitmapTabView, tab);
	tab->SetLabel("Bitmaps");

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

	BitmapView* bitmapView = new BitmapView(BRect(210, 210, 340, 340), "bitmap view", B_FOLLOW_ALL);
	testingTabView->AddChild(bitmapView);

	
	mStatusBar = new BStatusBar(BRect(15, 15, 255, 75), "status bar", "Progress", "% Done");
	mStatusBar->SetTo(50.0);
	mStatusBar->SetResizingMode(B_FOLLOW_LEFT_RIGHT);
	guiElementsTabView->AddChild(mStatusBar);

#if defined(__linux__) || defined(__APPLE__)
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

#if defined(__linux__) || defined(__APPLE__)
	BButton* ShowHideButton = new BButton(BRect(215, 127, 350, 141), "show-hide button", "Show / Hide View", new BMessage(SHOW_HIDE_VIEW));
	testingTabView->AddChild(ShowHideButton);

	// Bitmap Tab content

	BPlaceholder* placeA = new BPlaceholder(BRect(15, 15, 115, 115), "1", B_FOLLOW_NONE);
	placeA->SetViewBitmap(fIcon, 4626U, B_TILE_BITMAP_X);
	bitmapTabView->AddChild(placeA);

	BPlaceholder* placeB = new BPlaceholder(BRect(120, 15, 220, 115), "1", B_FOLLOW_NONE);
	placeB->SetViewBitmap(fIcon, 4626U, B_TILE_BITMAP_Y);
	bitmapTabView->AddChild(placeB);

	BPlaceholder* placeC = new BPlaceholder(BRect(225, 15, 580, 115), "1", B_FOLLOW_LEFT_RIGHT);
	placeC->SetViewBitmap(fIcon, 4626U, B_TILE_BITMAP);
	bitmapTabView->AddChild(placeC);

	IconView* iconView = new IconView(BRect(15, 150, 580, 302), B_FOLLOW_ALL);
	bitmapTabView->AddChild(iconView);
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
	// Allocate the icon bitmap - using 96x96 for retina quality
	// GetSystemIcon/GetIconResource will scale the vector icon to the bitmap size
	const int32 iconSize = 96;
	for (int i = 0; i < fIconCount; i++) {
		fIcons[i] = new(std::nothrow) BBitmap(BRect(0, 0, iconSize - 1, iconSize - 1), 0, B_RGBA32);
	}

	int index = 0;
	status_t err;

	// Load system icons from libbe - they're vector-based, so will scale to our bitmap size
	err = BIconUtils::GetSystemIcon("dialog-information", fIcons[index++]);
	if (err != B_OK) {
		printf("Error loading system icon 'dialog-information': %d\n", err);
	}
	err = BIconUtils::GetSystemIcon("dialog-idea", fIcons[index++]);
	if (err != B_OK) {
		printf("Error loading system icon 'dialog-idea': %d\n", err);
	}
	err = BIconUtils::GetSystemIcon("dialog-warning", fIcons[index++]);
	if (err != B_OK) {
		printf("Error loading system icon 'dialog-warning': %d\n", err);
	}
	err = BIconUtils::GetSystemIcon("dialog-error", fIcons[index++]);
	if (err != B_OK) {
		printf("Error loading system icon 'dialog-error': %d\n", err);
	}

	// Load some icons from libtracker
	err = GetTrackerResources()->GetIconResource(R_HardDiskIcon, B_LARGE_ICON, fIcons[index++]);
	if (err != B_OK) {
		printf("Error loading tracker icon R_HardDiskIcon: %d\n", err);
	}
	GetTrackerResources()->GetIconResource(R_AppIcon, B_LARGE_ICON, fIcons[index++]);
	GetTrackerResources()->GetIconResource(R_RootIcon, B_LARGE_ICON, fIcons[index++]);
	GetTrackerResources()->GetIconResource(R_BeosFolderIcon, B_LARGE_ICON, fIcons[index++]);
	GetTrackerResources()->GetIconResource(R_ResBackNav, B_LARGE_ICON, fIcons[index++]);
	GetTrackerResources()->GetIconResource(R_ResUpNav, B_LARGE_ICON, fIcons[index++]);
	GetTrackerResources()->GetIconResource(R_ResForwNav, B_LARGE_ICON, fIcons[index++]);
	GetTrackerResources()->GetIconResource(R_DownloadDirIcon, B_LARGE_ICON, fIcons[index++]);
	GetTrackerResources()->GetIconResource(R_QueryDirIcon, B_LARGE_ICON, fIcons[index++]);
	GetTrackerResources()->GetIconResource(R_CopyStatusIcon, B_LARGE_ICON, fIcons[index++]);
	GetTrackerResources()->GetIconResource(R_FileIcon, B_LARGE_ICON, fIcons[index++]);
	
	// Initialize mouse tracking
	fMousePos.Set(-1000, -1000);  // Start offscreen
	fMouseInView = false;
	SetEventMask(B_POINTER_EVENTS, 0);
}


IconView::~IconView()
{
	for (int i = 0; i < fIconCount; i++) {
		if (fIcons[i] != NULL)
			delete fIcons[i];
	}
}


void
IconView::Draw(BRect updateRect)
{
	for (int i = 0; i < fIconCount; i++) {
		if (fIcons[i] == NULL)
			return;
	}

	BRect bounds(Bounds());
	
	// Create a linear gradient from light gray at top to sky blue at bottom
	BGradientLinear gradient;
	gradient.SetStart(BPoint(0, bounds.top));
	gradient.SetEnd(BPoint(0, bounds.bottom));
	gradient.AddColor(rgb_color{185, 185, 185, 255}, 0.0f);    // Light gray at top
	gradient.AddColor(rgb_color{135, 206, 235, 255}, 255.0f);  // Sky blue at bottom
	
	FillRect(bounds, gradient);

	SetDrawingMode(B_OP_OVER);

	// Draw centered text at the top
	BFont font;
	GetFont(&font);
	font.SetFace(B_BOLD_FACE);
	SetFont(&font);
	
	const char* text = "Hover over these icons";
	float textWidth = font.StringWidth(text);
	float textX = (bounds.Width() - textWidth) / 2;
	float textY = 20;  // Position from top
	
	SetHighColor(50, 50, 50);  // Dark gray for good contrast
	DrawString(text, BPoint(textX, textY));

	// Draw a row of icons with magnification based on mouse proximity
	int padding_h = 10;
	int padding_v = 88;
	float baseWidth = (Bounds().Width() - padding_h) / fIconCount;
	const float maxScale = 3.0f;  // Maximum 3x magnification
	const float influenceRadius = baseWidth * 2.5f;  // Distance of influence
	
	for (int i = 0; i < fIconCount; i++) {
		if (fIcons[i] != NULL) {
			// Calculate base position and size
			float iconCenterX = padding_h + (baseWidth * i) + baseWidth / 2;
			float iconCenterY = padding_v + baseWidth / 2;
			
			// Calculate distance from mouse to icon center
			float scale = 1.0f;
			if (fMouseInView) {
				float dx = fMousePos.x - iconCenterX;
				float dy = fMousePos.y - iconCenterY;
				float distance = sqrt(dx * dx + dy * dy);
				
				// Apply smooth magnification based on distance
				if (distance < influenceRadius) {
					// Use smooth falloff: scale from maxScale at center to 1.0 at radius
					float normalizedDist = distance / influenceRadius;
					scale = 1.0f + (maxScale - 1.0f) * (1.0f - normalizedDist);
				}
			}
			
			// Calculate scaled icon size
			float scaledWidth = baseWidth * scale;
			float scaledHeight = baseWidth * scale;
			
			// Anchor icons at baseline (bottom), grow upward
			float baseline = padding_v + baseWidth;  // Bottom edge of base icon
			BRect r(
				iconCenterX - scaledWidth / 2,
				baseline - scaledHeight,
				iconCenterX + scaledWidth / 2,
				baseline
			);
			
			DrawBitmap(fIcons[i], r);
		}
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


#if defined(__linux__) || defined(__APPLE__)

SampleDataRow::SampleDataRow()
{
	static int32 count = 1;
	SetField(new BStringField("id1234"), 0);
	SetField(new BStringField("ABCD"), 1);
	SetField(new BStringField("Fnord"), 2);
	SetField(new BSizeField(count++ * 1024), 3);
}

#endif