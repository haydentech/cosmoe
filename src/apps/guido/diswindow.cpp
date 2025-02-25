/*This is diswindow.cpp*/

#include <Application.h>
#include "diswindow.h"

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
#include <StatusBar.h>
#include <Message.h>
#include <MessageRunner.h>
#include <Slider.h>
#include <TabView.h>
#include <ScrollBar.h>
#include <Alert.h>

const int CHECK_ONE = 'chk1';
const int CHECK_TWO = 'chk2';
const int RADIO_ONE = 'rad1';
const int RADIO_TWO = 'rad2';
const int SHOW_ALERT = 'SHWA';

DisWindow::DisWindow(BRect aRect)
	: BWindow ( aRect,"Guido - Test the Cosmoe GUI", B_TITLED_WINDOW, B_NOT_V_RESIZABLE)
{
}

bool DisWindow::QuitRequested()
{
	be_app->PostMessage(B_QUIT_REQUESTED);
	return (true);
}


void DisWindow::Populate()
{
	SetupMenus();

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
	/*tab = new BTab();
	BView* blankView = new BView(r, "Blank", B_FOLLOW_ALL, 0);
	tabView->AddTab(blankView, tab);
	tab->SetLabel("Blank");*/
	tab = new BTab();
	BView* constView = new BView(r, "Tab (Controls)", B_FOLLOW_ALL, 0);
	tabView->AddTab(constView, tab);
	tab->SetLabel("Controls");
	tab = new BTab();
	BView*destView = new BView(r, "Tab (GUI Elements)", B_FOLLOW_ALL, 0);
	tabView->AddTab(destView, tab);
	tab->SetLabel("GUI Elements");
	// Add a box
	BBox* aBox1 = new BBox(BRect(15, 15, 200, 75), "Box 1 (Check Boxes)");
	aBox1->SetLabel("Check Boxes");
	BCheckBox* aCheckBox1 = new BCheckBox(BRect(10, 12, 130, 32), "a check box", "Check Box 1", new BMessage(CHECK_ONE));
	BCheckBox* aCheckBox2 = new BCheckBox(BRect(10, 35, 130, 55), "a check box", "Check Box 2", new BMessage(CHECK_TWO));
	aBox1->AddChild(aCheckBox1);
	aBox1->AddChild(aCheckBox2);
	constView->AddChild(aBox1);

	// Add another box
	BBox* aBox2 = new BBox(BRect(15, 95, 200, 155), "Box 2 (Radio Buttons)");
	aBox2->SetLabel("Radio Buttons");
	BRadioButton* aRadioBut1 = new BRadioButton(BRect(10, 12, 130, 32), "a radio button", "Radio Button 1", new BMessage(RADIO_ONE));
	BRadioButton* aRadioBut2 = new BRadioButton(BRect(10, 35, 130, 55), "a radio button", "Radio Button 2", new BMessage(RADIO_TWO));
	aRadioBut1->SetValue(B_CONTROL_ON);
	aBox2->AddChild(aRadioBut1);
	aBox2->AddChild(aRadioBut2);
	constView->AddChild(aBox2);

	// Add yet another box
	BBox* aBox3 = new BBox(BRect(15, 175, 200, 270), "Box 3 (Button)", B_FOLLOW_TOP_BOTTOM);
	BButton* aBoxButton = new BButton(BRect(0, 0, 50, 24), "a button", "Button", new BMessage(B_PULSE));
	BStringView* aStringView = new BStringView(BRect(10, 26, 155, 66), "string view", "A button as a box label");
	aBox3->AddChild(aStringView);
	aBox3->SetLabel(aBoxButton);
	constView->AddChild(aBox3);

	// Add a box for a scrollbar sample
	BBox* aBox4 = new BBox(BRect(210, 15, 380, 75), "Box 4 (Scrollbar)", B_FOLLOW_LEFT_RIGHT);
	BStringView* scrollString = new BStringView(BRect(10, 15, 155, 34), "scrolling string view", "Use the horizontal scrollbar below to scroll this string of text.", B_FOLLOW_LEFT_RIGHT);
	BScrollBar* horizScroll = new BScrollBar(BRect(10, 35, 155, 35 + B_H_SCROLL_BAR_HEIGHT), "horizontal scrollbar", scrollString, 0, 170, B_HORIZONTAL);
	//horizScroll->SetProportion( 0.5 );
	aBox4->AddChild(scrollString);
	aBox4->AddChild(horizScroll);
	aBox4->SetLabel("Horizontal ScrollBar");
	constView->AddChild(aBox4);

	// Add a button which brings up a BAlert
	BButton* anAlertButton = new BButton(BRect(225, 90, 355, 110), "Button 4", "Show Alert", new BMessage(SHOW_ALERT), B_FOLLOW_LEFT_RIGHT);
	constView->AddChild(anAlertButton);
	anAlertButton->SetToolTip("Click me to show an alert");

	BTextControl* aTextControl = new BTextControl(BRect(210, 135, 380, 170), "a text control",
										 "Type here:",
										 "Some sample text", new BMessage(B_PULSE));
	constView->AddChild(aTextControl);

	// BSlider demo
	BBox* aBox5 = new BBox(BRect(210, 180, 380, 230), "Box 5 (Slider)", B_FOLLOW_ALL);
	BSlider* aSlider = new BSlider(BRect(10, 6, 160, 26), "a button", "Volume",
									new BMessage(B_PULSE), 0, 100, B_HORIZONTAL, B_BLOCK_THUMB, B_FOLLOW_LEFT_RIGHT);
	aBox5->AddChild(aSlider);
	constView->AddChild(aBox5);

	
	BStatusBar* aStatusBar = new BStatusBar(BRect(15, 15, 255, 75), "status bar", "Progress", "% Done");
	destView->AddChild(aStatusBar);

	//BPlaceholder* place1 = new BPlaceholder(BRect(15, 15, 200, 75), "Box 1", B_FOLLOW_TOP_BOTTOM | B_FOLLOW_RIGHT);
	//BPlaceholder* place2 = new BPlaceholder(BRect(15, 15, 35, 35), "Box 2", B_FOLLOW_BOTTOM);
	//AddChild(place1);
	//place1->AddChild(place2);
	//this->FindView("fTopView")->_PrintTree();

	SetupMenus();
}


void DisWindow::SetupMenus()
{
	BRect cMenuFrame = Bounds();
	cMenuFrame.bottom = 16;

	mMenuBar = new BMenuBar( cMenuFrame, "Menubar" );

	BMenu* fileMenu = new BMenu( "File" );
	fileMenu->AddItem(new BMenuItem("Quit", new BMessage(B_QUIT_REQUESTED)));
	mMenuBar->AddItem( fileMenu );

	BMenu* editMenu = new BMenu( "Edit" );
	editMenu->AddItem(new BMenuItem("Undo", new BMessage( B_UNDO )));
	editMenu->AddSeparatorItem();
	editMenu->AddItem(new BMenuItem("Cut", new BMessage( B_CUT )));
	editMenu->AddItem(new BMenuItem("Copy", new BMessage( B_COPY )));
	editMenu->AddItem(new BMenuItem("Paste", new BMessage( B_PASTE )));
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
			BWindow::MessageReceived(message);
			break;

		case CHECK_TWO:
			printf("Checkbox #2 clicked\n");
			BWindow::MessageReceived(message);
			break;

		case SHOW_ALERT:
			{
				printf("**************Showing an alert\n"); 

				BAlert* anAlert = new BAlert("Alert", "This is a sample alert.", "OK");

				if (anAlert)
					anAlert->Go(NULL);
			}
			break;

		default:
			BWindow::MessageReceived(message);
			break;
	}
}
