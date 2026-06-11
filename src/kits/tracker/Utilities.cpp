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

#ifdef _WIN32
// strcasestr is provided by libbe on Windows
extern "C" char *strcasestr(const char *s, const char *find);
#endif

#include "Utilities.h"

#include <ctype.h>
#include <fs_attr.h>
#include <fs_info.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <BitmapStream.h>
#include <Catalog.h>
#include <ControlLook.h>
#include <Debug.h>
#include <Font.h>
#include <IconUtils.h>
#include <MenuItem.h>
#include <OS.h>
#include <PopUpMenu.h>
#include <Region.h>
#include <StorageDefs.h>
#include <TextView.h>
#include <Volume.h>
#include <VolumeRoster.h>
#include <Window.h>

#include "Attributes.h"
#include "ContainerWindow.h"
#include "MimeTypes.h"
#include "Model.h"
#include "PoseView.h"


#ifndef _IMPEXP_BE
#	define _IMPEXP_BE
#endif
extern _IMPEXP_BE const uint32	LARGE_ICON_TYPE;
extern _IMPEXP_BE const uint32	MINI_ICON_TYPE;


FILE* logFile = NULL;

static const float kMinSeparatorStubX = 10;
static const float kStubToStringSlotX = 5;


namespace BPrivate {

const float kExactMatchScore = INFINITY;


bool gLocalizedNamePreferred;


float
ReadOnlyTint(rgb_color base)
{
	// darken tint if read-only (or lighten if dark)
	return base.IsLight() ? B_DARKEN_1_TINT : 0.853;
}


float
ReadOnlyTint(color_which base)
{
	return ReadOnlyTint(ui_color(base));
}


rgb_color
InvertColor(rgb_color color)
{
	return make_color(255 - color.red, 255 - color.green, 255 - color.blue);
}


rgb_color
InvertColorSmart(rgb_color color)
{
	rgb_color inverted = InvertColor(color);

	// The colors are different enough, we can use inverted
	if (rgb_color::Contrast(color, inverted) > 127)
		return inverted;

	// use black or white
	return color.IsLight() ? kBlack : kWhite;
}


bool
SecondaryMouseButtonDown(int32 modifiers, int32 buttons)
{
	return (buttons & B_SECONDARY_MOUSE_BUTTON) != 0
		|| ((buttons & B_PRIMARY_MOUSE_BUTTON) != 0
			&& (modifiers & B_CONTROL_KEY) != 0);
}


uint32
SeededHashString(const char* string, uint32 seed)
{
	char ch;
	uint32 hash = seed;

	while((ch = *string++) != 0) {
		hash = (hash << 7) ^ (hash >> 24);
		hash ^= ch;
	}
	hash ^= hash << 12;

	return hash;
}


uint32
AttrHashString(const char* string, uint32 type)
{
	char c;
	uint32 hash = 0;

	while((c = *string++) != 0) {
		hash = (hash << 7) ^ (hash >> 24);
		hash ^= c;
	}
	hash ^= hash << 12;

	hash &= ~0xff;
	hash |= type;

	return hash;
}


bool
ValidateStream(BMallocIO* stream, uint32 key, int32 version)
{
	uint32 testKey;
	int32 testVersion;

	if (stream->Read(&testKey, sizeof(uint32)) <= 0
		|| stream->Read(&testVersion, sizeof(int32)) <= 0) {
		return false;
	}

	return testKey == key && testVersion == version;
}


void
DisallowFilenameKeys(BTextView* textView)
{
	// disallow control characters
	for (uint32 i = 0; i < 0x20; ++i)
		textView->DisallowChar(i);

	textView->DisallowChar('/');
}


void
DisallowMetaKeys(BTextView* textView)
{
	textView->DisallowChar(B_TAB);
	textView->DisallowChar(B_ESCAPE);
	textView->DisallowChar(B_INSERT);
	textView->DisallowChar(B_DELETE);
	textView->DisallowChar(B_HOME);
	textView->DisallowChar(B_END);
	textView->DisallowChar(B_PAGE_UP);
	textView->DisallowChar(B_PAGE_DOWN);
	textView->DisallowChar(B_FUNCTION_KEY);
}


}	// namespace BPrivate


void
PoseInfo::EndianSwap(void* castToThis)
{
	PoseInfo* self = (PoseInfo*)castToThis;

	PRINT(("swapping PoseInfo\n"));

#if defined(_WIN32)
	// Special case for Windows' comically small ino_t size
	STATIC_ASSERT(sizeof(ino_t) == sizeof(int16));
	self->fInitedDirectory = SwapInt16(self->fInitedDirectory);
#else
	STATIC_ASSERT(sizeof(ino_t) == sizeof(int64));
	self->fInitedDirectory = SwapInt64(self->fInitedDirectory);
#endif
	swap_data(B_POINT_TYPE, &self->fLocation, sizeof(BPoint), B_SWAP_ALWAYS);

	// do a sanity check on the icon position
	if (self->fLocation.x < -20000 || self->fLocation.x > 20000
		|| self->fLocation.y < -20000 || self->fLocation.y > 20000) {
		// position out of range, force autoplcemement
		PRINT((" rejecting icon position out of range\n"));
		self->fInitedDirectory = -1LL;
		self->fLocation = BPoint(0, 0);
	}
}


void
PoseInfo::PrintToStream()
{
	PRINT(("%s, inode:%" B_PRIx64 ", location %f %f\n",
		fInvisible ? "hidden" : "visible",
		fInitedDirectory, fLocation.x, fLocation.y));
}


// #pragma mark - OffscreenBitmap


OffscreenBitmap::OffscreenBitmap(BRect frame)
	:
	fBitmap(NULL)
{
	NewBitmap(frame);
}


OffscreenBitmap::OffscreenBitmap()
	:
	fBitmap(NULL)
{
}


OffscreenBitmap::~OffscreenBitmap()
{
	delete fBitmap;
}


void
OffscreenBitmap::NewBitmap(BRect bounds)
{
	delete fBitmap;
	fBitmap = new(std::nothrow) BBitmap(bounds, B_RGB32, true);
	if (fBitmap != NULL && fBitmap->Lock()) {
		BView* view = new BView(fBitmap->Bounds(), "", B_FOLLOW_NONE, 0);
		fBitmap->AddChild(view);

		BRect clipRect = view->Bounds();
		BRegion newClip;
		newClip.Set(clipRect);
		view->ConstrainClippingRegion(&newClip);

		fBitmap->Unlock();
	} else {
		delete fBitmap;
		fBitmap = NULL;
	}
}


BView*
OffscreenBitmap::BeginUsing(BRect frame)
{
	if (!fBitmap || fBitmap->Bounds() != frame)
		NewBitmap(frame);

	fBitmap->Lock();
	return View();
}


void
OffscreenBitmap::DoneUsing()
{
	fBitmap->Unlock();
}


BBitmap*
OffscreenBitmap::Bitmap() const
{
	ASSERT(fBitmap);
	ASSERT(fBitmap->IsLocked());
	return fBitmap;
}


BView*
OffscreenBitmap::View() const
{
	ASSERT(fBitmap);
	return fBitmap->ChildAt(0);
}


// #pragma mark - BPrivate functions


namespace BPrivate {

// Changes the alpha value of the given bitmap to create a nice
// horizontal fade out in the specified region.
// "from" is always transparent, "to" opaque.
void
FadeRGBA32Horizontal(uint32* bits, int32 width, int32 height, int32 from,
	int32 to)
{
	// check parameters
	if (width < 0 || height < 0 || from < 0 || to < 0)
		return;

	float change = 1.f / (to - from);
	if (from > to) {
		int32 temp = from;
		from = to;
		to = temp;
	}

	for (int32 y = 0; y < height; y++) {
		float alpha = change > 0 ? 0.0f : 1.0f;

		for (int32 x = from; x <= to; x++) {
			if (bits[x] & 0xff000000) {
				uint32 a = uint32((bits[x] >> 24) * alpha);
				bits[x] = (bits[x] & 0x00ffffff) | (a << 24);
			}
			alpha += change;
		}
		bits += width;
	}
}


/*!	Changes the alpha value of the given bitmap to create a nice
	vertical fade out in the specified region.
	"from" is always transparent, "to" opaque.
*/
void
FadeRGBA32Vertical(uint32* bits, int32 width, int32 height, int32 from,
	int32 to)
{
	// check parameters
	if (width < 0 || height < 0 || from < 0 || to < 0)
		return;

	if (from > to)
		bits += width * (height - (from - to));

	float change = 1.f / (to - from);
	if (from > to) {
		int32 temp = from;
		from = to;
		to = temp;
	}

	float alpha = change > 0 ? 0.0f : 1.0f;

	for (int32 y = from; y <= to; y++) {
		for (int32 x = 0; x < width; x++) {
			if (bits[x] & 0xff000000) {
				uint32 a = uint32((bits[x] >> 24) * alpha);
				bits[x] = (bits[x] & 0x00ffffff) | (a << 24);
			}
		}
		alpha += change;
		bits += width;
	}
}

}	// namespace BPrivate


// #pragma mark - FlickerFreeStringView


FlickerFreeStringView::FlickerFreeStringView(BRect bounds, const char* name,
	const char* text, uint32 resizingMode, uint32 flags)
	:
	BStringView(bounds, name, text, resizingMode, flags),
	fBitmap(NULL),
	fViewColor(ViewColor()),
	fLowColor(LowColor()),
	fOriginalBitmap(NULL)
{
}


FlickerFreeStringView::FlickerFreeStringView(BRect bounds, const char* name,
	const char* text, BBitmap* inBitmap, uint32 resizingMode, uint32 flags)
	:
	BStringView(bounds, name, text, resizingMode, flags),
	fBitmap(NULL),
	fViewColor(ViewColor()),
	fLowColor(LowColor()),
	fOriginalBitmap(inBitmap)
{
}


FlickerFreeStringView::~FlickerFreeStringView()
{
	delete fBitmap;
}


void
FlickerFreeStringView::Draw(BRect)
{
	BRect bounds(Bounds());
	if (fBitmap == NULL)
		fBitmap = new OffscreenBitmap(Bounds());

	BView* offscreen = fBitmap->BeginUsing(bounds);

	if (Parent() != NULL) {
		fViewColor = Parent()->ViewColor();
		fLowColor = Parent()->ViewColor();
	}

	offscreen->SetViewColor(fViewColor);
	offscreen->SetHighColor(HighColor());
	offscreen->SetLowColor(fLowColor);

	BFont font;
	GetFont(&font);
	offscreen->SetFont(&font);

	offscreen->Sync();
	if (fOriginalBitmap != NULL)
		offscreen->DrawBitmap(fOriginalBitmap, Frame(), bounds);
	else
		offscreen->FillRect(bounds, B_SOLID_LOW);

	if (Text() != NULL) {
		BPoint loc;

		font_height	height;
		GetFontHeight(&height);

		edge_info eInfo;
		switch (Alignment()) {
			case B_ALIGN_LEFT:
			case B_ALIGN_HORIZONTAL_UNSET:
			case B_ALIGN_USE_FULL_WIDTH:
			{
				// If the first char has a negative left edge give it
				// some more room by shifting that much more to the right.
				font.GetEdges(Text(), 1, &eInfo);
				loc.x = bounds.left + (2 - eInfo.left);
				break;
			}

			case B_ALIGN_CENTER:
			{
				float width = StringWidth(Text());
				float center = (bounds.right - bounds.left) / 2;
				loc.x = center - (width/2);
				break;
			}

			case B_ALIGN_RIGHT:
			{
				float width = StringWidth(Text());
				loc.x = bounds.right - width - 2;
				break;
			}
		}
		loc.y = bounds.bottom - (1 + height.descent);
		offscreen->DrawString(Text(), loc);
	}
	offscreen->Sync();
	SetDrawingMode(B_OP_COPY);
	DrawBitmap(fBitmap->Bitmap());
	fBitmap->DoneUsing();
}


void
FlickerFreeStringView::AttachedToWindow()
{
	_inherited::AttachedToWindow();
	if (Parent() != NULL) {
		fViewColor = Parent()->ViewColor();
		fLowColor = Parent()->ViewColor();
	}
	SetViewColor(B_TRANSPARENT_32_BIT);
	SetLowColor(B_TRANSPARENT_32_BIT);
}


void
FlickerFreeStringView::SetViewColor(rgb_color color)
{
	if (fViewColor != color) {
		fViewColor = color;
		Invalidate();
	}
	_inherited::SetViewColor(B_TRANSPARENT_32_BIT);
}


void
FlickerFreeStringView::SetLowColor(rgb_color color)
{
	if (fLowColor != color) {
		fLowColor = color;
		Invalidate();
	}
	_inherited::SetLowColor(B_TRANSPARENT_32_BIT);
}


// #pragma mark - TitledSeparatorItem


TitledSeparatorItem::TitledSeparatorItem(const char* label)
	:
	BMenuItem(label, 0)
{
	_inherited::SetEnabled(false);
}


TitledSeparatorItem::~TitledSeparatorItem()
{
}


void
TitledSeparatorItem::SetEnabled(bool)
{
	// leave disabled
}


void
TitledSeparatorItem::GetContentSize(float* width, float* height)
{
	_inherited::GetContentSize(width, height);

	// Adjust for the extra space needed by the separator bars at the left and right
	if (width)
		*width += (kMinSeparatorStubX + kStubToStringSlotX) * 2;
}


inline rgb_color
ShiftMenuBackgroundColor(float by)
{
	return tint_color(ui_color(B_MENU_BACKGROUND_COLOR), by);
}


void
TitledSeparatorItem::Draw()
{
	BRect frame(Frame());

	BMenu* parent = Menu();
	ASSERT(parent != NULL);

	menu_info minfo;
	get_menu_info(&minfo);

	if (minfo.separator > 0) {
		frame.left += 10;
		frame.right -= 10;
	} else {
		frame.left += 1;
		frame.right -= 1;
	}

	float startX = frame.left;
	float endX = frame.right;

	float maxStringWidth = endX - startX - (2 * kMinSeparatorStubX
		+ 2 * kStubToStringSlotX);

	// ToDo:
	// handle case where maxStringWidth turns out negative here

	BString truncatedLabel(Label());
	parent->TruncateString(&truncatedLabel, B_TRUNCATE_END, maxStringWidth);

	maxStringWidth = parent->StringWidth(truncatedLabel.String());

	// first calculate the length of the stub part of the
	// divider line, so we can use it for secondStartX
	float firstEndX = ((endX - startX) - maxStringWidth) / 2
		- kStubToStringSlotX;
	if (firstEndX < 0)
		firstEndX = 0;

	float secondStartX = endX - firstEndX;

	// now finish calculating firstEndX
	firstEndX += startX;

	parent->PushState();

	int32 y = (int32) (frame.top + (frame.bottom - frame.top) / 2);

	parent->BeginLineArray(minfo.separator == 2 ? 6 : 4);
	parent->AddLine(BPoint(frame.left, y), BPoint(firstEndX, y),
		ShiftMenuBackgroundColor(B_DARKEN_1_TINT));
	parent->AddLine(BPoint(secondStartX, y), BPoint(frame.right, y),
		ShiftMenuBackgroundColor(B_DARKEN_1_TINT));

	if (minfo.separator == 2) {
		y++;
		frame.left++;
		frame.right--;
		parent->AddLine(BPoint(frame.left,y), BPoint(firstEndX, y),
			ShiftMenuBackgroundColor(B_DARKEN_1_TINT));
		parent->AddLine(BPoint(secondStartX,y), BPoint(frame.right, y),
			ShiftMenuBackgroundColor(B_DARKEN_1_TINT));
	}
	y++;
	if (minfo.separator == 2) {
		frame.left++;
		frame.right--;
	}
	parent->AddLine(BPoint(frame.left, y), BPoint(firstEndX, y),
		ShiftMenuBackgroundColor(B_DARKEN_1_TINT));
	parent->AddLine(BPoint(secondStartX, y), BPoint(frame.right, y),
		ShiftMenuBackgroundColor(B_DARKEN_1_TINT));

	parent->EndLineArray();

	font_height finfo;
	parent->GetFontHeight(&finfo);

	parent->SetLowColor(parent->ViewColor());
	BPoint loc(firstEndX + kStubToStringSlotX,
		ContentLocation().y + finfo.ascent);

	parent->MovePenTo(loc + BPoint(1, 1));
	parent->SetHighColor(ShiftMenuBackgroundColor(B_DARKEN_1_TINT));
	parent->DrawString(truncatedLabel.String());

	parent->MovePenTo(loc);
	parent->SetHighColor(ShiftMenuBackgroundColor(B_DISABLED_LABEL_TINT));
	parent->DrawString(truncatedLabel.String());

	parent->PopState();
}


// #pragma mark - ShortcutFilter


ShortcutFilter::ShortcutFilter(uint32 shortcutKey, uint32 shortcutModifier,
	uint32 shortcutWhat, BHandler* target)
	:
	BMessageFilter(B_KEY_DOWN),
	fShortcutKey(shortcutKey),
	fShortcutModifier(shortcutModifier),
	fShortcutWhat(shortcutWhat),
	fTarget(target)
{
}


filter_result
ShortcutFilter::Filter(BMessage* message, BHandler**)
{
	if (message->what == B_KEY_DOWN) {
		uint32 modifiers;
		uint32 rawKeyChar = 0;
		uint8 byte = 0;
		int32 key = 0;

		if (message->FindInt32("modifiers", (int32*)&modifiers) != B_OK
			|| message->FindInt32("raw_char", (int32*)&rawKeyChar) != B_OK
			|| message->FindInt8("byte", (int8*)&byte) != B_OK
			|| message->FindInt32("key", &key) != B_OK) {
			return B_DISPATCH_MESSAGE;
		}

		modifiers &= B_SHIFT_KEY | B_COMMAND_KEY | B_CONTROL_KEY
			| B_OPTION_KEY | B_MENU_KEY;
			// strip caps lock, etc.

		if (modifiers == fShortcutModifier && rawKeyChar == fShortcutKey) {
			fTarget->Looper()->PostMessage(fShortcutWhat, fTarget);
			return B_SKIP_MESSAGE;
		}
	}

	// let others deal with this
	return B_DISPATCH_MESSAGE;
}


// #pragma mark - BPrivate functions


namespace BPrivate {

void
StringFromStream(BString* string, BMallocIO* stream, bool endianSwap)
{
	int32 length;
	stream->Read(&length, sizeof(length));
	if (endianSwap)
		length = SwapInt32(length);

	if (length < 0 || length > 10000) {
		// TODO: should fail here
		PRINT(("problems instatiating a string, length probably wrong %"
			B_PRId32 "\n", length));
		return;
	}

	char* buffer = string->LockBuffer(length + 1);
	stream->Read(buffer, (size_t)length + 1);
	string->UnlockBuffer(length);
}


void
StringToStream(const BString* string, BMallocIO* stream)
{
	int32 length = string->Length();
	stream->Write(&length, sizeof(int32));
	stream->Write(string->String(), (size_t)string->Length() + 1);
}


int32
ArchiveSize(const BString* string)
{
	return string->Length() + 1 + (ssize_t)sizeof(int32);
}


int32
CountRefs(const BMessage* message)
{
	uint32 type;
	int32 count;
	message->GetInfo("refs", &type, &count);

	return count;
}


static entry_ref*
EachEntryRefCommon(BMessage* message, entry_ref *(*func)(entry_ref*, void*),
	void* passThru, int32 maxCount)
{
	uint32 type;
	int32 count;
	message->GetInfo("refs", &type, &count);

	if (maxCount >= 0 && count > maxCount)
		count = maxCount;

	for (int32 index = 0; index < count; index++) {
		entry_ref ref;
		message->FindRef("refs", index, &ref);
		entry_ref* newRef = (func)(&ref, passThru);
		if (newRef != NULL)
			return newRef;
	}

	return NULL;
}


bool
ContainsEntryRef(const BMessage* message, const entry_ref* ref)
{
	entry_ref match;
	for (int32 index = 0; (message->FindRef("refs", index, &match) == B_OK);
			index++) {
		if (*ref == match)
			return true;
	}

	return false;
}


entry_ref*
EachEntryRef(BMessage* message, entry_ref* (*func)(entry_ref*, void*),
	void* passThru)
{
	return EachEntryRefCommon(message, func, passThru, -1);
}

typedef entry_ref *(*EachEntryIteratee)(entry_ref *, void *);


const entry_ref*
EachEntryRef(const BMessage* message,
	const entry_ref* (*func)(const entry_ref*, void*), void* passThru)
{
	return EachEntryRefCommon(const_cast<BMessage*>(message),
		(EachEntryIteratee)func, passThru, -1);
}


entry_ref*
EachEntryRef(BMessage* message, entry_ref* (*func)(entry_ref*, void*),
	void* passThru, int32 maxCount)
{
	return EachEntryRefCommon(message, func, passThru, maxCount);
}


const entry_ref *
EachEntryRef(const BMessage* message,
	const entry_ref *(*func)(const entry_ref *, void *), void* passThru,
	int32 maxCount)
{
	return EachEntryRefCommon(const_cast<BMessage *>(message),
		(EachEntryIteratee)func, passThru, maxCount);
}


void
TruncateLeaf(BString* string)
{
	for (int32 index = string->Length(); index >= 0; index--) {
		if ((*string)[index] == '/') {
			string->Truncate(index + 1);
			return;
		}
	}
}


int64
StringToScalar(const char* text)
{
	char* end;
	int64 val;

	char* buffer = new char [strlen(text) + 1];
	strcpy(buffer, text);

	if (strstr(buffer, "k") || strstr(buffer, "K")) {
		val = strtoll(buffer, &end, 10);
		val *= kKBSize;
	} else if (strstr(buffer, "mb") || strstr(buffer, "MB")) {
		val = strtoll(buffer, &end, 10);
		val *= kMBSize;
	} else if (strstr(buffer, "gb") || strstr(buffer, "GB")) {
		val = strtoll(buffer, &end, 10);
		val *= kGBSize;
	} else if (strstr(buffer, "byte") || strstr(buffer, "BYTE")) {
		val = strtoll(buffer, &end, 10);
		val *= kGBSize;
	} else {
		// no suffix, try plain byte conversion
		val = strtoll(buffer, &end, 10);
	}
	delete[] buffer;

	return val;
}


int32
ListIconSize()
{
	static int32 sIconSize = be_control_look->ComposeIconSize(B_MINI_ICON)
		.IntegerWidth() + 1;
	return sIconSize;
}


static BRect
LineBounds(BPoint where, float length, bool vertical)
{
	BRect rect;
	rect.SetLeftTop(where);
	rect.SetRightBottom(where + BPoint(2, 2));
	if (vertical)
		rect.bottom = rect.top + length;
	else
		rect.right = rect.left + length;

	return rect;
}


SeparatorLine::SeparatorLine(BPoint where, float length, bool vertical,
	const char* name)
	:
	BView(LineBounds(where, length, vertical), name,
		B_FOLLOW_LEFT | B_FOLLOW_TOP, B_WILL_DRAW)
{
	SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	SetLowUIColor(B_PANEL_BACKGROUND_COLOR);
}


void
SeparatorLine::Draw(BRect)
{
	BRect bounds(Bounds());
	rgb_color hiliteColor = tint_color(ViewColor(), 1.5f);

	bool vertical = (bounds.left > bounds.right - 3);
	BeginLineArray(2);
	if (vertical) {
		AddLine(bounds.LeftTop(), bounds.LeftBottom(), hiliteColor);
		AddLine(bounds.LeftTop() + BPoint(1, 0),
			bounds.LeftBottom() + BPoint(1, 0), kWhite);
	} else {
		AddLine(bounds.LeftTop(), bounds.RightTop(), hiliteColor);
		AddLine(bounds.LeftTop() + BPoint(0, 1),
			bounds.RightTop() + BPoint(0, 1), kWhite);
	}
	EndLineArray();
}


void
HexDump(const void* buf, int32 length)
{
	const int32 kBytesPerLine = 16;
	int32 offset;
	unsigned char* buffer = (unsigned char*)buf;

	for (offset = 0; ; offset += kBytesPerLine, buffer += kBytesPerLine) {
		int32 remain = length;
		int32 index;

		printf( "0x%06x: ", (int)offset);

		for (index = 0; index < kBytesPerLine; index++) {
			if (remain-- > 0)
				printf("%02x%c", buffer[index], remain > 0 ? ',' : ' ');
			else
				printf("   ");
		}

		remain = length;
		printf(" \'");
		for (index = 0; index < kBytesPerLine; index++) {
			if (remain-- > 0)
				printf("%c", buffer[index] > ' ' ? buffer[index] : '.');
			else
				printf(" ");
		}
		printf("\'\n");

		length -= kBytesPerLine;
		if (length <= 0)
			break;
	}
	fflush(stdout);
}


int
CompareLabels(const BMenuItem* item1, const BMenuItem* item2)
{
	return strcasecmp(item1->Label(), item2->Label());
}


void
EnableNamedMenuItem(BMenu* menu, const char* itemName, bool on)
{
	BMenuItem* item = menu->FindItem(itemName);
	if (item != NULL)
		item->SetEnabled(on);
}


void
MarkNamedMenuItem(BMenu* menu, const char* itemName, bool on)
{
	BMenuItem* item = menu->FindItem(itemName);
	if (item != NULL)
		item->SetMarked(on);
}


void
EnableNamedMenuItem(BMenu* menu, uint32 commandName, bool on)
{
	BMenuItem* item = menu->FindItem(commandName);
	if (item != NULL)
		item->SetEnabled(on);
}


void
MarkNamedMenuItem(BMenu* menu, uint32 commandName, bool on)
{
	BMenuItem* item = menu->FindItem(commandName);
	if (item != NULL)
		item->SetMarked(on);
}


void
DeleteSubmenu(BMenuItem* submenuItem)
{
	if (submenuItem == NULL)
		return;

	BMenu* submenu = submenuItem->Submenu();
	if (submenu == NULL)
		return;

	// delete all submenu items
	submenu->RemoveItems(0, submenu->CountItems(), true);
}


status_t
GetAppSignatureFromAttr(BFile* file, char* attr)
{
	// This call is a performance improvement that
	// avoids using the BAppFileInfo API when retrieving the
	// app signature -- the call is expensive because by default
	// the resource fork is scanned to read the attribute

#ifdef B_APP_FILE_INFO_IS_FAST
	BAppFileInfo appFileInfo(file);
	return appFileInfo.GetSignature(attr);
#else
	ssize_t readResult = file->ReadAttr(kAttrAppSignature, B_MIME_STRING_TYPE,
		0, attr, B_MIME_TYPE_LENGTH);

	if (readResult <= 0)
		return (status_t)readResult;

	return B_OK;
#endif // B_APP_FILE_INFO_IS_FAST
}


status_t
GetAppIconFromAttr(BFile* file, BBitmap* icon, icon_size which)
{
	// This call is a performance improvement that
	// avoids using the BAppFileInfo API when retrieving the
	// app icons -- the call is expensive because by default
	// the resource fork is scanned to read the icons

//#ifdef B_APP_FILE_INFO_IS_FAST
	BAppFileInfo appFileInfo(file);
	return appFileInfo.GetIcon(icon, which);
//#else
//
//	const char* attrName = kAttrIcon;
//	uint32 type = B_VECTOR_ICON_TYPE;
//
//	// try vector icon
//	attr_info ainfo;
//	status_t result = file->GetAttrInfo(attrName, &ainfo);
//
//	if (result == B_OK) {
//		uint8 buffer[ainfo.size];
//		ssize_t readResult = file->ReadAttr(attrName, type, 0, buffer,
//			ainfo.size);
//		if (readResult == ainfo.size) {
//			if (BIconUtils::GetVectorIcon(buffer, ainfo.size, icon) == B_OK)
//				return B_OK;
//		}
//	}
//
//	// try again with R5 icons
//	attrName = which == B_LARGE_ICON ? kAttrLargeIcon : kAttrMiniIcon;
//	type = which == B_LARGE_ICON ? LARGE_ICON_TYPE : MINI_ICON_TYPE;
//
//	result = file->GetAttrInfo(attrName, &ainfo);
//	if (result < B_OK)
//		return result;
//
//	uint8 buffer[ainfo.size];
//
//	ssize_t readResult = file->ReadAttr(attrName, type, 0, buffer, ainfo.size);
//	if (readResult <= 0)
//		return (status_t)readResult;
//
//	if (icon->ColorSpace() != B_CMAP8)
//		result = BIconUtils::ConvertFromCMAP8(buffer, which, which, which, icon);
//	else
//		icon->SetBits(buffer, icon->BitsLength(), 0, B_CMAP8);
//
//	return result;
//#endif	// B_APP_FILE_INFO_IS_FAST
}


status_t
GetFileIconFromAttr(BNode* node, BBitmap* icon, icon_size which)
{
	// get icon from the node info
	BNodeInfo nodeInfo(node);
	return nodeInfo.GetIcon(icon, which);
}


//	#pragma mark - PrintToStream


void
PrintToStream(rgb_color color)
{
	printf("r:%x, g:%x, b:%x, a:%x\n",
		color.red, color.green, color.blue, color.alpha);
}


//	#pragma mark - EachMenuItem


extern BMenuItem*
EachMenuItem(BMenu* menu, bool recursive, BMenuItem* (*func)(BMenuItem *))
{
	int32 count = menu->CountItems();
	for (int32 index = 0; index < count; index++) {
		BMenuItem* item = menu->ItemAt(index);
		BMenuItem* newItem = (func)(item);
		if (newItem != NULL)
			return newItem;

		if (recursive) {
			BMenu* submenu = menu->SubmenuAt(index);
			if (submenu != NULL)
				return EachMenuItem(submenu, true, func);
		}
	}

	return NULL;
}


extern const BMenuItem*
EachMenuItem(const BMenu* menu, bool recursive,
	BMenuItem* (*func)(const BMenuItem *))
{
	int32 count = menu->CountItems();
	for (int32 index = 0; index < count; index++) {
		BMenuItem* item = menu->ItemAt(index);
		BMenuItem* newItem = (func)(item);
		if (newItem != NULL)
			return newItem;

		if (recursive) {
			BMenu* submenu = menu->SubmenuAt(index);
			if (submenu != NULL)
				return EachMenuItem(submenu, true, func);
		}
	}

	return NULL;
}


//	#pragma mark - PositionPassingMenuItem


PositionPassingMenuItem::PositionPassingMenuItem(const char* title,
	BMessage* message, char shortcut, uint32 modifiers)
	:
	BMenuItem(title, message, shortcut, modifiers)
{
}


PositionPassingMenuItem::PositionPassingMenuItem(BMenu* menu, BMessage* message)
	:
	BMenuItem(menu, message)
{
}


PositionPassingMenuItem::PositionPassingMenuItem(BMessage* data)
	:
	BMenuItem(data)
{
}


BArchivable*
PositionPassingMenuItem::Instantiate(BMessage* data)
{
	if (validate_instantiation(data, "PositionPassingMenuItem"))
		return new PositionPassingMenuItem(data);

	return NULL;
}


status_t
PositionPassingMenuItem::Invoke(BMessage* message)
{
	if (Menu() == NULL)
		return B_ERROR;

	if (!IsEnabled())
		return B_ERROR;

	if (message == NULL)
		message = Message();

	if (message == NULL)
		return B_BAD_VALUE;

	BMessage clone(*message);
	clone.AddInt32("index", Menu()->IndexOf(this));
	clone.AddInt64("when", system_time());
	clone.AddPointer("source", this);

	// embed the invoke location of the menu so that we can create
	// a new folder, etc. on the spot
	BMenu* menu = Menu();

	for (;;) {
		if (!menu->Supermenu())
			break;

		menu = menu->Supermenu();
	}

	// use the window position only, if the item was invoked from the menu
	// menu->Window() points to the window the item was invoked from
	if (dynamic_cast<BContainerWindow*>(menu->Window()) == NULL) {
		AutoLocker<BLooper> lock(menu->Looper());
		if (lock.IsLocked()) {
			BPoint invokeOrigin(menu->Window()->Frame().LeftTop());
			clone.AddPoint("be:invoke_origin", invokeOrigin);
		}
	}

	return BInvoker::Invoke(&clone);
}


//	#pragma mark - BPrivate functions



float
ComputeTypeAheadScore(const char* text, const char* match, bool wordMode)
{
	// highest score: exact match
	const char* found = strcasestr(text, match);
	if (found != NULL) {
		if (found == text)
			return kExactMatchScore;

		return 1.f / (found - text);
	}

	// there was no exact match

	// second best: all characters at word beginnings
	if (wordMode) {
		float score = 0;
		for (int32 j = 0, k = 0; match[j]; j++) {
			while (text[k]
				&& tolower(text[k]) != tolower(match[j])) {
				k++;
			}
			if (text[k] == '\0') {
				score = 0;
				break;
			}

			bool wordStart = k == 0 || isspace(text[k - 1]);
			if (wordStart)
				score++;
			if (j > 0) {
				bool wordEnd = !text[k + 1] || isspace(text[k + 1]);
				if (wordEnd)
					score += 0.3;
				if (match[j - 1] == text[k - 1])
					score += 0.7;
			}

			score += 1.f / (k + 1);
			k++;
		}

		return score;
	}

	return -1;
}


//	#pragma mark - throw on error functions.


void
_ThrowOnError(status_t result, const char* DEBUG_ONLY(file),
	int32 DEBUG_ONLY(line))
{
	if (result != B_OK) {
		PRINT(("%s at %s:%d\n", strerror(result), file, (int)line));
		throw result;
	}
}


void
_ThrowIfNotSize(ssize_t size, const char* DEBUG_ONLY(file),
	int32 DEBUG_ONLY(line))
{
	if (size < B_OK) {
		PRINT(("%s at %s:%d\n", strerror((status_t)size), file, (int)line));
		throw (status_t)size;
	}
}


void
_ThrowOnAssert(bool success, const char* DEBUG_ONLY(file),
	int32 DEBUG_ONLY(line))
{
	if (!success) {
		PRINT(("Assert failed at %s:%d\n", file, (int)line));
		throw B_ERROR;
	}
}

} // namespace BPrivate
