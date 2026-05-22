/*
 * Copyright (c) 2008-2009, Haiku, Inc.
 * Distributed under the terms of the MIT license.
 *
 * Authors:
 *		Artur Wyszynski <harakash@gmail.com>
 */


#include "GradientsView.h"

#include <Region.h>
#include <Shape.h>

#include <GradientLinear.h>
#include <GradientRadial.h>
#include <GradientRadialFocus.h>
#include <GradientDiamond.h>
#include <GradientConic.h>


namespace {

enum shape_kind {
	SHAPE_ROUND_RECT = 0,
	SHAPE_RECT,
	SHAPE_TRIANGLE,
	SHAPE_ELLIPSE,
	SHAPE_POLYGON,
	SHAPE_REGION,
	SHAPE_SHAPE,
	SHAPE_ARC,
	SHAPE_BEZIER,
	kShapeCount
};


void
AddDefaultColors(BGradient& gradient)
{
	gradient.AddColor(make_color(255, 0, 0), 0);
	gradient.AddColor(make_color(0, 255, 0), 127);
	gradient.AddColor(make_color(0, 0, 255), 255);
}


BPoint
RectCenter(const BRect& rect)
{
	return BPoint(rect.left + rect.Width() / 2, rect.top + rect.Height() / 2);
}


void
MakeTriangle(const BRect& rect, BPoint points[3])
{
	points[0] = BPoint(rect.left + rect.Width() / 2, rect.top);
	points[1] = BPoint(rect.left, rect.bottom);
	points[2] = BPoint(rect.right, rect.bottom);
}


void
MakePolygon(const BRect& rect, BPoint points[5])
{
	const float width = rect.Width();
	const float height = rect.Height();

	points[0] = BPoint(rect.left + width * 0.50f, rect.top);
	points[1] = BPoint(rect.right, rect.top + height * 0.35f);
	points[2] = BPoint(rect.right - width * 0.18f, rect.bottom);
	points[3] = BPoint(rect.left + width * 0.18f, rect.bottom);
	points[4] = BPoint(rect.left, rect.top + height * 0.35f);
}


void
MakeBezier(const BRect& rect, BPoint points[4])
{
	const float width = rect.Width();
	const float height = rect.Height();

	points[0] = BPoint(rect.left, rect.top + height * 0.75f);
	points[1] = BPoint(rect.left + width * 0.20f, rect.top);
	points[2] = BPoint(rect.right - width * 0.20f, rect.bottom);
	points[3] = BPoint(rect.right, rect.top + height * 0.25f);
}


void
MakeRegion(const BRect& rect, BRegion& region)
{
	region.MakeEmpty();

	const float width = rect.Width();
	const float height = rect.Height();

	// Left cluster: blocky "A" shape with a hole
	region.Include(BRect(rect.left + width * 0.08f, rect.top + height * 0.08f,
		rect.left + width * 0.24f, rect.bottom - height * 0.08f));
	region.Include(BRect(rect.left + width * 0.24f, rect.top + height * 0.08f,
		rect.left + width * 0.44f, rect.top + height * 0.24f));
	region.Include(BRect(rect.left + width * 0.24f, rect.top + height * 0.42f,
		rect.left + width * 0.38f, rect.top + height * 0.56f));
	region.Include(BRect(rect.left + width * 0.36f, rect.top + height * 0.08f,
		rect.left + width * 0.52f, rect.bottom - height * 0.08f));

	// Right cluster: detached "plus" island
	region.Include(BRect(rect.left + width * 0.70f, rect.top + height * 0.24f,
		rect.left + width * 0.88f, rect.top + height * 0.38f));
	region.Include(BRect(rect.left + width * 0.77f, rect.top + height * 0.14f,
		rect.left + width * 0.81f, rect.top + height * 0.48f));
}


void
MakeShape(const BRect& rect, BShape& shape)
{
	shape.Clear();

	const float width = rect.Width();
	const float height = rect.Height();

	shape.MoveTo(BPoint(rect.left + width * 0.50f, rect.top + height * 0.08f));
	shape.BezierTo(BPoint(rect.left + width * 0.88f, rect.top + height * 0.05f),
		BPoint(rect.right - width * 0.02f, rect.top + height * 0.45f),
		BPoint(rect.left + width * 0.58f, rect.bottom - height * 0.06f));
	shape.BezierTo(BPoint(rect.left + width * 0.42f, rect.bottom - height * 0.06f),
		BPoint(rect.left + width * 0.02f, rect.top + height * 0.45f),
		BPoint(rect.left + width * 0.50f, rect.top + height * 0.08f));
	shape.Close();
}


void
FillShape(BView* view, shape_kind shape, const BRect& rect)
{
	switch (shape) {
		case SHAPE_ROUND_RECT:
			view->FillRoundRect(rect, 5, 5);
			break;

		case SHAPE_RECT:
			view->FillRect(rect);
			break;

		case SHAPE_TRIANGLE:
		{
			BPoint points[3];
			MakeTriangle(rect, points);
			view->FillTriangle(points[0], points[1], points[2]);
			break;
		}

		case SHAPE_ELLIPSE:
			view->FillEllipse(rect);
			break;

		case SHAPE_POLYGON:
		{
			BPoint points[5];
			MakePolygon(rect, points);
			view->FillPolygon(points, 5);
			break;
		}

		case SHAPE_REGION:
		{
			BRegion region;
			MakeRegion(rect, region);
			view->FillRegion(&region);
			break;
		}

		case SHAPE_SHAPE:
		{
			BShape shapePath;
			MakeShape(rect, shapePath);
			view->FillShape(&shapePath);
			break;
		}

		case SHAPE_ARC:
			view->FillArc(rect, 35, 300);
			break;

		case SHAPE_BEZIER:
		{
			BPoint points[4];
			MakeBezier(rect, points);
			view->FillBezier(points);
			break;
		}

		default:
			break;
	}
}


void
FillShape(BView* view, shape_kind shape, const BRect& rect,
	const BGradient& gradient)
{
	switch (shape) {
		case SHAPE_ROUND_RECT:
			view->FillRoundRect(rect, 5, 5, gradient);
			break;

		case SHAPE_RECT:
			view->FillRect(rect, gradient);
			break;

		case SHAPE_TRIANGLE:
		{
			BPoint points[3];
			MakeTriangle(rect, points);
			view->FillTriangle(points[0], points[1], points[2], gradient);
			break;
		}

		case SHAPE_ELLIPSE:
			view->FillEllipse(rect, gradient);
			break;

		case SHAPE_POLYGON:
		{
			BPoint points[5];
			MakePolygon(rect, points);
			view->FillPolygon(points, 5, gradient);
			break;
		}

		case SHAPE_REGION:
		{
			BRegion region;
			MakeRegion(rect, region);
			view->FillRegion(&region, gradient);
			break;
		}

		case SHAPE_SHAPE:
		{
			BShape shapePath;
			MakeShape(rect, shapePath);
			view->FillShape(&shapePath, gradient);
			break;
		}

		case SHAPE_ARC:
			view->FillArc(rect, 35, 300, gradient);
			break;

		case SHAPE_BEZIER:
		{
			BPoint points[4];
			MakeBezier(rect, points);
			view->FillBezier(points, gradient);
			break;
		}

		default:
			break;
	}
}


void
StrokeShape(BView* view, shape_kind shape, const BRect& rect,
	const BGradient& gradient)
{
	float oldPenSize = view->PenSize();
	view->SetPenSize(3.0f);

	switch (shape) {
		case SHAPE_ROUND_RECT:
			view->StrokeRoundRect(rect, 5, 5, gradient);
			break;

		case SHAPE_RECT:
			view->StrokeRect(rect, gradient);
			break;

		case SHAPE_TRIANGLE:
		{
			BPoint points[3];
			MakeTriangle(rect, points);
			view->StrokeTriangle(points[0], points[1], points[2], gradient);
			break;
		}

		case SHAPE_ELLIPSE:
			view->StrokeEllipse(rect, gradient);
			break;

		case SHAPE_POLYGON:
		{
			BPoint points[5];
			MakePolygon(rect, points);
			view->StrokePolygon(points, 5, true, gradient);
			break;
		}

		case SHAPE_REGION:
		{
			// There is no StrokeRegion() method
			break;
		}

		case SHAPE_SHAPE:
		{
			BShape shapePath;
			MakeShape(rect, shapePath);
			view->StrokeShape(&shapePath, gradient);
			break;
		}

		case SHAPE_ARC:
			view->StrokeArc(rect, 35, 300, gradient);
			break;

		case SHAPE_BEZIER:
		{
			BPoint points[4];
			MakeBezier(rect, points);
			view->StrokeBezier(points, gradient);
			break;
		}

		default:
			break;
	}

	view->SetPenSize(oldPenSize);
}


void
ConfigureLinearGradient(BGradientLinear& gradient, shape_kind shape,
	const BRect& rect)
{
	switch (shape) {
		case SHAPE_ROUND_RECT:
			gradient.SetStart(rect.LeftTop());
			gradient.SetEnd(rect.RightBottom());
			break;

		case SHAPE_RECT:
			gradient.SetStart(BPoint(rect.left, rect.top + rect.Height() / 2));
			gradient.SetEnd(BPoint(rect.right, rect.top + rect.Height() / 2));
			break;

		case SHAPE_TRIANGLE:
		case SHAPE_ELLIPSE:
			gradient.SetStart(BPoint(rect.left + rect.Width() / 2, rect.top));
			gradient.SetEnd(BPoint(rect.left + rect.Width() / 2, rect.bottom));
			break;

		case SHAPE_POLYGON:
			gradient.SetStart(BPoint(rect.left, rect.bottom));
			gradient.SetEnd(BPoint(rect.right, rect.top));
			break;

		case SHAPE_REGION:
			gradient.SetStart(BPoint(rect.left + rect.Width() / 2, rect.top));
			gradient.SetEnd(BPoint(rect.left + rect.Width() / 2, rect.bottom));
			break;

		case SHAPE_SHAPE:
		case SHAPE_ARC:
			gradient.SetStart(BPoint(rect.left, rect.top + rect.Height() / 2));
			gradient.SetEnd(BPoint(rect.right, rect.top + rect.Height() / 2));
			break;

		case SHAPE_BEZIER:
			gradient.SetStart(BPoint(rect.left, rect.top + rect.Height() / 2));
			gradient.SetEnd(BPoint(rect.right, rect.top + rect.Height() / 2));
			break;

		default:
			break;
	}
}


template<typename Gradient>
void
ConfigureCenteredGradient(Gradient& gradient, const BRect& rect, float radius)
{
	gradient.SetCenter(RectCenter(rect));
	gradient.SetRadius(radius);
}


void
ConfigureRadialFocusGradient(BGradientRadialFocus& gradient, shape_kind shape,
	const BRect& rect, float radius)
{
	const float width = rect.Width();
	const float height = rect.Height();

	ConfigureCenteredGradient(gradient, rect, radius);

	switch (shape) {
		case SHAPE_ROUND_RECT:
			gradient.SetFocal(BPoint(rect.left + width / 3, rect.top + height / 3));
			break;

		case SHAPE_RECT:
			gradient.SetFocal(BPoint(rect.left + width * 2 / 3,
				rect.top + height / 3));
			break;

		case SHAPE_TRIANGLE:
			gradient.SetFocal(BPoint(rect.left + width / 3,
				rect.top + height * 2 / 3));
			break;

		case SHAPE_ELLIPSE:
			gradient.SetFocal(BPoint(rect.left + width * 2 / 3,
				rect.top + height * 2 / 3));
			break;

		case SHAPE_POLYGON:
			gradient.SetFocal(BPoint(rect.left + width / 2, rect.top + height / 4));
			break;

		case SHAPE_REGION:
			gradient.SetFocal(BPoint(rect.left + width * 0.35f,
				rect.top + height * 0.35f));
			break;

		case SHAPE_SHAPE:
			gradient.SetFocal(BPoint(rect.left + width * 0.65f,
				rect.top + height * 0.40f));
			break;

		case SHAPE_ARC:
			gradient.SetFocal(BPoint(rect.left + width * 0.50f,
				rect.top + height * 0.25f));
			break;

		case SHAPE_BEZIER:
			gradient.SetFocal(BPoint(rect.left + width / 4,
				rect.top + height / 2));
			break;

		default:
			break;
	}
}


template<typename Gradient, typename Configure>
void
DrawGradientSamples(BView* view, Gradient& gradient, Configure configure)
{
	const int32 kGridColumns = 3;
	const int32 kGridRows = 3;
	const int32 kSampleColumns = 3;

	static const shape_kind kShapes[] = {
		SHAPE_ROUND_RECT,
		SHAPE_RECT,
		SHAPE_TRIANGLE,
		SHAPE_ELLIPSE,
		SHAPE_POLYGON,
		SHAPE_REGION,
		SHAPE_SHAPE,
		SHAPE_ARC,
		SHAPE_BEZIER,
	};

	const float margin = 10.0f;
	const float cellSpacing = 10.0f;
	const float panelSpacing = 20.0f;

	const float panelWidth = (view->Bounds().Width() - margin * 2
		- panelSpacing * (kSampleColumns - 1)) / kSampleColumns;
	const float cellWidth = (panelWidth - cellSpacing * (kGridColumns - 1))
		/ kGridColumns;
	const float cellHeight = (view->Bounds().Height() - margin * 2
		- cellSpacing * (kGridRows - 1)) / kGridRows;
	const float shapeSize = cellWidth < cellHeight ? cellWidth : cellHeight;

	if (shapeSize <= 0)
		return;

	const float gridWidth = kGridColumns * shapeSize
		+ (kGridColumns - 1) * cellSpacing;
	const float gridHeight = kGridRows * shapeSize
		+ (kGridRows - 1) * cellSpacing;

	const float baseY = margin
		+ (view->Bounds().Height() - margin * 2 - gridHeight) / 2;

	auto gridRectAt = [&](int32 panel, int32 row, int32 column) {
		const float panelStartX = margin + panel * (panelWidth + panelSpacing);
		const float baseX = panelStartX + (panelWidth - gridWidth) / 2;
		const float x = baseX + column * (shapeSize + cellSpacing);
		const float y = baseY + row * (shapeSize + cellSpacing);
		return BRect(x, y, x + shapeSize, y + shapeSize);
	};

	for (int32 i = 0; i < kShapeCount; i++) {
		shape_kind shape = kShapes[i];
		const int32 row = i / kGridColumns;
		const int32 column = i % kGridColumns;

		BRect leftRect = gridRectAt(0, row, column);
		BRect middleRect = gridRectAt(1, row, column);
		BRect rightRect = gridRectAt(2, row, column);

		view->SetHighColor(0, 0, 0);
		FillShape(view, shape, leftRect);

		configure(gradient, shape, middleRect, shapeSize);
		FillShape(view, shape, middleRect, gradient);
		configure(gradient, shape, rightRect, shapeSize);
		StrokeShape(view, shape, rightRect, gradient);
	}
}

}


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
	AddDefaultColors(gradient);

	DrawGradientSamples(this, gradient,
		[](BGradientLinear& current, shape_kind shape, const BRect& rect,
			float) {
			ConfigureLinearGradient(current, shape, rect);
		});
}


void
GradientsView::DrawRadial(BRect update)
{
	BGradientRadial gradient;
	AddDefaultColors(gradient);

	DrawGradientSamples(this, gradient,
		[](BGradientRadial& current, shape_kind, const BRect& rect,
			float shapeHeight) {
			ConfigureCenteredGradient(current, rect, shapeHeight / 2);
		});
}


void
GradientsView::DrawRadialFocus(BRect update)
{
	BGradientRadialFocus gradient;
	AddDefaultColors(gradient);

	DrawGradientSamples(this, gradient,
		[](BGradientRadialFocus& current, shape_kind shape, const BRect& rect,
			float shapeHeight) {
			ConfigureRadialFocusGradient(current, shape, rect, shapeHeight / 2);
		});
}


void
GradientsView::DrawDiamond(BRect update)
{
	BGradientDiamond gradient;
	AddDefaultColors(gradient);

	DrawGradientSamples(this, gradient,
		[](BGradientDiamond& current, shape_kind, const BRect& rect, float) {
			current.SetCenter(RectCenter(rect));
		});
}


void
GradientsView::DrawConic(BRect update)
{
	BGradientConic gradient;
	AddDefaultColors(gradient);

	DrawGradientSamples(this, gradient,
		[](BGradientConic& current, shape_kind, const BRect& rect, float) {
			current.SetCenter(RectCenter(rect));
		});
}


void
GradientsView::SetType(BGradient::Type type)
{
	fType = type;
	Invalidate();
}
