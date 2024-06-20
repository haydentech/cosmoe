/*
 *		Bill Hayden <hayden@haydentech.com>
 */
#ifndef X11_INTERFACE_H
#define X11_INTERFACE_H


#include "BitmapHWInterface.h"
//#include <Region.h>	// for clipping_rect definition
#include "RGBColor.h"

namespace X11 {
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#undef ScreenCount
};



class X11Interface : public BitmapHWInterface {
 public:
							X11Interface();
	virtual					~X11Interface();

	virtual	status_t		Initialize();

	// query for available hardware accleration and perform it
	// (Initialize() must have been called already)

	virtual	status_t		Invalidate(const BRect& frame);

	int							screen;
	int							depth;
	unsigned long				window_mask;

	X11::Display*				display;
	X11::Window					xcanvas;
	X11::XImage*				image;
	X11::Colormap				colormap;
	X11::GC						image_gc;
	X11::XImage*				ximage;
	X11::XSetWindowAttributes	window_attributes;
	X11::XSizeHints				window_hints;
	X11::Pixmap					xpixmap;

 protected:
	sem_id					drawsem;
};


#endif // X11_INTERFACE_H
