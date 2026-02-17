/*This is diswindow.cpp*/

#include <Application.h>
#include "diswindow.h"

#include <iostream>
#include <stdio.h>
#include <string.h>

#if defined(__linux__) || defined(__APPLE__)
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif


#include <StringView.h>
#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <Path.h>
#include <Resources.h>

#include <ColumnListView.h>
#include <ColumnTypes.h>

#include <Screen.h>


const int UPDATE_SYSINFO = 'UPSI';




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
				BStringView*	fBoundsLabel;
				BStringView*	fFrameLabel;
				BStringView*	fRowHeightLabel;
};


class SampleDataRow : public BRow
{
	public:
								SampleDataRow();
};



DisWindow::DisWindow(BRect aRect)
	: BWindow ( aRect, "Cosmoe Columns", B_TITLED_WINDOW, /*B_NOT_V_RESIZABLE |*/ B_CLOSE_ON_ESCAPE)
{
}

bool DisWindow::QuitRequested()
{
	be_app->PostMessage(B_QUIT_REQUESTED);
	return (true);
}


void DisWindow::Populate()
{
	// Create a simple layout with just a ColumnListView
	BRect r = Bounds();
	r.InsetBy(10, 10);  // Add some padding
	r.right -= 180;

	BColumnListView* listView = new BColumnListView(r, "gridview", B_FOLLOW_ALL, B_WILL_DRAW, B_FANCY_BORDER);
	
	Lock();
	AddChild(listView);
	Unlock();
	
	float width = be_plain_font->StringWidth("00000") + 20;
	listView->AddColumn(new BStringColumn("ID", width, width, 100, B_TRUNCATE_END), 0);
	listView->AddColumn(new BStringColumn("Type", width, width, 100, B_TRUNCATE_END), 1);
	listView->AddColumn(new BStringColumn("Name", 150, 50, 300, B_TRUNCATE_END), 2);
	listView->AddColumn(new BSizeColumn("Data", 150, 50, 300), 3);

	for (int32 i = 0; i < 25; i++)
		listView->AddRow(new SampleDataRow());

	r = Bounds();
	r.InsetBy(10, 10);  // Add some padding
	r.left = r.right - 190;
	SystemInfoView* sysInfo = new SystemInfoView(r, B_FOLLOW_TOP_BOTTOM | B_FOLLOW_RIGHT);

	Lock();
	AddChild(sysInfo);
	Unlock();

	printf("be_plain_font size: %f\n", be_plain_font->Size());
}


void DisWindow::MessageReceived(BMessage* message)
{
	BWindow::MessageReceived(message);
}

//	#pragma mark - SystemInfoView

SystemInfoView::SystemInfoView(BRect rect, uint32 followFlags)
	: BView(rect, "system_info", followFlags, B_WILL_DRAW | B_PULSE_NEEDED)
{
	SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	
	// Create labels for showing BColumnListView information
	float yPos = 15;
	float xPos = 15;
	float labelHeight = 40;
	float spacing = 45;
	
	// Bounds label
	fBoundsLabel = new BStringView(BRect(xPos, yPos, xPos + 200, yPos + labelHeight), 
		"bounds", "Bounds:\nL: 0 T: 0 R: 0 B: 0", B_FOLLOW_LEFT | B_FOLLOW_TOP);
	AddChild(fBoundsLabel);
	yPos += spacing;
	
	// Frame label
	fFrameLabel = new BStringView(BRect(xPos, yPos, xPos + 200, yPos + labelHeight), 
		"frame", "Frame:\nL: 0 T: 0 R: 0 B: 0", B_FOLLOW_LEFT | B_FOLLOW_TOP);
	AddChild(fFrameLabel);
	yPos += spacing;

	// Row height label
	fRowHeightLabel = new BStringView(BRect(xPos, yPos, xPos + 200, yPos + labelHeight), 
		"row_height", "Row Height:\n0", B_FOLLOW_LEFT | B_FOLLOW_TOP);
	AddChild(fRowHeightLabel);
	yPos += spacing;
}


SystemInfoView::~SystemInfoView()
{
}


void
SystemInfoView::AttachedToWindow()
{
	BView::AttachedToWindow();
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
	if (message->what == UPDATE_SYSINFO) {
		UpdateInfo();
	} else {
		BView::MessageReceived(message);
	}
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
}


void
SystemInfoView::UpdateInfo()
{
	if (!Window())
		return;
	
	BWindow* window = Window();
	
	// Scroll position - get from the internal scrolling view
	BColumnListView* listView = dynamic_cast<BColumnListView*>(window->FindView("gridview"));
	BRect outlineFrame;
	BRect outlineBounds;

	if (listView) {
		// BColumnListView has an internal OutlineView that actually scrolls
		BView* scrollView = listView->ScrollView();
		if (scrollView) {
			outlineBounds = scrollView->Bounds();
			outlineFrame = scrollView->Frame();
		}
	}

	char posText[100];
	snprintf(posText, sizeof(posText), "Bounds:\nL: %.f T: %.f R: %.f B: %.f", outlineBounds.left, outlineBounds.top, outlineBounds.right, outlineBounds.bottom);
	fBoundsLabel->SetText(posText);
	
	// Frame rect
	char rectText[100];
	snprintf(rectText, sizeof(rectText), "Frame:\nL: %.f T: %.f R: %.f B: %.f", outlineFrame.left, outlineFrame.top, outlineFrame.right, outlineFrame.bottom);
	fFrameLabel->SetText(rectText);

	// Row height
	if (listView && listView->CountRows() > 0) {
		BRow* firstRow = listView->RowAt(0);
		if (firstRow) {
			char rowHeightText[100];
			snprintf(rowHeightText, sizeof(rowHeightText), "Row Height:\n%.f", firstRow->Height());
			fRowHeightLabel->SetText(rowHeightText);
		}
	} else if (listView) {
		fRowHeightLabel->SetText("Row Height:\n0 (No rows)");
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

