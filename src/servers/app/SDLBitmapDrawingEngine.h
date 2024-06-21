#ifndef SDL_BITMAP_DRAWING_ENGINE_H
#define SDL_BITMAP_DRAWING_ENGINE_H

#include "BitmapDrawingEngine.h"

#include <SDL.h>

namespace BPrivate {
	class PortLink;
}


class SDLBitmapDrawingEngine : public BitmapDrawingEngine {
public:
								SDLBitmapDrawingEngine();
	virtual						~SDLBitmapDrawingEngine();

	virtual bool Initialize(void);
	virtual	status_t			SetSize(int32 newWidth, int32 newHeight);

protected:

	bool 						AcquireBuffer(FBBitmap *bmp);

private:

	virtual void				Invalidate(const BRect &r);
	void						_InvalidateSDL(const SDL_Rect &r);

	void DrawPixel(int x, int y, uint32 color);
	
	virtual void Blit(const BRect &src, const BRect &dest, const DrawState *d);
	virtual void FillSolidRect(const BRect &rect, const rgb_color &color);
	virtual void FillPatternRect(const BRect &rect, const DrawState *d);
	virtual void StrokeSolidLine(int32 x1, int32 y1, int32 x2, int32 y2, const rgb_color &color);
	virtual void StrokePatternLine(int32 x1, int32 y1, int32 x2, int32 y2, const DrawState *d);
	virtual void StrokeSolidRect(const BRect &rect, const rgb_color &color);
	virtual void CopyBitmap(ServerBitmap *bitmap, const BRect &source, const BRect &dest, const DrawState *d);

	SDL_Window*					mWindow;
	SDL_Surface*				mScreen;

	BPrivate::PortLink*			serverlink;
	sem_id						drawsem;

};

class SDLBitmap : public ServerBitmap {
public:
								SDLBitmap(uint8* bits,
									uint32 width, uint32 height,
									color_space format);

	virtual						~SDLBitmap();
};

#endif // SDL_BITMAP_DRAWING_ENGINE_H
