#ifndef _VIEW_PICTURE_PLAYER_H
#define _VIEW_PICTURE_PLAYER_H

#include <PicturePlayer.h>

#include <vector>


class BPicture;
class BView;


class ViewPicturePlayer : public BPrivate::PicturePlayerCallbacks {
public:
	explicit					ViewPicturePlayer(BView& view,
							const BPicture& picture);

	virtual void			MovePenBy(const BPoint& where);
	virtual void			StrokeLine(const BPoint& start, const BPoint& end);
	virtual void			DrawRect(const BRect& rect, bool fill);
	virtual void			DrawRoundRect(const BRect& rect, const BPoint& radii,
							bool fill);
	virtual void			DrawBezier(const BPoint controlPoints[4], bool fill);
	virtual void			DrawArc(const BPoint& center, const BPoint& radii,
							float startTheta, float arcTheta, bool fill);
	virtual void			DrawEllipse(const BRect& rect, bool fill);
	virtual void			DrawPolygon(size_t numPoints, const BPoint points[],
							bool isClosed, bool fill);
	virtual void			DrawShape(const BShape& shape, bool fill);
	virtual void			DrawString(const char* string, size_t length,
							const escapement_delta& delta);
	virtual void			DrawPixels(const BRect& source,
							const BRect& destination, uint32 width,
							uint32 height, size_t bytesPerRow,
							color_space pixelFormat, uint32 flags,
							const void* data, size_t length);
	virtual void			DrawPicture(const BPoint& where, int32 token);
	virtual void			SetClippingRects(size_t numRects,
							const clipping_rect rects[]);
	virtual void			ClipToPicture(int32 token, const BPoint& where,
							bool clipToInverse);
	virtual void			PushState();
	virtual void			PopState();
	virtual void			EnterStateChange();
	virtual void			ExitStateChange();
	virtual void			EnterFontState();
	virtual void			ExitFontState();
	virtual void			SetOrigin(const BPoint& origin);
	virtual void			SetPenLocation(const BPoint& location);
	virtual void			SetDrawingMode(drawing_mode mode);
	virtual void			SetLineMode(cap_mode capMode, join_mode joinMode,
							float miterLimit);
	virtual void			SetPenSize(float size);
	virtual void			SetForeColor(const rgb_color& color);
	virtual void			SetBackColor(const rgb_color& color);
	virtual void			SetStipplePattern(const pattern& pattern);
	virtual void			SetScale(float scale);
	virtual void			SetFontFamily(const char* familyName, size_t length);
	virtual void			SetFontStyle(const char* styleName, size_t length);
	virtual void			SetFontSpacing(uint8 spacing);
	virtual void			SetFontSize(float size);
	virtual void			SetFontRotation(float rotation);
	virtual void			SetFontEncoding(uint8 encoding);
	virtual void			SetFontFlags(uint32 flags);
	virtual void			SetFontShear(float shear);
	virtual void			SetFontFace(uint16 face);
	virtual void			SetBlendingMode(source_alpha alphaSourceMode,
							alpha_function alphaFunctionMode);
	virtual void			SetTransform(const BAffineTransform& transform);
	virtual void			TranslateBy(double x, double y);
	virtual void			ScaleBy(double x, double y);
	virtual void			RotateBy(double angleRadians);
	virtual void			BlendLayer(Layer* layer);
	virtual void			ClipToRect(const BRect& rect, bool inverse);
	virtual void			ClipToShape(int32 opCount, const uint32 opList[],
							int32 ptCount, const BPoint ptList[], bool inverse);
	virtual void			DrawStringLocations(const char* string, size_t length,
							const BPoint locations[], size_t locationCount);
	virtual void			DrawRectGradient(const BRect& rect, BGradient& gradient,
							bool fill);
	virtual void			DrawRoundRectGradient(const BRect& rect,
							const BPoint& radii, BGradient& gradient,
							bool fill);
	virtual void			DrawBezierGradient(const BPoint controlPoints[4],
							BGradient& gradient, bool fill);
	virtual void			DrawArcGradient(const BPoint& center,
							const BPoint& radii, float startTheta,
							float arcTheta, BGradient& gradient, bool fill);
	virtual void			DrawEllipseGradient(const BRect& rect,
							BGradient& gradient, bool fill);
	virtual void			DrawPolygonGradient(size_t numPoints,
							const BPoint points[], bool isClosed,
							BGradient& gradient, bool fill);
	virtual void			DrawShapeGradient(const BShape& shape,
							BGradient& gradient, bool fill);
	virtual void			SetFillRule(int32 fillRule);
	virtual void			StrokeLineGradient(const BPoint& start,
							const BPoint& end, BGradient& gradient);

private:
	::pattern				Pattern() const;
	BPicture*				_ResolvePicture(int32 reference) const;

private:
	BView&					fView;
	BPicture*				fPicture;
	::pattern				fPattern;
	std::vector< ::pattern >	fPatternStack;
};


#endif