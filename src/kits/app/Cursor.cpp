/*
 * Copyright 2001-2006, Haiku.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Frans van Nispen (xlr8@tref.nl)
 *		Gabe Yoder (gyoder@stny.rr.com)
 *		Axel Dörfler, axeld@pinc-software.de
 */

/**	BCursor describes a view-wide or application-wide cursor. */


#include <AppDefs.h>
#include <Bitmap.h>
#include <Cursor.h>

#include <vector>

#include <CosmoeBackendAPI.h>
#include <ServerProtocol.h>


const BCursor *B_CURSOR_SYSTEM_DEFAULT;
const BCursor *B_CURSOR_I_BEAM;
	// these are initialized in BApplication::InitData()


static int32
create_custom_cursor_from_legacy_data(const uint8* data)
{
	const int32 kSize = 16;
	const int32 kRowBytes = kSize * 4;
	const int32 kImageMaskOffset = 4;
	const int32 kTransparencyMaskOffset = 36;

	if (data == NULL)
		return B_BAD_VALUE;

	std::vector<uint8> pixels((size_t)kSize * (size_t)kRowBytes, 0);

	for (int32 y = 0; y < kSize; y++) {
		uint16 imageMask = ((uint16)data[kImageMaskOffset + y * 2] << 8)
			| (uint16)data[kImageMaskOffset + y * 2 + 1];
		uint16 transparencyMask = ((uint16)data[kTransparencyMaskOffset + y * 2] << 8)
			| (uint16)data[kTransparencyMaskOffset + y * 2 + 1];

		for (int32 x = 0; x < kSize; x++) {
			uint16 bit = (uint16)(1 << (15 - x));
			uint8* dst = &pixels[(size_t)y * (size_t)kRowBytes + (size_t)x * 4];

			if ((transparencyMask & bit) == 0) {
				// Fully transparent pixel.
				dst[0] = 0;
				dst[1] = 0;
				dst[2] = 0;
				dst[3] = 0;
				continue;
			}

			// Opaque black/white pixel from image mask.
			bool black = (imageMask & bit) != 0;
			uint8 color = black ? 0 : 255;
			dst[0] = color;
			dst[1] = color;
			dst[2] = color;
			dst[3] = 255;
		}
	}

	return cosmoe_display_create_custom_cursor(pixels.data(), pixels.size(),
		kSize, kSize, kRowBytes, (int32)B_RGBA32,
		(int32)data[2], (int32)data[3]);
}

BCursor::BCursor(const void *cursorData)
	:
	fServerToken(-1),
	fNeedToFree(false)
{
	const uint8 *data = (const uint8 *)cursorData;

	if (data == B_HAND_CURSOR || data == B_I_BEAM_CURSOR) {
		// just use the default cursors from the app_server
		fServerToken = data == B_HAND_CURSOR ?
			B_CURSOR_ID_SYSTEM_DEFAULT : B_CURSOR_ID_I_BEAM;
		return;
	}

	// Create a new cursor in the app_server

	if (data == NULL
		|| data[0] != 16	// size
		|| data[1] != 1		// depth
		|| data[2] >= 16 || data[3] >= 16)	// hot-spot
		return;

	int32 cursorToken = create_custom_cursor_from_legacy_data(data);
	if (cursorToken < 0)
		return;

	fServerToken = cursorToken;
	fNeedToFree = true;
}


BCursor::BCursor(BCursorID id)
	:
	fServerToken(id),
	fNeedToFree(false)
{
}


BCursor::BCursor(const BCursor& other)
	:
	fServerToken(-1),
	fNeedToFree(false)
{
	*this = other;
}


BCursor::BCursor(BMessage *data)
{
	// undefined on BeOS
	fServerToken = -1;
	fNeedToFree = false;
}


BCursor::BCursor(const BBitmap* bitmap, const BPoint& hotspot)
	:
	fServerToken(-1),
	fNeedToFree(false)
{
	if (bitmap == NULL)
		return;

	BRect bounds = bitmap->Bounds();
	int32 width = (int32)bounds.IntegerWidth() + 1;
	int32 height = (int32)bounds.IntegerHeight() + 1;
	color_space colorspace = bitmap->ColorSpace();
	const uint8* bits = (const uint8*)bitmap->Bits();
	int32 size = bitmap->BitsLength();
	if (bits == NULL || size <= 0 || width <= 0 || height <= 0)
		return;

	int32 hotX = (int32)hotspot.x;
	int32 hotY = (int32)hotspot.y;
	if (hotX < 0 || hotY < 0 || hotX >= width || hotY >= height)
		return;

	int32 cursorToken = cosmoe_display_create_custom_cursor(bits,
		(size_t)size, width, height, bitmap->BytesPerRow(), (int32)colorspace,
		hotX, hotY);
	if (cursorToken < 0)
		return;

	fServerToken = cursorToken;
	fNeedToFree = true;
}


BCursor::~BCursor()
{
	_FreeCursorData();
}


status_t
BCursor::InitCheck() const
{
	return fServerToken >= 0 ? B_OK : fServerToken;
}


status_t
BCursor::Archive(BMessage *into, bool deep) const
{
	// not implemented on BeOS
	return B_OK;
}


BArchivable	*
BCursor::Instantiate(BMessage *data)
{
	// not implemented on BeOS
	return NULL;
}


BCursor&
BCursor::operator=(const BCursor& other)
{
	if (&other != this && other != *this) {
		_FreeCursorData();

		fServerToken = other.fServerToken;

		if (other.fNeedToFree) {
			// BPrivate::AppServerLink link;
			// link.StartMessage(AS_CLONE_CURSOR);
			// link.Attach<int32>(other.fServerToken);

			// status_t status;
			// if (link.FlushWithReply(status) == B_OK) {
			// 	if (status == B_OK) {
			// 		link.Read<int32>(&fServerToken);
			// 		fNeedToFree = true;
			// 	} else
			// 		fServerToken = status;
			// }
		}
	}
	return *this;
}


bool
BCursor::operator==(const BCursor& other) const
{
	return fServerToken == other.fServerToken;
}


bool
BCursor::operator!=(const BCursor& other) const
{
	return fServerToken != other.fServerToken;
}


status_t
BCursor::Perform(perform_code d, void *arg)
{
	return B_OK;
}


void BCursor::_ReservedCursor1() {}
void BCursor::_ReservedCursor2() {}
void BCursor::_ReservedCursor3() {}
void BCursor::_ReservedCursor4() {}


void
BCursor::_FreeCursorData()
{
	// Notify server to deallocate server-side objects for this cursor
	if (fNeedToFree) {
		cosmoe_display_delete_custom_cursor(fServerToken);
		fNeedToFree = false;
	}
}
