/*
 * Copyright 2005, Haiku.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Bill Hayden <hayden@haydentech.com>
 */
#ifndef SDL_INTERFACE_H
#define SDL_INTERFACE_H


#include "BitmapHWInterface.h"
#include <Region.h>	// for clipping_rect definition
#include "RGBColor.h"

#include <SDL.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL_render.h>


namespace BPrivate {
	class PortLink;
}

class SDLInterface : public BitmapHWInterface {
 public:
							SDLInterface();
	virtual					~SDLInterface();

	virtual	status_t		Initialize();
	virtual	status_t		Shutdown();

	// query for available hardware accleration and perform it
	// (Initialize() must have been called already)
	virtual	uint32			AvailableHWAcceleration() const
									{ return HW_ACC_COPY_REGION & HW_ACC_FILL_REGION; }

	virtual	void			CopyRegion(const clipping_rect* sortedRectList,
							uint32 count,
							int32 xOffset,
							int32 yOffset);
	virtual	void			FillRegion(/*const*/ BRegion& region,
							 const rgb_color& color,
							 bool autoSync);
	virtual	status_t		Invalidate(const BRect& frame);

 protected:
	virtual void			_InvalidateSDL(const SDL_Rect &r);
			status_t		SDLInitialize();

	sem_id					drawsem;

	SDL_Window*			mWindow;
	SDL_Surface*		mScreen;
	BPrivate::PortLink*				serverlink;
};

class SDLBitmap : public ServerBitmap {
public:
			SDLBitmap(BRect rect, color_space space,
					uint32 flags, int32 bytesperline = -1,
					screen_id screen = B_MAIN_SCREEN_ID);
};

#endif // SDL_INTERFACE_H
