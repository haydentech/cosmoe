#include <Catalog.h>
#include <LayoutBuilder.h>
#include <LayoutUtils.h>
#include <ScrollBar.h>
#include <Screen.h>


#include "TermWindow.h";
#include "TermConst.h"


extern bool g_bRun;


TermWindow::TermWindow(BRect frame, const char* title, window_type type, uint32 flags)
    : BWindow(frame, title, type, flags),
	scrollBar(NULL),
	termView(NULL)
{
		// init the GUI and add a tab
	_InitWindow();
}

TermWindow::~TermWindow()
{
	//g_pcWindow = NULL;
	g_bRun = false;
	close( g_nMasterPTY );
}

void TermWindow::MessageReceived(BMessage* msg)
{
	switch (msg->what)
	{
		case ID_SCROLL:
			termView->ScrollBack( scrollBar->Value() );
			break;

		default:
			BWindow::MessageReceived(msg);
	}
}


bool TermWindow::QuitRequested()
{
	return( true );
}


void
TermWindow::_InitWindow()
{
	BRect cTermFrame   = Bounds();
	BRect cScrollBarFrame = cTermFrame;

	//scrollBar = new BScrollBar( cScrollBarFrame, "", new BMessage( ID_SCROLL ), 0, 24 );
	scrollBar = new BScrollBar( cScrollBarFrame, "", NULL, 0, 24.0, B_VERTICAL );

	float width;
	scrollBar->GetPreferredSize(&width, NULL);
	cScrollBarFrame.left = cScrollBarFrame.right - width;
	//scrollBar->SetFrame( cScrollBarFrame );
	scrollBar->MoveTo(cScrollBarFrame.left, cScrollBarFrame.top);
	scrollBar->ResizeTo(cScrollBarFrame.Width(), cScrollBarFrame.Height());

	cTermFrame.right = cScrollBarFrame.left - 1;

	termView  = new TermView(cTermFrame, "", B_FOLLOW_ALL, B_WILL_DRAW | B_FRAME_EVENTS);

	this->AddChild( termView );
	this->AddChild( scrollBar );
	scrollBar->SetTarget(termView);

	IPoint cGlypSize( termView->GetGlyphSize() );
	IPoint cSizeOffset( int(cScrollBarFrame.Width()) % int(cGlypSize.x), 0 );
	this->SetWindowAlignment( B_PIXEL_ALIGNMENT,
									1,
									0,
									cGlypSize.x,
									cSizeOffset.x,
									1,
									0,
									cGlypSize.y,
									cSizeOffset.y);
	this->ResizeTo( cGlypSize.x * 80 + cScrollBarFrame.Width(), cGlypSize.y * 24 );

	int screenWidth = BScreen().Frame().Width();
	int screenHeight = BScreen().Frame().Height();

	if ( this->Frame().right >= screenWidth )
	{
		this->MoveTo( screenWidth / 2 - this->Frame().Width() * 0.5f, this->Frame().top );
	}

	if ( this->Frame().bottom >= screenHeight )
	{
		this->MoveTo( this->Frame().left, screenHeight / 2 - this->Frame().Height() * 0.5f );
	}
	this->Activate( true );
	this->Show();

	//g_pcTermView->FrameResized(500, 300);
}


int TermWindow::Write(const char* zBuf, int nSize)
{
	return termView->Write(zBuf, nSize);
}


void TermWindow::RefreshDisplay(bool bAll)
{
	termView->RefreshDisplay(bAll);
}