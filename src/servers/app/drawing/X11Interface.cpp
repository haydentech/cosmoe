//------------------------------------------------------------------------------
//	Copyright (c) 2004, Bill Hayden
//
//	Permission is hereby granted, free of charge, to any person obtaining a
//	copy of this software and associated documentation files (the "Software"),
//	to deal in the Software without restriction, including without limitation
//	the rights to use, copy, modify, merge, publish, distribute, sublicense,
//	and/or sell copies of the Software, and to permit persons to whom the
//	Software is furnished to do so, subject to the following conditions:
//
//	The above copyright notice and this permission notice shall be included in
//	all copies or substantial portions of the Software.
//
//	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//	FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//	DEALINGS IN THE SOFTWARE.
//
//	File Name:		X11Driver.cpp
//	Author:			Bill Hayden <hayden@haydentech.com>
//	Description:	Display driver which renders into an X11 window
//
//------------------------------------------------------------------------------

#include <string.h>
#include <stdio.h>
#include <pthread.h>

#include <SupportDefs.h>
#include <Message.h>

#include <Bitmap.h>

#include "X11Interface.h"
#include "ServerBitmap.h"
#include "ServerConfig.h"
#include "RectUtils.h"
#include "RenderingBuffer.h"

#include <PortLink.h>
#include <ServerProtocol.h>

#define DEBUG_X11_DRIVER

#ifdef DEBUG_X11_DRIVER
	#define STRACE(a) printf(a)
#else
	#define STRACE(a) /* nothing */
#endif

// Hardcode 800x600x32 for now
#define X11DRIVER_WIDTH		800
#define X11DRIVER_HEIGHT	600
#define X11DRIVER_DEPTH		32

using namespace X11;

/*!
	\brief Sets up internal variables needed by the X11Driver
*/
X11Interface::X11Interface(void) : BitmapHWInterface(new UtilityBitmap(BRect(0, 0, X11DRIVER_WIDTH - 1, X11DRIVER_HEIGHT - 1), B_RGBA32, 0))
{
	STRACE( "X11Interface constructor\n" );

	drawsem = create_sem(1, "X11 draw semaphore");
}


X11Interface::~X11Interface()
{
	STRACE("X11Driver::~X11Driver\n");
}

/*!
	\brief Translate X11 events into appserver events, as if they came
			right from the actual Input Server
*/
void XEventTranslator(void *arg)
{
	X11Interface *driver= (X11Interface*)arg;
	XEvent event;
	int quit = 0;
	float x, y;
	uint32 buttons = 0;
	port_id fInputPort = create_port(200, SERVER_INPUT_PORT);
	status_t err = B_OK;
	
	STRACE("Driver::EventTranslator\n");
	
	/* Loop until a QUIT event is found */
	while( !quit )
	{
		/* Poll for events */
		XNextEvent(driver->display, &event);
		switch(event.type)
		{
			case MotionNotify:
			{
				x=(float)((XMotionEvent *) &event)->x;
				y=(float)((XMotionEvent *) &event)->y;

				STRACE("Driver->MouseMoved\n");
				//fprintf(stderr, "Initialize thread id = %lu\n", pthread_self());

				BMessage mm(B_MOUSE_MOVED);
				mm.AddInt64("when", real_time_clock());
				mm.AddInt32("buttons", buttons);
				mm.AddPoint("where", BPoint(x,y));
				
				size_t length = mm.FlattenedSize();
				char stream[length];

				err = mm.Flatten(stream, length);
				if (err == B_OK)
					err = write_port(fInputPort, 0, stream, length);
				break;
			}

			case Expose:{
				STRACE("Draw\n");
				//driver->GetTarget()->Bounds().PrintToStream();
				driver->Invalidate(driver->FrontBuffer()->Bounds());
				break;
			}

			case KeyPress:{
				STRACE("KeyDown\n");
				break;
			}

			case KeyRelease:{
				STRACE("KeyUp\n");
				break;
			}

			case ButtonPress:
			case ButtonRelease:
			{
				STRACE("MouseDown/Up\n");
				uint32 buttons = ((XButtonEvent *) &event)->button;
				uint32 clicks = 1;		// can't get the # of clicks without a *lot* of extra work :(

				BMessage mc(event.type == ButtonPress ? B_MOUSE_DOWN : B_MOUSE_UP);
				mc.AddInt64("when", real_time_clock());
				mc.AddInt32("buttons", buttons);
				mc.AddPoint("where", BPoint(x,y));
				mc.AddInt32("clicks", clicks);
				
				size_t length = mc.FlattenedSize();
				char stream[length];

				err = mc.Flatten(stream, length);
				if (err == B_OK)
					err = write_port(fInputPort, 0, stream, length);
				break;
			}
			
			case DestroyNotify:
				STRACE("DestroyNotify\n");
				quit = 1;
				break;

			case UnmapNotify:
				STRACE("UnmapNotify\n");
				quit = 1;
				break;

			default:
				fprintf(stderr, "Unhandled X11 event: %d\n", event.type);
				break;
		}

		if (err != B_OK)
			printf("X11 event handler error %ld\n", err);

		X11::XFlush(driver->display);
	}
	
	BPrivate::PortLink applink(find_port(SERVER_PORT_NAME));

	STRACE("Driver: sending B_QUIT_REQUESTED message\n");
	applink.StartMessage(B_QUIT_REQUESTED);
	applink.Flush();

	STRACE("Leaving EventTranslator\n");
}


/*!
	\brief Opens the first available graphics device and initializes it
	\return B_OK on success or an appropriate error message on failure.
*/
status_t
X11Interface::Initialize(void)
{
	STRACE("X11Driver::Initialize\n");

	X11::XInitThreads();
	display = X11::XOpenDisplay(NULL);
	STRACE("XOpenDisplay obtained display\n");

	screen = DefaultScreen(display);
	depth = DefaultDepth(display, screen);
	window_attributes.border_pixel = BlackPixel(display, screen);
	window_attributes.background_pixel = WhitePixel(display, screen);
	window_attributes.override_redirect = True;
	window_mask = CWBackPixel | CWBorderPixel;

	xcanvas = X11::XCreateWindow(display, DefaultRootWindow(display),
				0, 0, X11DRIVER_WIDTH, X11DRIVER_HEIGHT, 0, depth,
				InputOutput, CopyFromParent,
				CWBackPixel | CWBorderPixel /*if you need noborder */
				/*| CWOverrideRedirect*/, &window_attributes);
	image_gc = XCreateGC(display, xcanvas, 0, 0);
	XMapWindow(display, xcanvas);
	X11::XFlush(display);
	XSelectInput(display, xcanvas, ExposureMask | PointerMotionMask |
			KeyPressMask | ButtonPressMask  | ButtonReleaseMask | StructureNotifyMask);
	STRACE("passed XCreateWindow\n");

	STRACE( "Driver::Initialize(): inited and video mode set\n" );

	// Create a new thread for mouse and key events
	pthread_t input_thread;
	pthread_create (&input_thread,
					NULL,
					(void *(*) (void *))&XEventTranslator,
					(void *) this);

	// UtilityBitmap* target;
	// target = new UtilityBitmap(BRect(0, 0,
	// 				X11DRIVER_WIDTH - 1, X11DRIVER_HEIGHT - 1),
	// 				B_RGBA32, 0);
	// SetTarget(target);

	// STRACE("passed SetTarget()\n");

	// Then we point XCreateImage at the UtilityBitmap's bits
	ximage = X11::XCreateImage (display, CopyFromParent, depth, ZPixmap, 0,
				(char*)FrontBuffer()->Bits(), X11DRIVER_WIDTH, X11DRIVER_HEIGHT,
				X11DRIVER_DEPTH, X11DRIVER_WIDTH * 4);

	xpixmap = X11::XCreatePixmap(display, xcanvas,
				X11DRIVER_WIDTH, X11DRIVER_HEIGHT, depth);
	X11::XPutImage(display, xpixmap, image_gc, ximage, 0, 0, 0, 0,
				X11DRIVER_WIDTH, X11DRIVER_HEIGHT);
	X11::XFlush(display);

	return true;
}


/*!
	\brief Refresh the framebuffer with the contents of the ServerBitmap
	\param r      The BRect rectangle to refresh
*/
status_t X11Interface::Invalidate(const BRect &r)
{
	fprintf(stderr, "Driver::Invalidate(%.0f, %.0f, %.0f, %.0f)\n", r.left, r.top, r.right, r.bottom);

	// Limit damage rect to screen coordinates to avoid writing out of bound
	BRect damage(r & BRect(0, 0, FrontBuffer()->Bounds().Width(), FrontBuffer()->Bounds().Height()));

	
	acquire_sem(drawsem);
//	X11::XPutImage(display, xcanvas, image_gc, ximage, damage.left, damage.top,
	X11::XPutImage(display, xpixmap, image_gc, ximage, damage.left, damage.top,
												  damage.left, damage.top,
												  damage.IntegerWidth() + 1,
												  damage.IntegerHeight() + 1);
	release_sem(drawsem);
	return B_OK;
}


