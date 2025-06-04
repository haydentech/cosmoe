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

} // namespace BPrivate
