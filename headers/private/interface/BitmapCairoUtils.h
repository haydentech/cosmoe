#ifndef _BITMAP_CAIRO_UTILS_H
#define _BITMAP_CAIRO_UTILS_H

#include <GraphicsDefs.h>
#include <SupportDefs.h>
#include <stdlib.h>
#include <string.h>

#include <cairo.h>

#if (defined(__i386__) || defined(__x86_64__)) && defined(__SSE2__)
#include <emmintrin.h>
#endif


static inline void
premultiply_rgba_row_scalar(const uint8* srcRow, uint8* dstRow, int32 width)
{
	for (int32 x = 0; x < width; x++) {
		uint8 b = srcRow[x * 4 + 0];
		uint8 g = srcRow[x * 4 + 1];
		uint8 r = srcRow[x * 4 + 2];
		uint8 a = srcRow[x * 4 + 3];

		dstRow[x * 4 + 0] = (uint8)((b * a + 127) / 255);
		dstRow[x * 4 + 1] = (uint8)((g * a + 127) / 255);
		dstRow[x * 4 + 2] = (uint8)((r * a + 127) / 255);
		dstRow[x * 4 + 3] = a;
	}
}


static inline void
force_opaque_rgba_row_scalar(const uint8* srcRow, uint8* dstRow, int32 width)
{
	for (int32 x = 0; x < width; x++) {
		dstRow[x * 4 + 0] = srcRow[x * 4 + 0];
		dstRow[x * 4 + 1] = srcRow[x * 4 + 1];
		dstRow[x * 4 + 2] = srcRow[x * 4 + 2];
		dstRow[x * 4 + 3] = 255;
	}
}


static inline void
unpremultiply_argb32_row_to_opaque_scalar(const uint8* srcRow, uint8* dstRow,
	int32 width)
{
	for (int32 x = 0; x < width; x++) {
		uint8 b = srcRow[x * 4 + 0];
		uint8 g = srcRow[x * 4 + 1];
		uint8 r = srcRow[x * 4 + 2];
		uint8 a = srcRow[x * 4 + 3];

		if (a == 0) {
			dstRow[x * 4 + 0] = 0;
			dstRow[x * 4 + 1] = 0;
			dstRow[x * 4 + 2] = 0;
		} else {
			dstRow[x * 4 + 0] = (uint8)((b * 255 + a / 2) / a);
			dstRow[x * 4 + 1] = (uint8)((g * 255 + a / 2) / a);
			dstRow[x * 4 + 2] = (uint8)((r * 255 + a / 2) / a);
		}
		dstRow[x * 4 + 3] = 255;
	}
}


#if (defined(__i386__) || defined(__x86_64__)) && defined(__SSE2__)
static inline __m128i
premultiply_div255_epi16(__m128i value)
{
	__m128i bias = _mm_set1_epi16(128);
	__m128i t = _mm_add_epi16(value, bias);
	__m128i tShift = _mm_srli_epi16(t, 8);
	t = _mm_add_epi16(t, tShift);
	return _mm_srli_epi16(t, 8);
}


static inline void
premultiply_rgba_row_sse2(const uint8* srcRow, uint8* dstRow, int32 width)
{
	const __m128i zero = _mm_setzero_si128();
	const __m128i alphaMask = _mm_set_epi16(0xFFFF, 0, 0, 0, 0xFFFF, 0, 0, 0);

	int32 x = 0;
	for (; x + 4 <= width; x += 4) {
		__m128i pixels = _mm_loadu_si128((const __m128i*)(srcRow + x * 4));

		__m128i low = _mm_unpacklo_epi8(pixels, zero);
		__m128i high = _mm_unpackhi_epi8(pixels, zero);

		__m128i alphaLow = _mm_shufflelo_epi16(low, _MM_SHUFFLE(3, 3, 3, 3));
		alphaLow = _mm_shufflehi_epi16(alphaLow, _MM_SHUFFLE(3, 3, 3, 3));
		__m128i alphaHigh = _mm_shufflelo_epi16(high, _MM_SHUFFLE(3, 3, 3, 3));
		alphaHigh = _mm_shufflehi_epi16(alphaHigh, _MM_SHUFFLE(3, 3, 3, 3));

		__m128i mulLow = _mm_mullo_epi16(low, alphaLow);
		__m128i mulHigh = _mm_mullo_epi16(high, alphaHigh);

		__m128i premulLow = premultiply_div255_epi16(mulLow);
		__m128i premulHigh = premultiply_div255_epi16(mulHigh);

		premulLow = _mm_or_si128(_mm_and_si128(low, alphaMask),
			_mm_andnot_si128(alphaMask, premulLow));
		premulHigh = _mm_or_si128(_mm_and_si128(high, alphaMask),
			_mm_andnot_si128(alphaMask, premulHigh));

		__m128i result = _mm_packus_epi16(premulLow, premulHigh);
		_mm_storeu_si128((__m128i*)(dstRow + x * 4), result);
	}

	if (x < width)
		premultiply_rgba_row_scalar(srcRow + x * 4, dstRow + x * 4, width - x);
}
#endif


// Cairo expects premultiplied ARGB32 data for transparency, but BBitmap with B_RGBA32 color space is non-premultiplied.

static inline bool
prepare_bitmap_bits_for_cairo_argb32(const uint8* sourceBits,
	cairo_format_t format, color_space sourceColorSpace, int32 width,
	int32 height, int32 stride, const uint8** outBits,
	uint8** outOwnedPremultipliedBits, bool forceOpaque = false)
{
	if (outBits == NULL || outOwnedPremultipliedBits == NULL)
		return false;

	*outBits = sourceBits;
	*outOwnedPremultipliedBits = NULL;

	if (sourceBits == NULL)
		return false;

	if (format != CAIRO_FORMAT_ARGB32 || sourceColorSpace != B_RGBA32)
		return true;

	if (width <= 0 || height <= 0 || stride <= 0)
		return false;

	int32 bufferSize = stride * height;
	uint8* premultipliedBits = (uint8*)malloc(bufferSize);
	if (premultipliedBits == NULL)
		return false;

	for (int32 y = 0; y < height; y++) {
		const uint8* srcRow = sourceBits + y * stride;
		uint8* dstRow = premultipliedBits + y * stride;
		if (forceOpaque) {
			force_opaque_rgba_row_scalar(srcRow, dstRow, width);
			if (stride > width * 4)
				memcpy(dstRow + width * 4, srcRow + width * 4, stride - width * 4);
		} else {
			memcpy(dstRow, srcRow, stride);
#if (defined(__i386__) || defined(__x86_64__)) && defined(__SSE2__)
			premultiply_rgba_row_sse2(srcRow, dstRow, width);
#else
			premultiply_rgba_row_scalar(srcRow, dstRow, width);
#endif
		}
	}

	*outBits = premultipliedBits;
	*outOwnedPremultipliedBits = premultipliedBits;
	return true;
}


static inline cairo_surface_t*
create_opaque_copy_surface_from_cairo_surface(cairo_surface_t* sourceSurface,
	int32 width, int32 height, const uint8* opaqueFallbackBits = NULL,
	int32 fallbackStride = 0)
{
	if (sourceSurface == NULL || width <= 0 || height <= 0)
		return NULL;

	cairo_surface_t* opaqueSurface = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, width, height);
	if (opaqueSurface == NULL
		|| cairo_surface_status(opaqueSurface) != CAIRO_STATUS_SUCCESS) {
		if (opaqueSurface != NULL)
			cairo_surface_destroy(opaqueSurface);
		return NULL;
	}

	cairo_t* cr = cairo_create(opaqueSurface);
	if (cr == NULL || cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
		if (cr != NULL)
			cairo_destroy(cr);
		cairo_surface_destroy(opaqueSurface);
		return NULL;
	}

	if (opaqueFallbackBits != NULL && fallbackStride >= width * 4) {
		uint8* data = cairo_image_surface_get_data(opaqueSurface);
		int32 stride = cairo_image_surface_get_stride(opaqueSurface);
		for (int32 y = 0; y < height; y++) {
			const uint8* sourceRow = opaqueFallbackBits + y * fallbackStride;
			uint8* destinationRow = data + y * stride;
			force_opaque_rgba_row_scalar(sourceRow, destinationRow, width);
		}
		cairo_surface_mark_dirty(opaqueSurface);
		cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	} else {
		cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	}
	cairo_set_source_surface(cr, sourceSurface, 0.0, 0.0);
	cairo_paint(cr);
	cairo_destroy(cr);

	cairo_surface_flush(opaqueSurface);
	uint8* data = cairo_image_surface_get_data(opaqueSurface);
	int32 stride = cairo_image_surface_get_stride(opaqueSurface);
	if (data == NULL || stride <= 0) {
		cairo_surface_destroy(opaqueSurface);
		return NULL;
	}

	for (int32 y = 0; y < height; y++) {
		uint8* row = data + y * stride;
		unpremultiply_argb32_row_to_opaque_scalar(row, row, width);
	}

	cairo_surface_mark_dirty(opaqueSurface);
	return opaqueSurface;
}


#endif
