/*
Open Tracker License

Terms and Conditions

Copyright (c) 1991-2000, Be Incorporated. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice applies to all licensees
and shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF TITLE, MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
BE INCORPORATED BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF, OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of Be Incorporated shall not be
used in advertising or otherwise to promote the sale, use or other dealings in
this Software without prior written authorization from Be Incorporated.

Tracker(TM), Be(R), BeOS(R), and BeIA(TM) are trademarks or registered trademarks
of Be Incorporated in the United States and other countries. Other brand product
names are registered trademarks or trademarks of their respective holders.
All rights reserved.
*/


#include "InfoWindow.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <Alert.h>
#include <Catalog.h>
#include <Debug.h>
#include <Directory.h>
#include <File.h>
#include <Font.h>
#include <Locale.h>
#include <MenuField.h>
#include <Mime.h>
#include <NodeInfo.h>
#include <NodeMonitor.h>
#include <Path.h>
#include <PopUpMenu.h>
#include <Region.h>
#include <Roster.h>
#include <Screen.h>
#include <ScrollView.h>
#include <StringFormat.h>
#include <SymLink.h>
#include <TabView.h>
#include <TextView.h>
#include <Volume.h>
#include <VolumeRoster.h>

#include "Attributes.h"
#include "AttributesView.h"
#include "AutoLock.h"
#include "Commands.h"
#include "DialogPane.h"
#include "FSUtils.h"
#include "GeneralInfoView.h"
#include "IconCache.h"
#include "Model.h"
#include "NavMenu.h"
#include "PoseView.h"
#include "StringForSize.h"
#include "Tracker.h"
#include "WidgetAttributeText.h"


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "InfoWindow"


const uint32 kNewTargetSelected = 'selc';

//	#pragma mark - BInfoWindow


BInfoWindow::BInfoWindow(Model* model, int32 group_index,
	LockingList<BWindow>* list)
	:
	BWindow(BInfoWindow::InfoWindowRect(),
		"InfoWindow", B_TITLED_WINDOW,
		B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS,
		B_CURRENT_WORKSPACE),
	fModel(model),
	fStopCalc(false),
	fIndex(group_index),
	fCalcThreadID(-1),
	fWindowList(list),
	fPermissionsView(NULL),
	fFilePanel(NULL),
	fFilePanelOpen(false)
{
	SetPulseRate(1000000);
		// we use pulse to check freebytes on volume

	//TTracker::WatchNode(model->NodeRef(), B_WATCH_ALL | B_WATCH_MOUNT, this);

	// window list is Locked by Tracker around this constructor
	if (list != NULL)
		list->AddItem(this);

	AddShortcut('E', 0, new BMessage(kEditName));
	AddShortcut('O', 0, new BMessage(kOpenSelection));
	AddShortcut('U', 0, new BMessage(kUnmountVolume));
	AddShortcut('P', 0, new BMessage(kPermissionsSelected));

	BGroupLayout* layout = new BGroupLayout(B_VERTICAL, 0);
	SetLayout(layout);

	BModelOpener modelOpener(TargetModel());
	if (TargetModel()->InitCheck() != B_OK)
		return;

	fHeaderView = new HeaderView(TargetModel());
	AddChild(fHeaderView);
	BTabView* tabView = new BTabView("tabs");
	tabView->SetBorder(B_NO_BORDER);
	AddChild(tabView);

	fGeneralInfoView = new GeneralInfoView(TargetModel());
	tabView->AddTab(fGeneralInfoView);

	BRect permissionsBounds(0,
		fGeneralInfoView->Bounds().bottom,
		fGeneralInfoView->Bounds().right,
		fGeneralInfoView->Bounds().bottom + 103);

	fPermissionsView = new FilePermissionsView(
		permissionsBounds, fModel);
	tabView->AddTab(fPermissionsView);

	tabView->AddTab(new AttributesView(TargetModel()));

	// This window accepts messages before being shown, so let's start the
	// looper immediately.
	Run();
}


BInfoWindow::~BInfoWindow()
{
	// Check to make sure the file panel is destroyed
	delete fFilePanel;
	delete fModel;
}


BRect
BInfoWindow::InfoWindowRect()
{
	// starting size of window
	return BRect(70, 50, 385, 240);
}


void
BInfoWindow::Quit()
{
	stop_watching(this);

	if (fWindowList) {
		AutoLock<LockingList<BWindow> > lock(fWindowList);
		fWindowList->RemoveItem(this);
	}

	fStopCalc = true;

	// wait until CalcSize thread has terminated before closing window
	status_t result;
	wait_for_thread(fCalcThreadID, &result);

	_inherited::Quit();
}


bool
BInfoWindow::IsShowing(const node_ref* node) const
{
	return *TargetModel()->NodeRef() == *node;
}


void
BInfoWindow::Show()
{
	if (TargetModel()->InitCheck() != B_OK) {
		Close();
		return;
	}

	AutoLock<BWindow> lock(this);

	// position window appropriately based on index
	BRect windRect(InfoWindowRect());
	if ((fIndex + 2) % 2 == 1) {
		windRect.OffsetBy(320, 0);
		fIndex--;
	}

	windRect.OffsetBy(fIndex * 8, fIndex * 8);

	// make sure window is visible on screen
	BScreen screen(this);
	if (!windRect.Intersects(screen.Frame()))
		windRect.OffsetTo(50, 50);

	MoveTo(windRect.LeftTop());

	// volume case is handled by view
	if (!TargetModel()->IsVolume() && !TargetModel()->IsRoot()) {
		if (TargetModel()->IsDirectory()) {
			// if this is a folder then spawn thread to calculate size
			SetSizeString(B_TRANSLATE("calculating" B_UTF8_ELLIPSIS));
			fCalcThreadID = spawn_thread(BInfoWindow::CalcSize, "CalcSize",
				B_NORMAL_PRIORITY, this);
			resume_thread(fCalcThreadID);
		} else {
			fGeneralInfoView->SetLastSize(TargetModel()->StatBuf()->st_size);

			BString sizeStr;
			GetSizeString(sizeStr, fGeneralInfoView->LastSize(), 0);
			SetSizeString(sizeStr.String());
		}
	}

	BString buffer(B_TRANSLATE_COMMENT("%name info", "InfoWindow Title"));
	buffer.ReplaceFirst("%name", TargetModel()->Name());
	SetTitle(buffer.String());

	lock.Unlock();
	_inherited::Show();
}


void
BInfoWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kRestoreState:
			Show();
			break;

		case kOpenSelection:
		{
			BMessage refsMessage(B_REFS_RECEIVED);
			refsMessage.AddRef("refs", fModel->EntryRef());

			// add a messenger to the launch message that will be used to
			// dispatch scripting calls from apps to the PoseView
			refsMessage.AddMessenger("TrackerViewToken", BMessenger(this));
			be_app->PostMessage(&refsMessage);
			break;
		}

		case kEditName:
		{
			BEntry entry(fModel->EntryRef());
			fHeaderView->BeginEditingTitle();
			break;
		}

		case kIdentifyEntry:
		{
			bool force = (modifiers() & B_OPTION_KEY) != 0;
			BEntry entry;
			if (entry.SetTo(fModel->EntryRef(), true) == B_OK) {
				BPath path;
				if (entry.GetPath(&path) == B_OK)
					update_mime_info(path.Path(), true, false, force ? 2 : 1);
			}
			break;
		}

		case kRecalculateSize:
		{
			fStopCalc = true;
			// Wait until any current CalcSize thread has terminated before
			// starting a new one
			status_t result;
			wait_for_thread(fCalcThreadID, &result);

			// Start recalculating..
			fStopCalc = false;
			SetSizeString(B_TRANSLATE("calculating" B_UTF8_ELLIPSIS));
			fCalcThreadID = spawn_thread(BInfoWindow::CalcSize, "CalcSize",
				B_NORMAL_PRIORITY, this);
			resume_thread(fCalcThreadID);
			break;
		}

		case B_CANCEL:
			break;

		case kPermissionsSelected:
		{
			BTabView* tabView = (BTabView*)FindView("tabs");
			tabView->Select(1);	
			break;
		}

		default:
			_inherited::MessageReceived(message);
			break;
	}
}


void
BInfoWindow::GetSizeString(BString& result, off_t size, int32 fileCount)
{
	static BStringFormat sizeFormat(B_TRANSLATE(
		"{0, plural, one{(# byte)} other{(# bytes)}}"));
	static BStringFormat countFormat(B_TRANSLATE(
		"{0, plural, one{for # file} other{for # files}}"));

	char sizeBuffer[128];
	result << string_for_size((double)size, sizeBuffer, sizeof(sizeBuffer));

	if (size >= kKBSize) {
		result << " ";

		sizeFormat.Format(result, size);
			// "bytes" translation could come from string_for_size
			// which could be part of the localekit itself
	}

	if (fileCount != 0) {
		result << " ";
		countFormat.Format(result, fileCount);
	}
}


int32
BInfoWindow::CalcSize(void* castToWindow)
{
	BInfoWindow* window = static_cast<BInfoWindow*>(castToWindow);
	BDirectory dir(window->TargetModel()->EntryRef());

	BEntry dirEntry, trashEntry;
	dir.GetEntry(&dirEntry);

	BString sizeString;

	// check if user has asked for trash dir info
	if (dirEntry != trashEntry) {
		// if not, perform normal info calculations
		off_t size = 0;
		int32 fileCount = 0;
		int32 dirCount = 0;

		// got the size value, update the size string
		GetSizeString(sizeString, size, fileCount);
	} else {
		// in the trash case, iterate through and sum up
		// size/counts for all present trash dirs
		off_t totalSize = 0, currentSize;
		int32 totalFileCount = 0, currentFileCount;
		int32 totalDirCount = 0, currentDirCount;
		BVolumeRoster volRoster;
		volRoster.Rewind();
		BVolume volume;
		while (volRoster.GetNextVolume(&volume) == B_OK) {
			if (!volume.IsPersistent())
				continue;

			currentSize = 0;
			currentFileCount = 0;
			currentDirCount = 0;
		}
		GetSizeString(sizeString, totalSize, totalFileCount);
	}

	if (window->StopCalc()) {
		// window closed, bail
		return B_OK;
	}

	AutoLock<BWindow> lock(window);
	if (lock.IsLocked())
		window->SetSizeString(sizeString.String());

	return B_OK;
}


void
BInfoWindow::SetSizeString(const char* sizeString)
{
	fGeneralInfoView->SetSizeString(sizeString);
}



