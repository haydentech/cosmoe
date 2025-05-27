/*This is diswindow.cpp*/

#include <Application.h>
#include "diswindow.h"
#include "disview.h"

#include <iostream>
#include <stdio.h>
#include <String.h>

#include <Placeholder.h>

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
#include <DecimalSpinner.h>
#include <ChannelSlider.h>
#include <ColumnListView.h>
#include <ColumnTypes.h>

#include <IconUtils.h>
#include <ControlLook.h>
#include <TranslationUtils.h>
#include <TranslatorFormats.h>

const int CHECK_ONE = 'chk1';
const int CHECK_TWO = 'chk2';
const int RADIO_ONE = 'rad1';
const int RADIO_TWO = 'rad2';
const int SHOW_ALERT = 'SHWA';
const int SHOW_HIDE_VIEW = 'SHVi';

class IconView : public BView {
	public:
								IconView(BRect rect, uint32 followFlags);
		virtual					~IconView();
	
		virtual void			Draw(BRect updateRect);
	
	private:
				BBitmap*		fIcons[4];
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

class SampleDataRow : public BRow
{
public:
					SampleDataRow();
};



DisWindow::DisWindow(BRect aRect)
	: BWindow ( aRect, "Guido - Test the Cosmoe GUI", B_TITLED_WINDOW, /*B_NOT_V_RESIZABLE |*/ B_CLOSE_ON_ESCAPE)
{
	fIcon = new(std::nothrow) BBitmap(BRect(BPoint(0, 0), be_control_look->ComposeIconSize(32)), 0, B_RGBA32);
	BIconUtils::GetAppIcon("BEOS:ICON", B_LARGE_ICON, fIcon);
	if (fIcon == NULL) {
		fprintf(stderr, "Failed to load icon\n");
	}
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
	
	tabView->SetViewColor(216,216,216,0);

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
	BCheckBox* aCheckBox1 = new BCheckBox(BRect(10, 12, 160, 32), "a check box", "Check Box 1", new BMessage(CHECK_ONE));
	BCheckBox* aCheckBox2 = new BCheckBox(BRect(10, 35, 160, 55), "a check box", "Check Box 2", new BMessage(CHECK_TWO));
	aBox1->AddChild(aCheckBox1);
	aBox1->AddChild(aCheckBox2);
	controlsTabView->AddChild(aBox1);

	// Add another box
	BBox* aBox2 = new BBox(BRect(15, 95, 200, 155), "Box 2 (Radio Buttons)");
	aBox2->SetLabel("Radio Buttons");
	BRadioButton* aRadioBut1 = new BRadioButton(BRect(10, 12, 160, 32), "a radio button", "Radio Button 1", new BMessage(RADIO_ONE));
	BRadioButton* aRadioBut2 = new BRadioButton(BRect(10, 35, 160, 55), "a radio button", "Radio Button 2", new BMessage(RADIO_TWO));
	aRadioBut1->SetValue(B_CONTROL_ON);
	aBox2->AddChild(aRadioBut1);
	aBox2->AddChild(aRadioBut2);
	controlsTabView->AddChild(aBox2);

	// Add yet another box
	BBox* aBox3 = new BBox(BRect(15, 175, 200, 270), "Box 3 (Button)", B_FOLLOW_TOP_BOTTOM);
	BButton* aBoxButton = new BButton(BRect(0, 0, 72, 24), "a button", "Button", new BMessage(B_PULSE));
	BStringView* aStringView = new BStringView(BRect(10, 26, 155, 66), "string view", "A button as a box label");
	aBox3->AddChild(aStringView);
	aBox3->SetLabel(aBoxButton);
	controlsTabView->AddChild(aBox3);

	// Add a box for a scrollbar sample
	BBox* aBox4 = new BBox(BRect(210, 15, 380, 75), "Box 4 (Scrollbar)", B_FOLLOW_LEFT_RIGHT);
	BStringView* scrollString = new BStringView(BRect(10, 15, 155, 34), "scrolling string view", "Use the horizontal scrollbar below to scroll this string of text.", B_FOLLOW_LEFT_RIGHT);
	BScrollBar* horizScroll = new BScrollBar(BRect(10, 35, 155, 35 + B_H_SCROLL_BAR_HEIGHT), "horizontal scrollbar", scrollString, 0, 170, B_HORIZONTAL);
	//horizScroll->SetProportion( 0.5 );
	aBox4->AddChild(scrollString);
	aBox4->AddChild(horizScroll);
	aBox4->SetLabel("Horizontal ScrollBar");
	controlsTabView->AddChild(aBox4);

	// Add a button which brings up a BAlert
	BButton* anAlertButton = new BButton(BRect(225, 90, 355, 110), "Button 4", "Show Alert", new BMessage(SHOW_ALERT), B_FOLLOW_LEFT_RIGHT);
	controlsTabView->AddChild(anAlertButton);
	anAlertButton->SetToolTip("Click me to show an alert");

	BTextControl* aTextControl = new BTextControl(BRect(210, 135, 380, 170), "a text control",
										 "Type here:",
										 "Some sample text", NULL, B_FOLLOW_LEFT_RIGHT);
	controlsTabView->AddChild(aTextControl);

	// BSlider demo
	BBox* aBox5 = new BBox(BRect(210, 180, 380, 230), "Box 5 (Slider)", B_FOLLOW_LEFT_RIGHT);
	BSlider* aSlider = new BSlider(BRect(10, 6, 160, 26), "slider", "Volume",
									new BMessage(B_PULSE), 0, 100, B_HORIZONTAL, B_BLOCK_THUMB, B_FOLLOW_LEFT_RIGHT);
	aBox5->AddChild(aSlider);
	controlsTabView->AddChild(aBox5);

	IconView* iconView = new IconView(BRect(210, 250, 380, 302), B_FOLLOW_ALL);
	controlsTabView->AddChild(iconView);

	BitmapView* bitmapView = new BitmapView(BRect(210, 210, 340, 340), "bitmap view", B_FOLLOW_ALL);
	testingTabView->AddChild(bitmapView);

	
	mStatusBar = new BStatusBar(BRect(15, 15, 255, 75), "status bar", "Progress", "% Done");
	mStatusBar->SetTo(50.0);
	mStatusBar->SetResizingMode(B_FOLLOW_LEFT_RIGHT);
	guiElementsTabView->AddChild(mStatusBar);

	BDecimalSpinner* spinner = new BDecimalSpinner(BRect(15, 85, 205, 109), "spinner", "Spinner", NULL);
	guiElementsTabView->AddChild(spinner);

	// BChannelSlider* channelSlider = new BChannelSlider(BRect(15, 115, 205, 139), "channel slider", "Channel Slider", NULL);
	// guiElementsTabView->AddChild(channelSlider);

	r = BRect(15, 115, 505, 339);
	BColumnListView* listView = new BColumnListView(r, "gridview", B_FOLLOW_ALL, B_WILL_DRAW, B_FANCY_BORDER);
	guiElementsTabView->AddChild(listView);
	
	float width = be_plain_font->StringWidth("00000") + 20;
	listView->AddColumn(new BStringColumn("ID", width, width, 100, B_TRUNCATE_END), 0);
	
	listView->AddColumn(new BStringColumn("Type", width, width, 100, B_TRUNCATE_END), 1);
	listView->AddColumn(new BStringColumn("Name", 150, 50, 300, B_TRUNCATE_END), 2);
	listView->AddColumn(new BStringColumn("Data", 150, 50, 300, B_NO_TRUNCATION), 3);

	listView->AddRow(new SampleDataRow());

	BPlaceholder* place1 = new BPlaceholder(BRect(215, 15, 300, 55), "Placeholder 1", B_FOLLOW_NONE);
	BPlaceholder* place2 = new BPlaceholder(BRect(215, 57, 300, 107), "Placeholder 2", B_FOLLOW_NONE);
	BPlaceholder* place3 = new BPlaceholder(BRect(302, 15, 350, 55), "Placeholder 3", B_FOLLOW_NONE);
	BPlaceholder* place4 = new BPlaceholder(BRect(302, 57, 350, 107), "Placeholder 4", B_FOLLOW_ALL_SIDES);
	testingTabView->AddChild(place1);
	testingTabView->AddChild(place2);
	testingTabView->AddChild(place3);
	testingTabView->AddChild(place4);

	DisView* aDisView = new DisView(BRect(15, 15, 200, 61), "DisView");
	testingTabView->AddChild(aDisView);

	BButton* ShowHideButton = new BButton(BRect(215, 127, 350, 141), "show-hide button", "Show / Hide View", new BMessage(SHOW_HIDE_VIEW));
	testingTabView->AddChild(ShowHideButton);

	BPlaceholder* placeA = new BPlaceholder(BRect(15, 15, 115, 115), "1", B_FOLLOW_NONE);
	placeA->SetViewBitmap(fIcon, 4626U, B_TILE_BITMAP_X);
	bitmapTabView->AddChild(placeA);

	BPlaceholder* placeB = new BPlaceholder(BRect(120, 15, 220, 115), "1", B_FOLLOW_NONE);
	placeB->SetViewBitmap(fIcon, 4626U, B_TILE_BITMAP_Y);
	bitmapTabView->AddChild(placeB);

	BPlaceholder* placeC = new BPlaceholder(BRect(225, 15, 325, 115), "1", B_FOLLOW_NONE);
	placeC->SetViewBitmap(fIcon, 4626U, B_TILE_BITMAP);
	bitmapTabView->AddChild(placeC);

	#endif
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

		case SHOW_ALERT:
			{
				BAlert* alert = new BAlert("Alert", "This is a sample alert.", "OK");

				if (alert) {
					alert->SetFlags(alert->Flags() | B_CLOSE_ON_ESCAPE);
					alert->Go(NULL);
				}
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

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


//	#pragma mark - IconView



IconView::IconView(BRect rect, uint32 followFlags)
	:
	BView(rect, "logo", followFlags, B_WILL_DRAW)
{
	// Allocate the icon bitmap
	for (int i = 0; i < 4; i++) {
		fIcons[i] = new(std::nothrow) BBitmap(BRect(BPoint(0, 0), be_control_look->ComposeIconSize(32)), 0, B_RGBA32);
	}

	// Load the raw icon data
	BIconUtils::GetSystemIcon("dialog-information", fIcons[0]);
	BIconUtils::GetSystemIcon("dialog-idea", fIcons[1]);
	BIconUtils::GetSystemIcon("dialog-warning", fIcons[2]);
	BIconUtils::GetSystemIcon("dialog-error", fIcons[3]);
}


IconView::~IconView()
{
	for (int i = 0; i < 4; i++) {
		delete fIcons[i];
	}
}



void
IconView::Draw(BRect updateRect)
{
	for (int i = 0; i < 4; i++) {
		if (fIcons[i] == NULL)
			return;
	}

	BRect bounds(Bounds());
	SetLowColor(185, 185, 185);
	FillRect(bounds, B_SOLID_LOW);

	SetDrawingMode(B_OP_OVER);

	// Draw the first 3 icons normally
	for (int i = 0; i < 3; i++) {
		DrawBitmap(fIcons[i], BPoint(10 + (34.0 * i), 10));
	}

	// Stretch this last one out dynamically to test the scaling of DrawBitmap
	DrawBitmap(fIcons[3], BRect(112, 10, this->Bounds().Width() - 10, this->Bounds().Height() - 10));

	SetDrawingMode(B_OP_COPY);
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




SampleDataRow::SampleDataRow()
{
	SetField(new BStringField("id1234"), 0);
	SetField(new BStringField("ABCD"), 1);
	SetField(new BStringField("Fnord"), 2);
	SetField(new BSizeField(31337), 4);
}