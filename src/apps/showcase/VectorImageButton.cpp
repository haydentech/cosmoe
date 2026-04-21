/*
 * Copyright 2010 Stephan Aßmus <superstippi@gmx.de>. All rights reserved.
 * Distributed under the terms of the MIT License.
 */

#include "VectorImageButton.h"

#include <string.h>

#include <Bitmap.h>
#include <ControlLook.h>
#include <TranslationUtils.h>
#include <Size.h>
#include <IconUtils.h>


static const float kFrameInset = 2;
static const float kRasterMinFactor = 1.5f;
static const float kRasterTargetFactor = 2.0f;
static const float kRasterMaxFactor = 2.5f;


BVectorImageButton::BVectorImageButton(const char* resourceName, BSize imageSize, BMessage* message)
	:
	BButton("", message),
	fBitmap(NULL),
	fBackgroundMode(BUTTON_BACKGROUND),
	fImageSize(imageSize),
	fRasterizedSize(imageSize),
	fAutoscale(false),
	fResourceName(resourceName),
	fInitStatus(B_NO_INIT)
{
	fInitStatus = _LoadBitmap(resourceName, imageSize);
}


BVectorImageButton::BVectorImageButton(const char* resourceName,
	BMessage* message)
	:
	BVectorImageButton(resourceName, BSize(32, 32), message)
{
}


status_t
BVectorImageButton::LoadBitmap(const char* resourceName)
{
	return LoadBitmap(resourceName, fImageSize);
}


status_t
BVectorImageButton::LoadBitmap(const char* resourceName, BSize imageSize)
{
	status_t status = _LoadBitmap(resourceName, imageSize);
	fInitStatus = status;
	if (status == B_OK)
		Invalidate();

	return status;
}


status_t
BVectorImageButton::InitCheck() const
{
	return fInitStatus;
}



BVectorImageButton::~BVectorImageButton()
{
	delete fBitmap;
}


BSize
BVectorImageButton::MinSize()
{
	BSize min(0, 0);
	if (fBitmap) {
		min.width = fImageSize.Width();
		min.height = fImageSize.Height();
	}
	min.width += kFrameInset * 2;
	min.height += kFrameInset * 2;
	return min;
}


BSize
BVectorImageButton::MaxSize()
{
	return BSize(B_SIZE_UNLIMITED, B_SIZE_UNLIMITED);
}


BSize
BVectorImageButton::PreferredSize()
{
	return MinSize();
}


void
BVectorImageButton::Draw(BRect updateRect)
{
	BRect bounds(Bounds());
	rgb_color base = ui_color(B_PANEL_BACKGROUND_COLOR);
	uint32 flags = be_control_look->Flags(this);

	if (fBackgroundMode != NO_BACKGROUND) {
		if (fBackgroundMode == BUTTON_BACKGROUND || Value() == B_CONTROL_ON) {
			be_control_look->DrawButtonBackground(this, bounds, updateRect, base,
				flags);
		} else {
			SetHighColor(tint_color(base, B_DARKEN_2_TINT));
			StrokeLine(bounds.LeftBottom(), bounds.RightBottom());
			bounds.bottom--;
			be_control_look->DrawMenuBarBackground(this, bounds, updateRect, base,
				flags);
		}
	}

	if (fBitmap == NULL)
		return;

	SetDrawingMode(B_OP_ALPHA);

	if (!IsEnabled()) {
		SetBlendingMode(B_CONSTANT_ALPHA, B_ALPHA_OVERLAY);
		SetHighColor(0, 0, 0, 120);
	}

	BRect bitmapBounds(fBitmap->Bounds());
	BRect contentRect(bounds);
	contentRect.InsetBy(kFrameInset, kFrameInset);

	if (!contentRect.IsValid())
		return;

	float bitmapWidth = bitmapBounds.Width() + 1;
	float bitmapHeight = bitmapBounds.Height() + 1;
	float sourceWidth = bitmapWidth;
	float sourceHeight = bitmapHeight;
	float maxWidth = contentRect.Width() + 1;
	float maxHeight = contentRect.Height() + 1;

	float drawWidth = sourceWidth;
	float drawHeight = sourceHeight;

	if (fAutoscale) {
		// Autoscale should work in logical size, not backing bitmap pixels.
		if (fImageSize.Width() > 0)
			sourceWidth = fImageSize.Width();
		if (fImageSize.Height() > 0)
			sourceHeight = fImageSize.Height();

		float scaleX = maxWidth / sourceWidth;
		float scaleY = maxHeight / sourceHeight;
		float scale = scaleX < scaleY ? scaleX : scaleY;

		drawWidth = sourceWidth * scale;
		drawHeight = sourceHeight * scale;

		// Keep rasterization close to current target size. Quality actually decreases if the bitmap
		// gets too large and we have to scale down excessively, especially with line art icons.
		// Since _LoadBitmap() renders at 2x the logical size, keep the backing bitmap size clamped
		// between 1.5x and 2.5x the displayed size when in autoscale mode.
		float currentRasterWidth = fRasterizedSize.Width();
		float currentRasterHeight = fRasterizedSize.Height();
		float minRasterWidth = drawWidth * kRasterMinFactor;
		float minRasterHeight = drawHeight * kRasterMinFactor;
		float maxRasterWidth = drawWidth * kRasterMaxFactor;
		float maxRasterHeight = drawHeight * kRasterMaxFactor;

		bool needsRerasterize = currentRasterWidth < minRasterWidth
			|| currentRasterHeight < minRasterHeight
			|| currentRasterWidth > maxRasterWidth
			|| currentRasterHeight > maxRasterHeight;

		if (needsRerasterize && !fResourceName.IsEmpty()) {
			float desiredRasterWidth = ceilf(drawWidth * kRasterTargetFactor);
			float desiredRasterHeight = ceilf(drawHeight * kRasterTargetFactor);
			if (desiredRasterWidth < 1)
				desiredRasterWidth = 1;
			if (desiredRasterHeight < 1)
				desiredRasterHeight = 1;

			if (_LoadBitmap(fResourceName.String(),
					BSize(desiredRasterWidth, desiredRasterHeight), false) == B_OK) {
				bitmapBounds = fBitmap->Bounds();
			}
		}
	} else {
		drawWidth = fImageSize.Width();
		drawHeight = fImageSize.Height();

		if (drawWidth <= 0)
			drawWidth = sourceWidth;
		if (drawHeight <= 0)
			drawHeight = sourceHeight;

		if (drawWidth > maxWidth)
			drawWidth = maxWidth;
		if (drawHeight > maxHeight)
			drawHeight = maxHeight;
	}

	BRect targetRect;
	targetRect.left = floorf(contentRect.left
		+ (maxWidth - drawWidth) / 2 + 0.5f);
	targetRect.top = floorf(contentRect.top
		+ (maxHeight - drawHeight) / 2 + 0.5f);
	targetRect.right = targetRect.left + drawWidth - 1;
	targetRect.bottom = targetRect.top + drawHeight - 1;

	DrawBitmap(fBitmap, bitmapBounds, targetRect, B_FILTER_BITMAP_BILINEAR);
}


void
BVectorImageButton::SetBackgroundMode(uint32 mode)
{
	if (fBackgroundMode != mode) {
		fBackgroundMode = mode;
		Invalidate();
	}
}


void
BVectorImageButton::SetAutoscale(bool autoscale)
{
	if (fAutoscale != autoscale) {
		fAutoscale = autoscale;
		Invalidate();
	}
}


status_t
BVectorImageButton::_LoadBitmap(const char* resourceName, BSize imageSize,
	bool updateImageSize)
{
	if (resourceName == NULL)
		return B_BAD_VALUE;

	if (imageSize.Width() <= 0 || imageSize.Height() <= 0)
		return B_BAD_VALUE;

	BBitmap* bitmap = new(std::nothrow) BBitmap(BRect(0, 0,
		(imageSize.Width() * 2) - 1, (imageSize.Height() * 2) - 1),
		0, B_RGBA32);
	if (bitmap == NULL)
		return B_NO_MEMORY;

	status_t iconStatus = B_ERROR;
#if !defined(__HAIKU__)
	iconStatus = BIconUtils::GetAppIcon(resourceName, B_LARGE_ICON, bitmap);
#else
	iconStatus = GetAppIcon(resourceName, B_LARGE_ICON, bitmap);
#endif
	if (iconStatus != B_OK) {
		delete bitmap;
		return iconStatus;
	}

	delete fBitmap;
	fBitmap = bitmap;
	fRasterizedSize = imageSize;
	if (updateImageSize)
		fImageSize = imageSize;
	fResourceName = resourceName;
	
	return B_OK;
}

