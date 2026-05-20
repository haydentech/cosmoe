#include <ViewPicturePlayer.h>

#include <Bitmap.h>
#include <Picture.h>
#include <PicturePrivate.h>
#include <Shape.h>
#include <ShapePrivate.h>
#include <View.h>
#include <ViewPrivate.h>


ViewPicturePlayer::ViewPicturePlayer(BView& view, const BPicture& picture)
	:
	fView(view),
	fPicture(const_cast<BPicture*>(&picture)),
	fPattern(B_SOLID_HIGH)
{
}


void
ViewPicturePlayer::MovePenBy(const BPoint& where)
{
	fView.MovePenBy(where.x, where.y);
}


::pattern
ViewPicturePlayer::Pattern() const
{
	return fPattern;
}


void
ViewPicturePlayer::StrokeLine(const BPoint& start, const BPoint& end)
{
	fView.StrokeLine(start, end, Pattern());
}


void
ViewPicturePlayer::DrawRect(const BRect& rect, bool fill)
{
	if (fill)
		fView.FillRect(rect, Pattern());
	else
		fView.StrokeRect(rect, Pattern());
}


void
ViewPicturePlayer::DrawRoundRect(const BRect& rect, const BPoint& radii,
	bool fill)
{
	if (fill)
		fView.FillRoundRect(rect, radii.x, radii.y, Pattern());
	else
		fView.StrokeRoundRect(rect, radii.x, radii.y, Pattern());
}


void
ViewPicturePlayer::DrawBezier(const BPoint controlPoints[4], bool fill)
{
	BPoint copy[4] = { controlPoints[0], controlPoints[1], controlPoints[2],
		controlPoints[3] };
	if (fill)
		fView.FillBezier(copy, Pattern());
	else
		fView.StrokeBezier(copy, Pattern());
}


void
ViewPicturePlayer::DrawArc(const BPoint& center, const BPoint& radii,
	float startTheta, float arcTheta, bool fill)
{
	if (fill)
		fView.FillArc(center, radii.x, radii.y, startTheta, arcTheta, Pattern());
	else
		fView.StrokeArc(center, radii.x, radii.y, startTheta, arcTheta,
			Pattern());
}


void
ViewPicturePlayer::DrawEllipse(const BRect& rect, bool fill)
{
	if (fill)
		fView.FillEllipse(rect, Pattern());
	else
		fView.StrokeEllipse(rect, Pattern());
}


void
ViewPicturePlayer::DrawPolygon(size_t numPoints, const BPoint points[],
	bool isClosed, bool fill)
{
	if (fill)
		fView.FillPolygon(points, numPoints, Pattern());
	else
		fView.StrokePolygon(points, numPoints, isClosed, Pattern());
}


void
ViewPicturePlayer::DrawShape(const BShape& shape, bool fill)
{
	BShape copy(shape);
	if (fill)
		fView.FillShape(&copy, Pattern());
	else
		fView.StrokeShape(&copy, Pattern());
}


void
ViewPicturePlayer::DrawString(const char* string, size_t length,
	const escapement_delta& delta)
{
	escapement_delta mutableDelta = delta;
	fView.DrawString(string, length, &mutableDelta);
}


void
ViewPicturePlayer::DrawPixels(const BRect& source, const BRect& destination,
	uint32 width, uint32 height, size_t bytesPerRow, color_space pixelFormat,
	uint32 flags, const void* data, size_t length)
{
	if (width == 0 || height == 0)
		return;

	BBitmap bitmap(BRect(0, 0, width - 1, height - 1), pixelFormat);
	if (bitmap.InitCheck() != B_OK)
		return;

	if (bitmap.ImportBits(data, length, bytesPerRow, 0, pixelFormat) != B_OK)
		return;

	fView.DrawBitmapAsync(&bitmap, source, destination, flags);
}


void
ViewPicturePlayer::DrawPicture(const BPoint& where, int32 token)
{
	BPicture* picture = _ResolvePicture(token);
	if (picture != NULL)
		fView.DrawPictureAsync(picture, where);
}


void
ViewPicturePlayer::SetClippingRects(size_t numRects,
	const clipping_rect rects[])
{
	if (numRects == 0) {
		fView.ConstrainClippingRegion(NULL);
		return;
	}

	BRegion region;
	for (size_t i = 0; i < numRects; i++) {
		region.Include(BRect(rects[i].left, rects[i].top, rects[i].right,
			rects[i].bottom));
	}

	fView.ConstrainClippingRegion(&region);
}


void
ViewPicturePlayer::ClipToPicture(int32 token, const BPoint& where,
	bool clipToInverse)
{
	BPicture* picture = _ResolvePicture(token);
	if (picture != NULL)
		BView::Private(&fView).ClipToPicture(picture, where, clipToInverse,
			false);
}


void
ViewPicturePlayer::PushState()
{
	fPatternStack.push_back(fPattern);
	fView.PushState();
}


void
ViewPicturePlayer::PopState()
{
	fView.PopState();
	if (!fPatternStack.empty()) {
		fPattern = fPatternStack.back();
		fPatternStack.pop_back();
	}
}


void
ViewPicturePlayer::EnterStateChange()
{
}


void
ViewPicturePlayer::ExitStateChange()
{
}


void
ViewPicturePlayer::EnterFontState()
{
}


void
ViewPicturePlayer::ExitFontState()
{
}


void
ViewPicturePlayer::SetOrigin(const BPoint& origin)
{
	fView.SetOrigin(origin);
}


void
ViewPicturePlayer::SetPenLocation(const BPoint& location)
{
	fView.MovePenTo(location);
}


void
ViewPicturePlayer::SetDrawingMode(drawing_mode mode)
{
	fView.SetDrawingMode(mode);
}


void
ViewPicturePlayer::SetLineMode(cap_mode capMode, join_mode joinMode,
	float miterLimit)
{
	fView.SetLineMode(capMode, joinMode, miterLimit);
}


void
ViewPicturePlayer::SetPenSize(float size)
{
	fView.SetPenSize(size);
}


void
ViewPicturePlayer::SetForeColor(const rgb_color& color)
{
	fView.SetHighColor(color);
}


void
ViewPicturePlayer::SetBackColor(const rgb_color& color)
{
	fView.SetLowColor(color);
}


void
ViewPicturePlayer::SetStipplePattern(const pattern& pattern)
{
	fPattern = pattern;
	BView::Private(&fView).SetPattern(pattern);
}


void
ViewPicturePlayer::SetScale(float scale)
{
	fView.SetScale(scale);
}


void
ViewPicturePlayer::SetFontFamily(const char* familyName, size_t)
{
	BFont font;
	fView.GetFont(&font);

	font_style style;
	font_family family;
	font.GetFamilyAndStyle(&family, &style);
	font.SetFamilyAndStyle(familyName, style);
	fView.SetFont(&font, B_FONT_FAMILY_AND_STYLE);
}


void
ViewPicturePlayer::SetFontStyle(const char* styleName, size_t)
{
	BFont font;
	fView.GetFont(&font);

	font_style style;
	font_family family;
	font.GetFamilyAndStyle(&family, &style);
	font.SetFamilyAndStyle(family, styleName);
	fView.SetFont(&font, B_FONT_FAMILY_AND_STYLE);
}


void
ViewPicturePlayer::SetFontSpacing(uint8 spacing)
{
	BFont font;
	fView.GetFont(&font);
	font.SetSpacing(spacing);
	fView.SetFont(&font, B_FONT_SPACING);
}


void
ViewPicturePlayer::SetFontSize(float size)
{
	fView.SetFontSize(size);
}


void
ViewPicturePlayer::SetFontRotation(float rotation)
{
	BFont font;
	fView.GetFont(&font);
	font.SetRotation(rotation);
	fView.SetFont(&font, B_FONT_ROTATION);
}


void
ViewPicturePlayer::SetFontEncoding(uint8 encoding)
{
	BFont font;
	fView.GetFont(&font);
	font.SetEncoding(encoding);
	fView.SetFont(&font, B_FONT_ENCODING);
}


void
ViewPicturePlayer::SetFontFlags(uint32 flags)
{
	BFont font;
	fView.GetFont(&font);
	font.SetFlags(flags);
	fView.SetFont(&font, B_FONT_FLAGS);
}


void
ViewPicturePlayer::SetFontShear(float shear)
{
	BFont font;
	fView.GetFont(&font);
	font.SetShear(shear);
	fView.SetFont(&font, B_FONT_SHEAR);
}


void
ViewPicturePlayer::SetFontFace(uint16 face)
{
	BFont font;
	fView.GetFont(&font);
	font.SetFace(face);
	fView.SetFont(&font, B_FONT_FACE);
}


void
ViewPicturePlayer::SetBlendingMode(source_alpha alphaSourceMode,
	alpha_function alphaFunctionMode)
{
	fView.SetBlendingMode(alphaSourceMode, alphaFunctionMode);
}


void
ViewPicturePlayer::SetTransform(const BAffineTransform& transform)
{
	fView.SetTransform(transform);
}


void
ViewPicturePlayer::TranslateBy(double x, double y)
{
	fView.TranslateBy(x, y);
}


void
ViewPicturePlayer::ScaleBy(double x, double y)
{
	fView.ScaleBy(x, y);
}


void
ViewPicturePlayer::RotateBy(double angleRadians)
{
	fView.RotateBy(angleRadians);
}


void
ViewPicturePlayer::BlendLayer(Layer*)
{
}


void
ViewPicturePlayer::ClipToRect(const BRect& rect, bool inverse)
{
	if (inverse)
		fView.ClipToInverseRect(rect);
	else
		fView.ClipToRect(rect);
}


void
ViewPicturePlayer::ClipToShape(int32 opCount, const uint32 opList[],
	int32 ptCount, const BPoint ptList[], bool inverse)
{
	BShape shape;
	BShape::Private(shape).SetData(opCount, ptCount, opList, ptList);
	if (inverse)
		fView.ClipToInverseShape(&shape);
	else
		fView.ClipToShape(&shape);
}


void
ViewPicturePlayer::DrawStringLocations(const char* string, size_t length,
	const BPoint locations[], size_t locationCount)
{
	fView.DrawString(string, length, locations, locationCount);
}


void
ViewPicturePlayer::DrawRectGradient(const BRect& rect, BGradient& gradient,
	bool fill)
{
	if (fill)
		fView.FillRect(rect, gradient);
	else
		fView.StrokeRect(rect);
}


void
ViewPicturePlayer::DrawRoundRectGradient(const BRect& rect,
	const BPoint& radii, BGradient& gradient, bool fill)
{
	if (fill)
		fView.FillRoundRect(rect, radii.x, radii.y, gradient);
	else
		fView.StrokeRoundRect(rect, radii.x, radii.y, gradient);
}


void
ViewPicturePlayer::DrawBezierGradient(const BPoint controlPoints[4],
	BGradient& gradient, bool fill)
{
	BPoint copy[4] = { controlPoints[0], controlPoints[1], controlPoints[2],
		controlPoints[3] };
	if (fill)
		fView.FillBezier(copy, gradient);
	else
		fView.StrokeBezier(copy, gradient);
}


void
ViewPicturePlayer::DrawArcGradient(const BPoint& center,
	const BPoint& radii, float startTheta, float arcTheta, BGradient& gradient,
	bool fill)
{
	if (fill)
		fView.FillArc(center, radii.x, radii.y, startTheta, arcTheta, gradient);
	else
		fView.StrokeArc(center, radii.x, radii.y, startTheta, arcTheta,
			gradient);
}


void
ViewPicturePlayer::DrawEllipseGradient(const BRect& rect, BGradient& gradient,
	bool fill)
{
	if (fill)
		fView.FillEllipse(rect, gradient);
	else
		fView.StrokeEllipse(rect, gradient);
}


void
ViewPicturePlayer::DrawPolygonGradient(size_t numPoints,
	const BPoint points[], bool isClosed, BGradient& gradient, bool fill)
{
	if (fill)
		fView.FillPolygon(points, numPoints, gradient);
	else
		fView.StrokePolygon(points, numPoints, isClosed, gradient);
}


void
ViewPicturePlayer::DrawShapeGradient(const BShape& shape,
	BGradient& gradient, bool fill)
{
	BShape copy(shape);
	if (fill)
		fView.FillShape(&copy, gradient);
	else
		fView.StrokeShape(&copy, gradient);
}


void
ViewPicturePlayer::SetFillRule(int32 fillRule)
{
	fView.SetFillRule(fillRule);
}


void
ViewPicturePlayer::StrokeLineGradient(const BPoint& start,
	const BPoint& end, BGradient& gradient)
{
	fView.StrokeLine(start, end, gradient);
}


BPicture*
ViewPicturePlayer::_ResolvePicture(int32 reference) const
{
	if (reference < 0)
		return NULL;

	BPicture::Private picturePrivate(fPicture);
	if (reference >= picturePrivate.CountPictures())
		return NULL;

	return picturePrivate.PictureAt(reference);
}