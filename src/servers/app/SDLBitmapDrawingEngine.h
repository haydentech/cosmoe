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

private:

	virtual void				Invalidate(const BRect &r);
	void						_InvalidateSDL(const SDL_Rect &r);


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
