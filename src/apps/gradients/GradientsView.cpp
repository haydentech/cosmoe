/*
 * Copyright (c) 2008-2009, Haiku, Inc.
 * Distributed under the terms of the MIT license.
 *
 * Authors:
 *		Artur Wyszynski <harakash@gmail.com>
 */


#include "GradientsView.h"
#include <GradientLinear.h>
#include <GradientRadial.h>
#include <GradientRadialFocus.h>
#include <GradientDiamond.h>
#include <GradientConic.h>


GradientsView::GradientsView(const BRect &rect)
	: BView(rect, "gradientsview", B_FOLLOW_ALL, B_WILL_DRAW),
	fType(BGradient::TYPE_LINEAR)
{
}


GradientsView::~GradientsView()
{
}


void
GradientsView::Draw(BRect update)
{
	switch (fType) {
		case BGradient::TYPE_LINEAR:
			DrawLinear(update);
			break;

		case BGradient::TYPE_RADIAL:
			DrawRadial(update);
			break;

		case BGradient::TYPE_RADIAL_FOCUS:
			DrawRadialFocus(update);
			break;

		case BGradient::TYPE_DIAMOND:
			DrawDiamond(update);
			break;

		case BGradient::TYPE_CONIC:
			DrawConic(update);
			break;

		case BGradient::TYPE_NONE:
		default:
			break;
	}
}


void
GradientsView::DrawLinear(BRect update)
{
	BGradientLinear gradient;
	rgb_color c;
	c.red = 255;
	c.green = 0;
	c.blue = 0;
	gradient.AddColor(c, 0);
	c.red = 0;
	c.green = 255;
	c.blue = 0;
	gradient.AddColor(c, 127);
	c.red = 0;
	c.green = 0;
	c.blue = 255;
	gradient.AddColor(c, 255);

	float spacing = 10.0;
	float shapeHeight = (Bounds().Height() - (5 * spacing)) / 4;

	BRect leftRect(spacing, spacing, spacing + shapeHeight, spacing + shapeHeight);
	BRect rightRect = leftRect.OffsetByCopy(spacing + shapeHeight, 0);

	// RoundRect
	SetHighColor(0, 0, 0);
	FillRoundRect(leftRect, 5, 5);
	gradient.SetStart(BPoint((2 * spacing) + shapeHeight, spacing));
	gradient.SetEnd(BPoint((2 * spacing) + (2 * shapeHeight), spacing + shapeHeight));
	FillRoundRect(rightRect, 5, 5, gradient);

	leftRect.OffsetBy(0, spacing + shapeHeight);
	rightRect.OffsetBy(0, spacing + shapeHeight);

	// Rect
	SetHighColor(0, 0, 0);
	FillRect(leftRect);
	gradient.SetStart(BPoint((2 * spacing) + shapeHeight, (2 * spacing) + shapeHeight));
	gradient.SetEnd(BPoint((2 * spacing) + (2 * shapeHeight), (2 * spacing) + (2 * shapeHeight)));
	FillRect(rightRect, gradient);

	leftRect.OffsetBy(0, spacing + shapeHeight);
	rightRect.OffsetBy(0, spacing + shapeHeight);

	// Triangle
	SetHighColor(0, 0, 0);
	FillTriangle(BPoint(leftRect.right - leftRect.Width() / 2, leftRect.top),
					BPoint(leftRect.left, leftRect.bottom),
					BPoint(leftRect.right, leftRect.bottom));
	gradient.SetStart(BPoint(leftRect.right - leftRect.Width() / 2, (3 * spacing) + (2 * shapeHeight)));
	gradient.SetEnd(BPoint(leftRect.right - leftRect.Width() / 2, (3 * spacing) + (3 * shapeHeight)));
	FillTriangle(BPoint(rightRect.right - rightRect.Width() / 2, rightRect.top),
					BPoint(rightRect.left, rightRect.bottom),
					BPoint(rightRect.right, rightRect.bottom),
					gradient);

	leftRect.OffsetBy(0, spacing + shapeHeight);
	rightRect.OffsetBy(0, spacing + shapeHeight);

	// Ellipse
	SetHighColor(0, 0, 0);
	FillEllipse(leftRect);
	gradient.SetStart(BPoint(leftRect.right - leftRect.Width() / 2, (4 * spacing) + (3 * shapeHeight)));
	gradient.SetEnd(BPoint(leftRect.right - leftRect.Width() / 2, (4 * spacing) + (4 * shapeHeight)));
	FillEllipse(rightRect, gradient);
}


void
GradientsView::DrawRadial(BRect update)
{
	BGradientRadial gradient;
	gradient.AddColor(make_color(255, 0, 0), 0);
	gradient.AddColor(make_color(0, 255, 0), 127);
	gradient.AddColor(make_color(0, 0, 255), 255);

	float spacing = 10.0;
	float shapeHeight = (Bounds().Height() - (5 * spacing)) / 4;

	BRect leftRect(spacing, spacing, spacing + shapeHeight, spacing + shapeHeight);
	BRect rightRect = leftRect.OffsetByCopy(spacing + shapeHeight, 0);

	// RoundRect
	SetHighColor(0, 0, 0);
	FillRoundRect(leftRect, 5, 5);
	gradient.SetCenter(BPoint(rightRect.left + rightRect.Width() / 2, rightRect.top + rightRect.Height() / 2));
	gradient.SetRadius(shapeHeight / 2);
	FillRoundRect(rightRect, 5, 5, gradient);

	leftRect.OffsetBy(0, spacing + shapeHeight);
	rightRect.OffsetBy(0, spacing + shapeHeight);

	// Rect
	SetHighColor(0, 0, 0);
	FillRect(leftRect);
	gradient.SetCenter(BPoint(rightRect.left + rightRect.Width() / 2, rightRect.top + rightRect.Height() / 2));
	FillRect(rightRect, gradient);

	leftRect.OffsetBy(0, spacing + shapeHeight);
	rightRect.OffsetBy(0, spacing + shapeHeight);

	// Triangle
	SetHighColor(0, 0, 0);
	FillTriangle(BPoint(leftRect.right - leftRect.Width() / 2, leftRect.top),
					BPoint(leftRect.left, leftRect.bottom),
					BPoint(leftRect.right, leftRect.bottom));
	gradient.SetCenter(BPoint(rightRect.left + rightRect.Width() / 2, rightRect.top + rightRect.Height() / 2));
	FillTriangle(BPoint(rightRect.right - rightRect.Width() / 2, rightRect.top),
					BPoint(rightRect.left, rightRect.bottom),
					BPoint(rightRect.right, rightRect.bottom),
					gradient);

	leftRect.OffsetBy(0, spacing + shapeHeight);
	rightRect.OffsetBy(0, spacing + shapeHeight);

	// Ellipse
	SetHighColor(0, 0, 0);
	FillEllipse(leftRect);
	gradient.SetCenter(BPoint(rightRect.left + rightRect.Width() / 2, rightRect.top + rightRect.Height() / 2));
	FillEllipse(rightRect, gradient);
}


void
GradientsView::DrawRadialFocus(BRect update)
{
	BGradientRadialFocus gradient;
	gradient.AddColor(make_color(255, 0, 0), 0);
	gradient.AddColor(make_color(0, 255, 0), 127);
	gradient.AddColor(make_color(0, 0, 255), 255);

	float spacing = 10.0;
	float shapeHeight = (Bounds().Height() - (5 * spacing)) / 4;

	BRect leftRect(spacing, spacing, spacing + shapeHeight, spacing + shapeHeight);
	BRect rightRect = leftRect.OffsetByCopy(spacing + shapeHeight, 0);

	// RoundRect
	SetHighColor(0, 0, 0);
	FillRoundRect(leftRect, 5, 5);
	gradient.SetCenter(BPoint(rightRect.left + rightRect.Width() / 2, rightRect.top + rightRect.Height() / 2));
	gradient.SetRadius(shapeHeight / 2);
	// Set focal point offset to upper-left to create a "light from above-left" effect
	gradient.SetFocal(BPoint(rightRect.left + rightRect.Width() / 3, rightRect.top + rightRect.Height() / 3));
	FillRoundRect(rightRect, 5, 5, gradient);

	leftRect.OffsetBy(0, spacing + shapeHeight);
	rightRect.OffsetBy(0, spacing + shapeHeight);

	// Rect
	SetHighColor(0, 0, 0);
	FillRect(leftRect);
	gradient.SetCenter(BPoint(rightRect.left + rightRect.Width() / 2, rightRect.top + rightRect.Height() / 2));
	gradient.SetRadius(shapeHeight / 2);
	// Set focal point offset to upper-right
	gradient.SetFocal(BPoint(rightRect.left + rightRect.Width() * 2 / 3, rightRect.top + rightRect.Height() / 3));
	FillRect(rightRect, gradient);


	leftRect.OffsetBy(0, spacing + shapeHeight);
	rightRect.OffsetBy(0, spacing + shapeHeight);

	// Triangle
	SetHighColor(0, 0, 0);
	FillTriangle(BPoint(leftRect.right - leftRect.Width() / 2, leftRect.top),
					BPoint(leftRect.left, leftRect.bottom),
					BPoint(leftRect.right, leftRect.bottom));
	gradient.SetCenter(BPoint(rightRect.left + rightRect.Width() / 2, rightRect.top + rightRect.Height() / 2));
	gradient.SetRadius(shapeHeight / 2);
	// Set focal point offset to lower-left
	gradient.SetFocal(BPoint(rightRect.left + rightRect.Width() / 3, rightRect.top + rightRect.Height() * 2 / 3));
	FillTriangle(BPoint(rightRect.right - rightRect.Width() / 2, rightRect.top),
					BPoint(rightRect.left, rightRect.bottom),
					BPoint(rightRect.right, rightRect.bottom),
					gradient);

	leftRect.OffsetBy(0, spacing + shapeHeight);
	rightRect.OffsetBy(0, spacing + shapeHeight);

	// Ellipse
	SetHighColor(0, 0, 0);
	FillEllipse(leftRect);
	gradient.SetCenter(BPoint(rightRect.left + rightRect.Width() / 2, rightRect.top + rightRect.Height() / 2));
	gradient.SetRadius(shapeHeight / 2);
	// Set focal point offset to lower-right to create a "light from below-right" effect
	gradient.SetFocal(BPoint(rightRect.left + rightRect.Width() * 2 / 3, rightRect.top + rightRect.Height() * 2 / 3));
	FillEllipse(rightRect, gradient);
}


void
GradientsView::DrawDiamond(BRect update)
{
	BGradientDiamond gradient;
	gradient.AddColor(make_color(255, 0, 0), 0);
	gradient.AddColor(make_color(0, 255, 0), 127);
	gradient.AddColor(make_color(0, 0, 255), 255);

	float spacing = 10.0;
	float shapeHeight = (Bounds().Height() - (5 * spacing)) / 4;

	BRect leftRect(spacing, spacing, spacing + shapeHeight, spacing + shapeHeight);
	BRect rightRect = leftRect.OffsetByCopy(spacing + shapeHeight, 0);

	// RoundRect
	SetHighColor(0, 0, 0);
	FillRoundRect(leftRect, 5, 5);
	gradient.SetCenter(BPoint(rightRect.left + rightRect.Width() / 2, rightRect.top + rightRect.Height() / 2));
	FillRoundRect(rightRect, 5, 5, gradient);

	leftRect.OffsetBy(0, spacing + shapeHeight);
	rightRect.OffsetBy(0, spacing + shapeHeight);

	// Rect
	SetHighColor(0, 0, 0);
	FillRect(leftRect);
	gradient.SetCenter(BPoint(rightRect.left + rightRect.Width() / 2, rightRect.top + rightRect.Height() / 2));
	FillRect(rightRect, gradient);

	leftRect.OffsetBy(0, spacing + shapeHeight);
	rightRect.OffsetBy(0, spacing + shapeHeight);

	// Triangle
	SetHighColor(0, 0, 0);
	FillTriangle(BPoint(leftRect.right - leftRect.Width() / 2, leftRect.top),
					BPoint(leftRect.left, leftRect.bottom),
					BPoint(leftRect.right, leftRect.bottom));
	gradient.SetCenter(BPoint(rightRect.left + rightRect.Width() / 2, rightRect.top + rightRect.Height() / 2));
	FillTriangle(BPoint(rightRect.right - rightRect.Width() / 2, rightRect.top),
					BPoint(rightRect.left, rightRect.bottom),
					BPoint(rightRect.right, rightRect.bottom),
					gradient);

	leftRect.OffsetBy(0, spacing + shapeHeight);
	rightRect.OffsetBy(0, spacing + shapeHeight);

	// Ellipse
	SetHighColor(0, 0, 0);
	FillEllipse(leftRect);
	gradient.SetCenter(BPoint(rightRect.left + rightRect.Width() / 2, rightRect.top + rightRect.Height() / 2));
	FillEllipse(rightRect, gradient);
}


void
GradientsView::DrawConic(BRect update)
{
	BGradientConic gradient;
	gradient.AddColor(make_color(255, 0, 0), 0);
	gradient.AddColor(make_color(0, 255, 0), 127);
	gradient.AddColor(make_color(0, 0, 255), 255);

	float spacing = 10.0;
	float shapeHeight = (Bounds().Height() - (5 * spacing)) / 4;

	BRect leftRect(spacing, spacing, spacing + shapeHeight, spacing + shapeHeight);
	BRect rightRect = leftRect.OffsetByCopy(spacing + shapeHeight, 0);

	// RoundRect
	SetHighColor(0, 0, 0);
	FillRoundRect(leftRect, 5, 5);
	gradient.SetCenter(BPoint(rightRect.left + rightRect.Width() / 2, rightRect.top + rightRect.Height() / 2));
	FillRoundRect(rightRect, 5, 5, gradient);

	leftRect.OffsetBy(0, spacing + shapeHeight);
	rightRect.OffsetBy(0, spacing + shapeHeight);

	// Rect
	SetHighColor(0, 0, 0);
	FillRect(leftRect);
	gradient.SetCenter(BPoint(rightRect.left + rightRect.Width() / 2, rightRect.top + rightRect.Height() / 2));
	FillRect(rightRect, gradient);

	leftRect.OffsetBy(0, spacing + shapeHeight);
	rightRect.OffsetBy(0, spacing + shapeHeight);

	// Triangle
	SetHighColor(0, 0, 0);
	FillTriangle(BPoint(leftRect.right - leftRect.Width() / 2, leftRect.top),
					BPoint(leftRect.left, leftRect.bottom),
					BPoint(leftRect.right, leftRect.bottom));
	gradient.SetCenter(BPoint(rightRect.left + rightRect.Width() / 2, rightRect.top + rightRect.Height() / 2));
	FillTriangle(BPoint(rightRect.right - rightRect.Width() / 2, rightRect.top),
					BPoint(rightRect.left, rightRect.bottom),
					BPoint(rightRect.right, rightRect.bottom),
					gradient);

	leftRect.OffsetBy(0, spacing + shapeHeight);
	rightRect.OffsetBy(0, spacing + shapeHeight);

	// Ellipse
	SetHighColor(0, 0, 0);
	FillEllipse(leftRect);
	gradient.SetCenter(BPoint(rightRect.left + rightRect.Width() / 2, rightRect.top + rightRect.Height() / 2));
	FillEllipse(rightRect, gradient);
}


void
GradientsView::SetType(BGradient::Type type)
{
	fType = type;
	Invalidate();
}
