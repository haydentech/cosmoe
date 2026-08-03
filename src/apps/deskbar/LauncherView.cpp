#include "LauncherView.h"

#include <algorithm>
#include <string.h>
#include <strings.h>

#include <AppFileInfo.h>
#include <Bitmap.h>
#include <BitmapButton.h>
#include <ControlLook.h>
#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <FindDirectory.h>
#include <GroupLayout.h>
#include <Mime.h>
#include <Node.h>
#include <OpenWithTracker.h>
#include <Path.h>
#include <Roster.h>
#include <Size.h>

#include "BarApp.h"
#include "BarView.h"
namespace {

const float kLauncherInset = 0.0f;
const uint32 kLaunchShortcut = 'Lnch';
const uint32 kAddShortcut = 'Adsh';

const char*
launcher_leaf_name(const char* path)
{
	if (path == NULL)
		return "";

	const char* leaf = strrchr(path, '/');
	if (leaf != NULL && leaf[1] != '\0')
		return leaf + 1;

	return path;
}

} // namespace


TLauncherView::LauncherItem::LauncherItem(const entry_ref& ref,
	const BString& label)
	:
	fRef(ref),
	fLabel(label),
	fButton(NULL)
{
}


TLauncherView::LauncherItem::~LauncherItem()
{
}


TLauncherView::TLauncherView(TBarView* barView)
	:
	BView(BRect(0, 0, -1, -1), "LauncherView", B_FOLLOW_NONE,
		B_WILL_DRAW | B_SUPPORTS_LAYOUT),
	fBarView(barView),
	fLaunchers(8),
	fAddShortcutButton(NULL)
{
	SetViewUIColor(B_MENU_BACKGROUND_COLOR);
	BGroupLayout* layout = new BGroupLayout(B_HORIZONTAL, 0.0f);
	layout->SetInsets(kLauncherInset, kLauncherInset, kLauncherInset,
		kLauncherInset);
	SetLayout(layout);

	fAddShortcutButton = _CreateAddButton();
	if (fAddShortcutButton != NULL)
		AddChild(fAddShortcutButton);
}


TLauncherView::~TLauncherView()
{
	_ClearLaunchers();
}


void
TLauncherView::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kLaunchShortcut:
		{
			int32 index;
			if (message->FindInt32("index", &index) != B_OK)
				break;

			LauncherItem* item = fLaunchers.ItemAt(index);
			if (item != NULL)
				be_roster->Launch(&item->fRef);
			break;
		}

		case kAddShortcut:
			OpenWithTracker(B_USER_DESKBAR_DIRECTORY);
			break;

		default:
			BView::MessageReceived(message);
			break;
	}
}


void
TLauncherView::AttachedToWindow()
{
	BView::AttachedToWindow();
	if (fAddShortcutButton != NULL)
		fAddShortcutButton->SetTarget(this);
}


void
TLauncherView::Refresh()
{
	_ClearLaunchers();

	BDirectory directory;
	if (_GetShortcutsDirectory(directory) != B_OK) {
		SetToolTip((const char*)NULL);
		return;
	}

	BEntry entry;
	while (directory.GetNextEntry(&entry) == B_OK)
		_AddLauncher(entry);

	fLaunchers.SortItems([](const LauncherItem* a, const LauncherItem* b) {
		return strcasecmp(a->fLabel.String(), b->fLabel.String());
	});

	if (fAddShortcutButton != NULL && fAddShortcutButton->Parent() == this)
		RemoveChild(fAddShortcutButton);

	for (int32 i = 0; i < fLaunchers.CountItems(); i++) {
		LauncherItem* item = fLaunchers.ItemAt(i);
		if (item == NULL)
			continue;

		item->fButton = _CreateButton(*item, i);
		if (item->fButton != NULL)
			AddChild(item->fButton);
	}

	if (fAddShortcutButton != NULL && fAddShortcutButton->Parent() != this)
		AddChild(fAddShortcutButton);

	if (fAddShortcutButton != NULL)
		fAddShortcutButton->SetTarget(this);
}


float
TLauncherView::PreferredWidth() const
{
	float width = _AddButtonWidth();
	if (HasLaunchers())
		width += fLaunchers.CountItems() * _ButtonWidth();
	return width;
}


status_t
TLauncherView::_GetShortcutsDirectory(BDirectory& directory) const
{
	BPath path;
	status_t error = find_directory(B_USER_DESKBAR_DIRECTORY, &path, false);
	if (error != B_OK)
		return error;

	error = path.Append("shortcuts");
	if (error != B_OK)
		return error;

	return directory.SetTo(path.Path());
}


status_t
TLauncherView::_AddLauncher(const BEntry& sourceEntry)
{
	if (!sourceEntry.IsSymLink())
		return B_BAD_VALUE;

	entry_ref sourceRef;
	status_t error = sourceEntry.GetRef(&sourceRef);
	if (error != B_OK)
		return error;

	BEntry targetEntry(&sourceRef, true);
	if (!targetEntry.Exists() || !targetEntry.IsFile())
		return B_BAD_VALUE;

	entry_ref ref;
	error = targetEntry.GetRef(&ref);
	if (error != B_OK)
		return error;

	BString label = _FetchLabel(ref);
	LauncherItem* item = new(std::nothrow) LauncherItem(ref, label);
	if (item == NULL) {
		return B_NO_MEMORY;
	}

	if (!fLaunchers.AddItem(item)) {
		delete item;
		return B_NO_MEMORY;
	}

	return B_OK;
}


BBitmapButton*
TLauncherView::_CreateButton(const LauncherItem& item, int32 index) const
{
	BBitmap* icon = _FetchIcon(item.fRef);
	if (icon == NULL)
		return NULL;

	BMessage* message = new(std::nothrow) BMessage(kLaunchShortcut);
	if (message == NULL) {
		delete icon;
		return NULL;
	}
	message->AddInt32("index", index);

	BBitmapButton* button = new(std::nothrow) BBitmapButton(
		reinterpret_cast<const uint8*>(icon->Bits()),
		icon->Bounds().IntegerWidth() + 1,
		icon->Bounds().IntegerHeight() + 1,
		icon->ColorSpace(), message);
	delete icon;
	if (button == NULL)
		return NULL;

	float buttonWidth = _ButtonWidth();
	button->SetBackgroundMode(BBitmapButton::MENUBAR_BACKGROUND);
	button->SetToolTip(item.fLabel.String());
	button->SetTarget(this);
	button->SetExplicitMinSize(BSize(buttonWidth, buttonWidth));
	button->SetExplicitMaxSize(BSize(buttonWidth, B_SIZE_UNLIMITED));

	return button;
}


BBitmapButton*
TLauncherView::_CreateAddButton() const
{
	float buttonWidth = _ButtonWidth();
	float addButtonWidth = _AddButtonWidth();
	int32 width = std::max(1, static_cast<int32>(addButtonWidth));
	int32 height = std::max(1, static_cast<int32>(buttonWidth));

	BBitmap bitmap(BRect(0, 0, width - 1, height - 1), B_RGBA32);
	if (bitmap.InitCheck() != B_OK)
		return NULL;

	memset(bitmap.Bits(), 0, bitmap.BitsLength());
	uint8* bits = reinterpret_cast<uint8*>(bitmap.Bits());
	int32 bytesPerRow = bitmap.BytesPerRow();
	int32 centerX = (width / 2) - 2;
	int32 centerY = (height / 2) - 1;
	int32 halfArm = std::max(2, std::min(width, height) / 4);
	int32 thickness = std::max(1, std::min(width, height) / 9);

	for (int32 y = centerY - halfArm; y <= centerY + halfArm; y++) {
		for (int32 x = centerX - thickness; x <= centerX + thickness; x++) {
			if (x < 0 || x >= width || y < 0 || y >= height)
				continue;

			uint8* pixel = bits + y * bytesPerRow + x * 4;
			pixel[0] = 0x40;
			pixel[1] = 0x40;
			pixel[2] = 0x40;
			pixel[3] = 0xff;
		}
	}
	for (int32 y = centerY - thickness; y <= centerY + thickness; y++) {
		for (int32 x = centerX - halfArm; x <= centerX + halfArm; x++) {
			if (x < 0 || x >= width || y < 0 || y >= height)
				continue;

			uint8* pixel = bits + y * bytesPerRow + x * 4;
			pixel[0] = 0x40;
			pixel[1] = 0x40;
			pixel[2] = 0x40;
			pixel[3] = 0xff;
		}
	}

	BMessage* message = new(std::nothrow) BMessage(kAddShortcut);
	if (message == NULL)
		return NULL;

	BBitmapButton* button = new(std::nothrow) BBitmapButton(
		reinterpret_cast<const uint8*>(bitmap.Bits()), width, height,
		bitmap.ColorSpace(), message);
	if (button == NULL)
		return NULL;

	button->SetBackgroundMode(BBitmapButton::MENUBAR_BACKGROUND);
	button->SetToolTip("Add shortcut");
	button->SetExplicitMinSize(BSize(addButtonWidth, buttonWidth));
	button->SetExplicitMaxSize(BSize(addButtonWidth, B_SIZE_UNLIMITED));
	return button;
}


BBitmap*
TLauncherView::_FetchIcon(const entry_ref& ref) const
{
	int32 iconSize = static_cast<TBarApp*>(be_app)->Settings()->iconSize;
	int32 composed = be_control_look->ComposeIconSize(iconSize).IntegerWidth() + 1;
	BRect iconRect(0, 0, composed - 1, composed - 1);
	BBitmap* icon = new(std::nothrow) BBitmap(iconRect, B_RGBA32);
	if (icon == NULL || icon->InitCheck() != B_OK) {
		delete icon;
		return NULL;
	}

	BFile file(ref.name, B_READ_ONLY);
	BAppFileInfo appInfo(&file);
	if (appInfo.InitCheck() == B_OK
		&& appInfo.GetIcon(icon, (icon_size)iconSize) == B_OK) {
		return icon;
	}

	char signature[B_MIME_TYPE_LENGTH];
	BMimeType appMimeType;
	if (appInfo.InitCheck() == B_OK
		&& appInfo.GetSignature(signature) == B_OK
		&& appMimeType.SetTo(signature) == B_OK
		&& appMimeType.GetIcon(icon, (icon_size)iconSize) == B_OK) {
		return icon;
	}

	BMimeType appMime(B_APP_MIME_TYPE);
	if (appMime.InitCheck() == B_OK
		&& appMime.GetIcon(icon, (icon_size)iconSize) == B_OK) {
		return icon;
	}

	uint8* iconBits = (uint8*)icon->Bits();
	for (int32 i = 0; i < icon->BitsLength(); i += 4) {
		iconBits[i + 0] = 0x80;
		iconBits[i + 1] = 0x80;
		iconBits[i + 2] = 0x80;
		iconBits[i + 3] = 0xff;
	}

	return icon;
}


BString
TLauncherView::_FetchLabel(const entry_ref& ref) const
{
	BString label(launcher_leaf_name(ref.name));
	return label;
}


void
TLauncherView::_ClearLaunchers()
{
	for (int32 i = 0; i < fLaunchers.CountItems(); i++) {
		LauncherItem* item = fLaunchers.ItemAt(i);
		if (item == NULL || item->fButton == NULL)
			continue;

		RemoveChild(item->fButton);
		delete item->fButton;
		item->fButton = NULL;
	}

	fLaunchers.MakeEmpty();
}





float
TLauncherView::_ButtonWidth() const
{
	if (fBarView != NULL)
		return std::max(14.0f, fBarView->TeamMenuItemHeight() - 2.0f);

	return 0.0f;
}


float
TLauncherView::_AddButtonWidth() const
{
	return std::max(14.0f, _ButtonWidth() * 0.6f);
}
