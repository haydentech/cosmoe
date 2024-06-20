/*
 * Copyright 2001-2009, Haiku, Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		DarkWyrm <bpmagic@columbus.rr.com>
 *		Gabe Yoder <gyoder@stny.rr.com>
 *		Stephan Aßmus <superstippi@gmx.de>
 */
#ifndef DRAWING_ENGINE_H_
#define DRAWING_ENGINE_H_


#include <Accelerant.h>
#include <OS.h>

#include <View.h>
#include <Font.h>
#include <Rect.h>
#include <Locker.h>
#include <Point.h>
#include <Gradient.h>
#include <ServerProtocolStructs.h>

#include <Screen.h>
#include "RGBColor.h"
#include <Region.h>
#include "PatternHandler.h"
#include "CursorHandler.h"
#include "DisplaySupport.h"
#include "DrawState.h"
#include "ServerBitmap.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#include "HWInterface.h"


class BPoint;
class BRect;
class BRegion;

class DrawState;
class Painter;
class ServerBitmap;
class ServerCursor;
class ServerFont;

#ifndef ROUND
	#define ROUND(a)	( (long)(a+.5) )
#endif

/*!
	\brief Data structure for passing cursor information to hardware drivers.
*/
typedef struct
{
	uchar *xormask, *andmask;
	int32 width, height;
	int32 hotx, hoty;

} cursor_data;

#ifndef HOOK_DEFINE_CURSOR

#define HOOK_DEFINE_CURSOR		0
#define HOOK_MOVE_CURSOR		1
#define HOOK_SHOW_CURSOR		2
#define HOOK_DRAW_LINE_8BIT		3
#define HOOK_DRAW_LINE_16BIT	12
#define HOOK_DRAW_LINE_32BIT	4
#define HOOK_DRAW_RECT_8BIT		5
#define HOOK_DRAW_RECT_16BIT	13
#define HOOK_DRAW_RECT_32BIT	6
#define HOOK_BLIT				7
#define HOOK_DRAW_ARRAY_8BIT	8
#define HOOK_DRAW_ARRAY_16BIT	14	// Not implemented in current R5 drivers
#define HOOK_DRAW_ARRAY_32BIT	9
#define HOOK_SYNC				10
#define HOOK_INVERT_RECT		11

#endif

class DrawingEngine;

typedef void (DrawingEngine::* SetPixelFuncType)(int x, int y);
typedef void (DrawingEngine::* SetHorizontalLineFuncType)(int xstart, int xend, int y);
typedef void (DrawingEngine::* SetVerticalLineFuncType)(int x, int ystart, int yend);
typedef void (DrawingEngine::* SetRectangleFuncType)(int left, int top, int right, int bottom);

class DrawingEngine : public HWInterfaceListener {
public:
							DrawingEngine(HWInterface* interface = NULL);
	virtual					~DrawingEngine();

	// HWInterfaceListener interface
	virtual	void			FrameBufferChanged();

	// for "changing" hardware
			void			SetHWInterface(HWInterface* interface);

	virtual	void			SetCopyToFrontEnabled(bool enable);
			bool			CopyToFrontEnabled() const
								{ return fCopyToFront; }
	virtual	void			CopyToFront(/*const*/ BRegion& region);

	// locking
			bool			LockParallelAccess();
	virtual	bool			IsParallelAccessLocked() const;
			void			UnlockParallelAccess();

			bool			LockExclusiveAccess();
	virtual	bool			IsExclusiveAccessLocked() const;
			void			UnlockExclusiveAccess();

	// clipping for all drawing functions, passing a NULL region
	// will remove any clipping (drawing allowed everywhere)
	virtual	void			ConstrainClippingRegion(const BRegion* region);

			void			SuspendAutoSync();
			void			Sync();

	// drawing functions
	virtual	void			CopyRegion(/*const*/ BRegion* region,
								int32 xOffset, int32 yOffset);

	virtual	void			InvertRect(BRect r);

	virtual	void			DrawBitmap(ServerBitmap* bitmap,
								const BRect& bitmapRect, const BRect& viewRect,
								const DrawState *d);
	// drawing primitives
	virtual	void			DrawArc(BRect r, const float& angle,
								const float& span, const DrawState *d, bool filled);
	virtual	void			FillArc(BRect r, const float& angle,
								const float& span, const RGBColor &color);
	virtual	void			FillArc(const BRect &r, const float &angle, const float &span, const DrawState *d);

	void CopyBits(const BRect &src, const BRect &dest, const DrawState *d);
	void DrawBitmap(BRegion *region, ServerBitmap *bitmap, const BRect &source, const BRect &dest, const DrawState *d);
		// one more:
	void CopyRegionList(BList* list, BList* pList, int32 rCount, BRegion* clipReg);

			// drawing primitives


			void			DrawBezier(BPoint *pts, const DrawState *d,
								bool filled);

			void			DrawEllipse(BRect r, const DrawState *d,
								bool filled);

			void			DrawPolygon(BPoint *ptlist, int32 numpts,
								BRect bounds, const DrawState *d,
								bool filled, bool closed);

			void			DrawRoundRect(BRect r, float xrad,
								float yrad, const DrawState *d,
								bool filled);

			void			DrawShape(const BRect& bounds,
								int32 opcount, const uint32* oplist, 
								int32 ptcount, const BPoint* ptlist,
								const DrawState* d, bool filled);

			void			DrawTriangle(BPoint* pts, const BRect& bounds,
								const DrawState* d, bool filled);

	void FillBezier(BPoint *pts, const RGBColor &color);
	void FillBezier(BPoint *pts, const DrawState *d);
	void FillEllipse(const BRect &r, const RGBColor &color);
	void FillEllipse(const BRect &r, const DrawState *d);
	void FillPolygon(BPoint *ptlist, int32 numpts, const BRect &bounds, const RGBColor &color);
	void FillPolygon(BPoint *ptlist, int32 numpts, const BRect &bounds, const DrawState *d);
	void FillRect(const BRect &r, const RGBColor &color);
	void FillRect(const BRect &r, const DrawState *d);
	void FillRegion(BRegion &r, const RGBColor &color);
	void FillRegion(BRegion &r, const DrawState *d);
	void FillRoundRect(const BRect &r, const float &xrad, const float &yrad, const RGBColor &color);
	void FillRoundRect(const BRect &r, const float &xrad, const float &yrad, const DrawState *d);
	void FillShape(const BRect &bounds, const int32 &opcount, const int32 *oplist, 
			const int32 &ptcount, const BPoint *ptlist, const DrawState *d);
	void FillTriangle(BPoint *pts, const BRect &bounds, const RGBColor &color);
	void FillTriangle(BPoint *pts, const BRect &bounds, const DrawState *d);

	ServerCursor *Cursor(void);
	void HideCursor(void);
	bool IsCursorHidden(void);
	void MoveCursorTo(const float &x, const float &y);
	void ShowCursor(void);
	void ObscureCursor(void);
	void SetCursor(ServerCursor *cursor);

	void StrokeArc(const BRect &r, const float &angle, const float &span, const RGBColor &color);
	void StrokeArc(const BRect &r, const float &angle, const float &span, const DrawState *d);
	void StrokeBezier(BPoint *pts, const RGBColor &color);
	void StrokeBezier(BPoint *pts, const DrawState *d);
	void StrokeEllipse(const BRect &r, const RGBColor &color);
	void StrokeEllipse(const BRect &r, const DrawState *d);
	void StrokeLine(const BPoint &start, const BPoint &end, const RGBColor &color);
	void StrokeLine(const BPoint &start, const BPoint &end, const DrawState *d);
	void StrokePoint(const BPoint &pt, const RGBColor &color);
	void StrokePoint(const BPoint &pt, const DrawState *d);
	void StrokePolygon(BPoint *ptlist, int32 numpts, const BRect &bounds, const RGBColor &color, bool is_closed=true);
	void StrokePolygon(BPoint *ptlist, int32 numpts, const BRect &bounds, const DrawState *d, bool is_closed=true);
	void StrokeRect(const BRect &r, const RGBColor &color);
	void StrokeRect(const BRect &r, const DrawState *d);
	void StrokeRegion(BRegion &r, const RGBColor &color);
	void StrokeRegion(BRegion &r, const DrawState *d);
	void StrokeRoundRect(const BRect &r, const float &xrad, const float &yrad, const RGBColor &color);
	void StrokeRoundRect(const BRect &r, const float &xrad, const float &yrad, const DrawState *d);
	void StrokeShape(const BRect &bounds, const int32 &opcount, const int32 *oplist, 
			const int32 &ptcount, const BPoint *ptlist, const DrawState *d);
	void StrokeTriangle(BPoint *pts, const BRect &bounds, const RGBColor &color);
	void StrokeTriangle(BPoint *pts, const BRect &bounds, const DrawState *d);

	void GetMode(display_mode *mode);

	// Font-related calls
	
	// DrawState is NOT const because this call updates the pen position in the passed DrawState
	void DrawString(const char *string, const int32 &length, const BPoint &pt, DrawState *d);
	void DrawString(const char *string, const int32 &length, const BPoint &pt, const RGBColor &color, escapement_delta *delta=NULL);

	float StringWidth(const char *string, int32 length, const DrawState *d);
	float StringHeight(const char *string, int32 length, const DrawState *d);

	void GetBoundingBoxes(const char *string, int32 count, font_metric_mode mode, 
			escapement_delta *delta, BRect *rectarray, const DrawState *d);
	void GetEscapements(const char *string, int32 charcount, escapement_delta *delta, 
			escapement_delta *escapements, escapement_delta *offsets, const DrawState *d);
	void GetEdges(const char *string, int32 charcount, edge_info *edgearray, const DrawState *d);
	void GetHasGlyphs(const char *string, int32 charcount, bool *hasarray);
	void GetTruncatedStrings(const char **instrings, const int32 &stringcount, const uint32 &mode, 
			const float &maxwidth, char **outstrings);
	
	bool IsCursorObscured(bool state);
	
	
	// Virtual methods which need to be implemented by each subclass
	virtual bool Initialize(void);
	virtual void Shutdown(void);

	// These two will rarely be implemented by subclasses, but it still needs to be possible
	virtual bool Lock(bigtime_t timeout=B_INFINITE_TIMEOUT);
	virtual void Unlock(void);

	virtual status_t SetMode(const display_mode &mode);

	virtual bool DumpToFile(const char *path);
	virtual ServerBitmap *DumpToBitmap(void);
	virtual void StrokeLineArray(const int32 &numlines, const ViewLineArrayInfo *data, const DrawState *d);

	virtual status_t SetDPMSMode(const uint32 &state);
	virtual uint32 DPMSMode(void) const;
	virtual uint32 DPMSCapabilities(void) const;
	virtual status_t GetDeviceInfo(accelerant_device_info *info);
	virtual status_t GetModeList(display_mode **mode_list, uint32 *count);
	virtual status_t GetPixelClockLimits(display_mode *mode, uint32 *low, uint32 *high);
	virtual status_t GetTimingConstraints(display_timing_constraints *dtc);
	virtual status_t ProposeMode(display_mode *candidate, const display_mode *low, const display_mode *high);
	virtual status_t WaitForRetrace(bigtime_t timeout=B_INFINITE_TIMEOUT);

protected:
friend class Layer;
friend class WindowLayer;
friend class CursorHandler;

	virtual void HLinePatternThick(int32 x1, int32 x2, int32 y);
	virtual void VLinePatternThick(int32 x, int32 y1, int32 y2);
	virtual void SetThickPatternPixel(int x, int y);

	// Blit functions specific to FreeType2 glyph copying. These probably could be replaced with
	// more generic functions, but these are written and can be replaced later.
	void BlitMono2RGB32(FT_Bitmap *src, const BPoint &pt, const DrawState *d);
	void BlitGray2RGB32(FT_Bitmap *src, const BPoint &pt, const DrawState *d);
	
	// Two functions for gaining direct access to the framebuffer of a child class. This removes the need
	// for a set of glyph-blitting virtual functions for each driver.
	virtual bool AcquireBuffer(FBBitmap *bmp);
	virtual void ReleaseBuffer(void);
	
	// This is for drivers which are internally double buffered and calling this will cause the real
	// framebuffer to be updated
	virtual void Invalidate(const BRect &r);
	
	void FillBezier(BPoint *pts, DrawingEngine* driver, SetHorizontalLineFuncType setLine);
	void FillRegion(BRegion &r, DrawingEngine* driver, SetRectangleFuncType setRect);
	void StrokeArc(const BRect &r, const float &angle, const float &span, DrawingEngine* driver, SetPixelFuncType setPixel);
	void StrokeBezier(BPoint *pts, DrawingEngine* driver, SetPixelFuncType setPixel);
	void StrokeEllipse(const BRect &r, DrawingEngine* driver, SetPixelFuncType setPixel);
	void StrokeLine(const BPoint &start, const BPoint &end, DrawingEngine* driver, SetPixelFuncType setPixel);

	// Support functions for the rest of the driver
	virtual void Blit(const BRect &src, const BRect &dest, const DrawState *d);
	virtual void FillSolidRect(const BRect &rect, const RGBColor &color);
	virtual void FillPatternRect(const BRect &rect, const DrawState *d);
	virtual void StrokeSolidLine(int32 x1, int32 y1, int32 x2, int32 y2, const RGBColor &color);
	virtual void StrokePatternLine(int32 x1, int32 y1, int32 x2, int32 y2, const DrawState *d);
	virtual void StrokeSolidRect(const BRect &rect, const RGBColor &color);
	virtual void CopyBitmap(ServerBitmap *bitmap, const BRect &source, const BRect &dest, const DrawState *d);
	virtual void CopyToBitmap(ServerBitmap *target, const BRect &source);
		// temporarily virtual - until clipping code is added in DrawingEngine

	PatternHandler fDrawPattern;
	RGBColor fDrawColor;
	int fLineThickness;

	BLocker *_locker;

	uint32 fDPMSState;
	uint32 fDPMSCaps;
	accelerant_device_info fAccDeviceInfo;
	display_mode fDisplayMode;
	
	CursorHandler *fCursorHandler;
	
			Painter*		fPainter;
			HWInterface*	fGraphicsCard;
			uint32			fAvailableHWAccleration;
			int32			fSuspendSyncLevel;
			bool			fCopyToFront;

	DrawState fDrawData;
};

#endif
