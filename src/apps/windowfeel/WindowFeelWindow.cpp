#include "WindowFeelWindow.h"

#include <Application.h>
#include <Button.h>
#include <Menu.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <StringView.h>
#include <View.h>
#include <cstdio>


enum {
	kMsgSetLook = 'wflk',
	kMsgSetFeel = 'wffe',
	kMsgNew = 'wfwn',
	kMsgQuit = 'wfqt'
};


struct LookEntry {
	const char* name;
	window_look value;
};


struct FeelEntry {
	const char* name;
	window_feel value;
};


static const LookEntry kLookEntries[] = {
	{"B_TITLED_WINDOW_LOOK", B_TITLED_WINDOW_LOOK},
	{"B_DOCUMENT_WINDOW_LOOK", B_DOCUMENT_WINDOW_LOOK},
	{"B_MODAL_WINDOW_LOOK", B_MODAL_WINDOW_LOOK},
	{"B_FLOATING_WINDOW_LOOK", B_FLOATING_WINDOW_LOOK},
	{"B_BORDERED_WINDOW_LOOK", B_BORDERED_WINDOW_LOOK},
	{"B_NO_BORDER_WINDOW_LOOK", B_NO_BORDER_WINDOW_LOOK},
};


static const FeelEntry kFeelEntries[] = {
	{"B_NORMAL_WINDOW_FEEL", B_NORMAL_WINDOW_FEEL},
	{"B_MODAL_APP_WINDOW_FEEL", B_MODAL_APP_WINDOW_FEEL},
	{"B_MODAL_SUBSET_WINDOW_FEEL", B_MODAL_SUBSET_WINDOW_FEEL},
	{"B_MODAL_ALL_WINDOW_FEEL", B_MODAL_ALL_WINDOW_FEEL},
	{"B_FLOATING_APP_WINDOW_FEEL", B_FLOATING_APP_WINDOW_FEEL},
	{"B_FLOATING_SUBSET_WINDOW_FEEL", B_FLOATING_SUBSET_WINDOW_FEEL},
	{"B_FLOATING_ALL_WINDOW_FEEL", B_FLOATING_ALL_WINDOW_FEEL},
};


static BMenuItem*
find_item_by_value(BMenu* menu, const char* field, int32 value)
{
	for (int32 index = 0; index < menu->CountItems(); index++) {
		BMenuItem* item = menu->ItemAt(index);
		if (item == NULL)
			continue;

		BMessage* message = item->Message();
		if (message == NULL)
			continue;

		int32 itemValue;
		if (message->FindInt32(field, &itemValue) == B_OK && itemValue == value)
			return item;
	}

	return NULL;
}


WindowFeelWindow::WindowFeelWindow()
	:	WindowFeelWindow(B_TITLED_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL)

{
}


WindowFeelWindow::WindowFeelWindow(window_look look, window_feel feel)
	:	BWindow(BRect(100, 100, 460, 280), "WindowFeel",
		look, feel, B_ASYNCHRONOUS_CONTROLS),
		fStatusView(NULL)
{
	BView* background = new BView(Bounds(), "background", B_FOLLOW_ALL, B_WILL_DRAW);
	background->SetViewColor(216, 216, 216);
	AddChild(background);

	BPopUpMenu* lookMenu = new BPopUpMenu("window look");
	lookMenu->SetRadioMode(true);
	_AddLookItems(lookMenu);
	lookMenu->SetTargetForItems(this);

	BMenuField* lookField = new BMenuField(
		BRect(15, 18, Bounds().right - 15, 54),
		"look field", "Window look:", lookMenu);
	lookField->SetDivider(120);
	background->AddChild(lookField);

	BPopUpMenu* feelMenu = new BPopUpMenu("window feel");
	feelMenu->SetRadioMode(true);
	_AddFeelItems(feelMenu);
	feelMenu->SetTargetForItems(this);

	BMenuField* feelField = new BMenuField(
		BRect(15, 68, Bounds().right - 15, 104),
		"feel field", "Window feel:", feelMenu);
	feelField->SetDivider(120);
	background->AddChild(feelField);

	fStatusView = new BStringView(
		BRect(15, 138, Bounds().right - 205, 168),
		"status",
		"Select a look or feel to update the window.");
	fStatusView->SetResizingMode(B_FOLLOW_LEFT_RIGHT | B_FOLLOW_BOTTOM);
	background->AddChild(fStatusView);

	BButton* newButton = new BButton(
		BRect(Bounds().right - 185, 138, Bounds().right - 105, 168),
		"new button", "New", new BMessage(kMsgNew));
	newButton->SetResizingMode(B_FOLLOW_RIGHT | B_FOLLOW_BOTTOM);
	background->AddChild(newButton);

	BButton* quitButton = new BButton(
		BRect(Bounds().right - 95, 138, Bounds().right - 15, 168),
		"close button", "Close", new BMessage(kMsgQuit));
	quitButton->SetResizingMode(B_FOLLOW_RIGHT | B_FOLLOW_BOTTOM);
	background->AddChild(quitButton);

	SetSizeLimits(this->Bounds().Width(), this->Bounds().Width(),
		this->Bounds().Height(), this->Bounds().Height() + 100);

	_UpdateStatus();
}


bool
WindowFeelWindow::QuitRequested()
{
	if (be_app != NULL && be_app->CountWindows() <= 1)
		be_app->PostMessage(B_QUIT_REQUESTED);

	return true;
}


void
WindowFeelWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgSetLook:
		{
			int32 value;
			if (message->FindInt32("look", &value) == B_OK)
				_ApplyLook((window_look)value);
			return;
		}

		case kMsgSetFeel:
		{
			int32 value;
			if (message->FindInt32("feel", &value) == B_OK)
				_ApplyFeel((window_feel)value);
			return;
		}

		case kMsgNew:
		{
			WindowFeelWindow* window = new WindowFeelWindow(Look(), Feel());
			BRect frame = Frame();
			window->MoveTo(frame.left + 20, frame.top + 20);
			window->Show();
			return;
		}

		case kMsgQuit:
			PostMessage(B_QUIT_REQUESTED);
			return;

		default:
			break;
	}

	BWindow::MessageReceived(message);
}


void
WindowFeelWindow::_AddLookItems(BMenu* menu)
{
	for (size_t index = 0; index < sizeof(kLookEntries) / sizeof(kLookEntries[0]); index++) {
		BMessage* message = new BMessage(kMsgSetLook);
		message->AddInt32("look", (int32)kLookEntries[index].value);

		BMenuItem* item = new BMenuItem(kLookEntries[index].name, message);
		menu->AddItem(item);
		if (kLookEntries[index].value == Look())
			item->SetMarked(true);
	}
}


void
WindowFeelWindow::_AddFeelItems(BMenu* menu)
{
	for (size_t index = 0; index < sizeof(kFeelEntries) / sizeof(kFeelEntries[0]); index++) {
		BMessage* message = new BMessage(kMsgSetFeel);
		message->AddInt32("feel", (int32)kFeelEntries[index].value);

		BMenuItem* item = new BMenuItem(kFeelEntries[index].name, message);
		menu->AddItem(item);
		if (kFeelEntries[index].value == Feel())
			item->SetMarked(true);
	}
}


void
WindowFeelWindow::_ApplyLook(window_look look)
{
	SetLook(look);

	BMenuItem* item = find_item_by_value(((BMenuField*)FindView("look field"))->Menu(),
		"look", (int32)look);
	if (item != NULL)
		item->SetMarked(true);

	_UpdateStatus();
}


void
WindowFeelWindow::_ApplyFeel(window_feel feel)
{
	SetFeel(feel);

	BMenuItem* item = find_item_by_value(((BMenuField*)FindView("feel field"))->Menu(),
		"feel", (int32)feel);
	if (item != NULL)
		item->SetMarked(true);

	_UpdateStatus();
}


void
WindowFeelWindow::_UpdateStatus()
{
	char text[256];
	snprintf(text, sizeof(text), "look=%ld, feel=%ld", (long)Look(), (long)Feel());
	fStatusView->SetText(text);
}