#ifndef SDL_BITMAP_DRAWING_ENGINE_H
#define SDL_BITMAP_DRAWING_ENGINE_H

#include "BitmapDrawingEngine.h"

#include <SDL.h>

namespace BPrivate {
	class PortLink;
}


/*!
	\class FBBitmap DrawingEngine.h
	\brief Class used for easily passing around information about the framebuffer
*/
class FBBitmap : public ServerBitmap
{
public:
	FBBitmap(void) : ServerBitmap(BRect(0,0,0,0),B_NO_COLOR_SPACE,0) { }
	~FBBitmap(void) { }
	void SetBytesPerRow(const int32 &bpr) { fBytesPerRow=bpr; }
	void SetSpace(const color_space &space) { fSpace=space; }
	
	// WARNING: - for some reason ServerBitmap adds 1 to the width and height. We do that also.
	void SetSize(const int32 &w, const int32 &h) { fWidth=w+1; fHeight=h+1; }
	void SetBuffer(void *ptr) { fBuffer=(uint8*)ptr; }
	//void SetBitsPerPixel(color_space space,int32 bytesperline) { _HandleSpace(space,bytesperline); }
	void ShallowCopy(const FBBitmap *from)
	{
		ServerBitmap::ShallowCopy((ServerBitmap*)from);
		SetBuffer(from->Bits());
	}
};


class SDLBitmapDrawingEngine : public BitmapDrawingEngine {
public:
								SDLBitmapDrawingEngine();
	virtual						~SDLBitmapDrawingEngine();

	virtual bool Initialize(void);
	virtual	status_t			SetSize(int32 newWidth, int32 newHeight);

protected:

	bool 						AcquireBuffer(FBBitmap *bmp);

private:

	virtual status_t			Invalidate(const BRect &r);
	void						_InvalidateSDL(const SDL_Rect &r);

	void DrawPixel(int x, int y, uint32 color);
	
	virtual void CopyRect(BRect rect, int32 xOffset, int32 yOffset);

	virtual void FillRect(BRect r, const rgb_color& col);
	virtual void StrokeLine(const BPoint& start, const BPoint& end, const rgb_color& color);
	virtual void StrokeRect(BRect rect, const rgb_color &color);
	virtual void FillPatternRect(const BRect &rect, const DrawState *d);
	//virtual void StrokePatternLine(int32 x1, int32 y1, int32 x2, int32 y2, const DrawState *d);
	//virtual void StrokeSolidRect(const BRect &rect, const rgb_color &color);
	virtual void CopyBitmap(ServerBitmap *bitmap, const BRect &source, const BRect &dest, const DrawState *d);

	SDL_Window*					mWindow;
	SDL_Surface*				mScreen;

	BPrivate::PortLink*			serverlink;
	sem_id						drawsem;
	bool						inited;

};

class SDLBitmap : public ServerBitmap {
public:
								SDLBitmap(uint8* bits,
									uint32 width, uint32 height,
									color_space format);

	virtual						~SDLBitmap();
};

#endif // SDL_BITMAP_DRAWING_ENGINE_H
