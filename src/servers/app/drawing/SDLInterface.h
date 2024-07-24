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


class SDLInterface : public BitmapHWInterface {
 public:
								SDLInterface();
	virtual						~SDLInterface();

	virtual	status_t			Initialize();
	virtual	status_t			Shutdown();

	// query for available hardware accleration and perform it
	// (Initialize() must have been called already)
	virtual	uint32				AvailableHWAcceleration() const
									{ return HW_ACC_COPY_REGION | HW_ACC_FILL_REGION; }

	virtual	void				CopyRegion(const clipping_rect* sortedRectList,
									uint32 count, int32 xOffset, int32 yOffset);
	virtual	void				FillRegion(/*const*/ BRegion& region,
									const rgb_color& color, bool autoSync);

	virtual	status_t			Invalidate(const BRect& frame);
	virtual	void				_CopyBackToFront(/*const*/ BRegion& region);

	virtual	status_t			SetMode(const display_mode& mode);

	virtual void				GetMode(display_mode* mode);
	virtual	status_t			GetModeList(display_mode** modes, uint32 *count);

	virtual	status_t			GetPreferredMode(display_mode* mode);

 protected:
			status_t			SDLInitialize();

			status_t			_UpdateModeList();

	sem_id						drawsem;

	int							fModeCount;
	display_mode*				fModeList;

	SDL_Window*					mWindow;
	SDL_Surface*				mScreen;
};


#endif // SDL_INTERFACE_H
