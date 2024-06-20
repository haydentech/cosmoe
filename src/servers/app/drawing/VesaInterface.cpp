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
//	File Name:		VesaInterface.cpp
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

#include "vesa_defs.h"
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <linux/fb.h>

include <isa_io.h>

//#include <vesa_gfx.h>

#include "VesaInterface.h"
#include "ServerBitmap.h"
#include "ServerConfig.h"
#include "RectUtils.h"
#include "RenderingBuffer.h"

#include <PortLink.h>
#include <ServerProtocol.h>

#define DEBUG_VESA_DRIVER

#ifdef DEBUG_VESA_DRIVER
	#define STRACE(a) printf(a)
#else
	#define STRACE(a) /* nothing */
#endif

#define dbprintf printf

#define PAGE_MASK	(~(B_PAGE_SIZE-1))

#define RAS_OFFSET8( ptr, x, y, bpl )  (((uint8*)(ptr)) + (x) + (y) * (bpl))
#define RAS_OFFSET16( ptr, x, y, bpl ) ((uint16*)(((uint8*)(ptr)) + (x*2) + (y) * (bpl)))
#define RAS_OFFSET32( ptr, x, y, bpl ) ((uint32*)(((uint8*)(ptr)) + (x*4) + (y) * (bpl)))

rgb_color g_asDefaultPallette[] = {
    { 0x00, 0x00, 0x00, 0x00 },        // 0
    { 0x08, 0x08, 0x08, 0x00 },
    { 0x10, 0x10, 0x10, 0x00 },
    { 0x18, 0x18, 0x18, 0x00 },
    { 0x20, 0x20, 0x20, 0x00 },
    { 0x28, 0x28, 0x28, 0x00 },        // 5
    { 0x30, 0x30, 0x30, 0x00 },
    { 0x38, 0x38, 0x38, 0x00 },
    { 0x40, 0x40, 0x40, 0x00 },
    { 0x48, 0x48, 0x48, 0x00 },
    { 0x50, 0x50, 0x50, 0x00 },        // 10
    { 0x58, 0x58, 0x58, 0x00 },
    { 0x60, 0x60, 0x60, 0x00 },
    { 0x68, 0x68, 0x68, 0x00 },
    { 0x70, 0x70, 0x70, 0x00 },
    { 0x78, 0x78, 0x78, 0x00 },        // 15
    { 0x80, 0x80, 0x80, 0x00 },
    { 0x88, 0x88, 0x88, 0x00 },
    { 0x90, 0x90, 0x90, 0x00 },
    { 0x98, 0x98, 0x98, 0x00 },
    { 0xa0, 0xa0, 0xa0, 0x00 },        // 20
    { 0xa8, 0xa8, 0xa8, 0x00 },
    { 0xb0, 0xb0, 0xb0, 0x00 },
    { 0xb8, 0xb8, 0xb8, 0x00 },
    { 0xc0, 0xc0, 0xc0, 0x00 },
    { 0xc8, 0xc8, 0xc8, 0x00 },        // 25
    { 0xd0, 0xd0, 0xd0, 0x00 },
    { 0xd9, 0xd9, 0xd9, 0x00 },
    { 0xe2, 0xe2, 0xe2, 0x00 },
    { 0xeb, 0xeb, 0xeb, 0x00 },
    { 0xf5, 0xf5, 0xf5, 0x00 },        // 30
    { 0xfe, 0xfe, 0xfe, 0x00 },
    { 0x00, 0x00, 0xff, 0x00 },
    { 0x00, 0x00, 0xe5, 0x00 },
    { 0x00, 0x00, 0xcc, 0x00 },
    { 0x00, 0x00, 0xb3, 0x00 },        // 35
    { 0x00, 0x00, 0x9a, 0x00 },
    { 0x00, 0x00, 0x81, 0x00 },
    { 0x00, 0x00, 0x69, 0x00 },
    { 0x00, 0x00, 0x50, 0x00 },
    { 0x00, 0x00, 0x37, 0x00 },        // 40
    { 0x00, 0x00, 0x1e, 0x00 },
    { 0xff, 0x00, 0x00, 0x00 },
    { 0xe4, 0x00, 0x00, 0x00 },
    { 0xcb, 0x00, 0x00, 0x00 },
    { 0xb2, 0x00, 0x00, 0x00 },        // 45
    { 0x99, 0x00, 0x00, 0x00 },
    { 0x80, 0x00, 0x00, 0x00 },
    { 0x69, 0x00, 0x00, 0x00 },
    { 0x50, 0x00, 0x00, 0x00 },
    { 0x37, 0x00, 0x00, 0x00 },        // 50
    { 0x1e, 0x00, 0x00, 0x00 },
    { 0x00, 0xff, 0x00, 0x00 },
    { 0x00, 0xe4, 0x00, 0x00 },
    { 0x00, 0xcb, 0x00, 0x00 },
    { 0x00, 0xb2, 0x00, 0x00 },        // 55
    { 0x00, 0x99, 0x00, 0x00 },
    { 0x00, 0x80, 0x00, 0x00 },
    { 0x00, 0x69, 0x00, 0x00 },
    { 0x00, 0x50, 0x00, 0x00 },
    { 0x00, 0x37, 0x00, 0x00 },        // 60
    { 0x00, 0x1e, 0x00, 0x00 },
    { 0x00, 0x98, 0x33, 0x00 },
    { 0xff, 0xff, 0xff, 0x00 },
    { 0xcb, 0xff, 0xff, 0x00 },
    { 0xcb, 0xff, 0xcb, 0x00 },        // 65
    { 0xcb, 0xff, 0x98, 0x00 },
    { 0xcb, 0xff, 0x66, 0x00 },
    { 0xcb, 0xff, 0x33, 0x00 },
    { 0xcb, 0xff, 0x00, 0x00 },
    { 0x98, 0xff, 0xff, 0x00 },
    { 0x98, 0xff, 0xcb, 0x00 },
    { 0x98, 0xff, 0x98, 0x00 },
    { 0x98, 0xff, 0x66, 0x00 },
    { 0x98, 0xff, 0x33, 0x00 },
    { 0x98, 0xff, 0x00, 0x00 },
    { 0x66, 0xff, 0xff, 0x00 },
    { 0x66, 0xff, 0xcb, 0x00 },
    { 0x66, 0xff, 0x98, 0x00 },
    { 0x66, 0xff, 0x66, 0x00 },
    { 0x66, 0xff, 0x33, 0x00 },
    { 0x66, 0xff, 0x00, 0x00 },
    { 0x33, 0xff, 0xff, 0x00 },
    { 0x33, 0xff, 0xcb, 0x00 },
    { 0x33, 0xff, 0x98, 0x00 },
    { 0x33, 0xff, 0x66, 0x00 },
    { 0x33, 0xff, 0x33, 0x00 },
    { 0x33, 0xff, 0x00, 0x00 },
    { 0xff, 0x98, 0xff, 0x00 },
    { 0xff, 0x98, 0xcb, 0x00 },
    { 0xff, 0x98, 0x98, 0x00 },
    { 0xff, 0x98, 0x66, 0x00 },
    { 0xff, 0x98, 0x33, 0x00 },
    { 0xff, 0x98, 0x00, 0x00 },
    { 0x00, 0x66, 0xff, 0x00 },
    { 0x00, 0x66, 0xcb, 0x00 },
    { 0xcb, 0xcb, 0xff, 0x00 },
    { 0xcb, 0xcb, 0xcb, 0x00 },
    { 0xcb, 0xcb, 0x98, 0x00 },
    { 0xcb, 0xcb, 0x66, 0x00 },
    { 0xcb, 0xcb, 0x33, 0x00 },
    { 0xcb, 0xcb, 0x00, 0x00 },
    { 0x98, 0xcb, 0xff, 0x00 },
    { 0x98, 0xcb, 0xcb, 0x00 },
    { 0x98, 0xcb, 0x98, 0x00 },
    { 0x98, 0xcb, 0x66, 0x00 },
    { 0x98, 0xcb, 0x33, 0x00 },
    { 0x98, 0xcb, 0x00, 0x00 },
    { 0x66, 0xcb, 0xff, 0x00 },
    { 0x66, 0xcb, 0xcb, 0x00 },
    { 0x66, 0xcb, 0x98, 0x00 },
    { 0x66, 0xcb, 0x66, 0x00 },
    { 0x66, 0xcb, 0x33, 0x00 },
    { 0x66, 0xcb, 0x00, 0x00 },
    { 0x33, 0xcb, 0xff, 0x00 },
    { 0x33, 0xcb, 0xcb, 0x00 },
    { 0x33, 0xcb, 0x98, 0x00 },
    { 0x33, 0xcb, 0x66, 0x00 },
    { 0x33, 0xcb, 0x33, 0x00 },
    { 0x33, 0xcb, 0x00, 0x00 },
    { 0xff, 0x66, 0xff, 0x00 },
    { 0xff, 0x66, 0xcb, 0x00 },
    { 0xff, 0x66, 0x98, 0x00 },
    { 0xff, 0x66, 0x66, 0x00 },
    { 0xff, 0x66, 0x33, 0x00 },
    { 0xff, 0x66, 0x00, 0x00 },
    { 0x00, 0x66, 0x98, 0x00 },
    { 0x00, 0x66, 0x66, 0x00 },
    { 0xcb, 0x98, 0xff, 0x00 },
    { 0xcb, 0x98, 0xcb, 0x00 },
    { 0xcb, 0x98, 0x98, 0x00 },
    { 0xcb, 0x98, 0x66, 0x00 },
    { 0xcb, 0x98, 0x33, 0x00 },
    { 0xcb, 0x98, 0x00, 0x00 },
    { 0x98, 0x98, 0xff, 0x00 },
    { 0x98, 0x98, 0xcb, 0x00 },
    { 0x98, 0x98, 0x98, 0x00 },
    { 0x98, 0x98, 0x66, 0x00 },
    { 0x98, 0x98, 0x33, 0x00 },
    { 0x98, 0x98, 0x00, 0x00 },
    { 0x66, 0x98, 0xff, 0x00 },
    { 0x66, 0x98, 0xcb, 0x00 },
    { 0x66, 0x98, 0x98, 0x00 },
    { 0x66, 0x98, 0x66, 0x00 },
    { 0x66, 0x98, 0x33, 0x00 },
    { 0x66, 0x98, 0x00, 0x00 },
    { 0x33, 0x98, 0xff, 0x00 },
    { 0x33, 0x98, 0xcb, 0x00 },
    { 0x33, 0x98, 0x98, 0x00 },
    { 0x33, 0x98, 0x66, 0x00 },
    { 0x33, 0x98, 0x33, 0x00 },
    { 0x33, 0x98, 0x00, 0x00 },
    { 0xe6, 0x86, 0x00, 0x00 },
    { 0xff, 0x33, 0xcb, 0x00 },
    { 0xff, 0x33, 0x98, 0x00 },
    { 0xff, 0x33, 0x66, 0x00 },
    { 0xff, 0x33, 0x33, 0x00 },
    { 0xff, 0x33, 0x00, 0x00 },
    { 0x00, 0x66, 0x33, 0x00 },
    { 0x00, 0x66, 0x00, 0x00 },
    { 0xcb, 0x66, 0xff, 0x00 },
    { 0xcb, 0x66, 0xcb, 0x00 },
    { 0xcb, 0x66, 0x98, 0x00 },
    { 0xcb, 0x66, 0x66, 0x00 },
    { 0xcb, 0x66, 0x33, 0x00 },
    { 0xcb, 0x66, 0x00, 0x00 },
    { 0x98, 0x66, 0xff, 0x00 },
    { 0x98, 0x66, 0xcb, 0x00 },
    { 0x98, 0x66, 0x98, 0x00 },
    { 0x98, 0x66, 0x66, 0x00 },
    { 0x98, 0x66, 0x33, 0x00 },
    { 0x98, 0x66, 0x00, 0x00 },
    { 0x66, 0x66, 0xff, 0x00 },
    { 0x66, 0x66, 0xcb, 0x00 },
    { 0x66, 0x66, 0x98, 0x00 },
    { 0x66, 0x66, 0x66, 0x00 },
    { 0x66, 0x66, 0x33, 0x00 },
    { 0x66, 0x66, 0x00, 0x00 },
    { 0x33, 0x66, 0xff, 0x00 },
    { 0x33, 0x66, 0xcb, 0x00 },
    { 0x33, 0x66, 0x98, 0x00 },
    { 0x33, 0x66, 0x66, 0x00 },
    { 0x33, 0x66, 0x33, 0x00 },
    { 0x33, 0x66, 0x00, 0x00 },
    { 0xff, 0x00, 0xff, 0x00 },
    { 0xff, 0x00, 0xcb, 0x00 },
    { 0xff, 0x00, 0x98, 0x00 },
    { 0xff, 0x00, 0x66, 0x00 },
    { 0xff, 0x00, 0x33, 0x00 },
    { 0xff, 0xaf, 0x13, 0x00 },
    { 0x00, 0x33, 0xff, 0x00 },
    { 0x00, 0x33, 0xcb, 0x00 },
    { 0xcb, 0x33, 0xff, 0x00 },
    { 0xcb, 0x33, 0xcb, 0x00 },
    { 0xcb, 0x33, 0x98, 0x00 },
    { 0xcb, 0x33, 0x66, 0x00 },
    { 0xcb, 0x33, 0x33, 0x00 },
    { 0xcb, 0x33, 0x00, 0x00 },
    { 0x98, 0x33, 0xff, 0x00 },
    { 0x98, 0x33, 0xcb, 0x00 },
    { 0x98, 0x33, 0x98, 0x00 },
    { 0x98, 0x33, 0x66, 0x00 },
    { 0x98, 0x33, 0x33, 0x00 },
    { 0x98, 0x33, 0x00, 0x00 },
    { 0x66, 0x33, 0xff, 0x00 },
    { 0x66, 0x33, 0xcb, 0x00 },
    { 0x66, 0x33, 0x98, 0x00 },
    { 0x66, 0x33, 0x66, 0x00 },
    { 0x66, 0x33, 0x33, 0x00 },
    { 0x66, 0x33, 0x00, 0x00 },
    { 0x33, 0x33, 0xff, 0x00 },
    { 0x33, 0x33, 0xcb, 0x00 },
    { 0x33, 0x33, 0x98, 0x00 },
    { 0x33, 0x33, 0x66, 0x00 },
    { 0x33, 0x33, 0x33, 0x00 },
    { 0x33, 0x33, 0x00, 0x00 },
    { 0xff, 0xcb, 0x66, 0x00 },
    { 0xff, 0xcb, 0x98, 0x00 },
    { 0xff, 0xcb, 0xcb, 0x00 },
    { 0xff, 0xcb, 0xff, 0x00 },
    { 0x00, 0x33, 0x98, 0x00 },
    { 0x00, 0x33, 0x66, 0x00 },
    { 0x00, 0x33, 0x33, 0x00 },
    { 0x00, 0x33, 0x00, 0x00 },
    { 0xcb, 0x00, 0xff, 0x00 },
    { 0xcb, 0x00, 0xcb, 0x00 },
    { 0xcb, 0x00, 0x98, 0x00 },
    { 0xcb, 0x00, 0x66, 0x00 },
    { 0xcb, 0x00, 0x33, 0x00 },
    { 0xff, 0xe3, 0x46, 0x00 },
    { 0x98, 0x00, 0xff, 0x00 },
    { 0x98, 0x00, 0xcb, 0x00 },
    { 0x98, 0x00, 0x98, 0x00 },
    { 0x98, 0x00, 0x66, 0x00 },
    { 0x98, 0x00, 0x33, 0x00 },
    { 0x98, 0x00, 0x00, 0x00 },
    { 0x66, 0x00, 0xff, 0x00 },
    { 0x66, 0x00, 0xcb, 0x00 },
    { 0x66, 0x00, 0x98, 0x00 },
    { 0x66, 0x00, 0x66, 0x00 },
    { 0x66, 0x00, 0x33, 0x00 },
    { 0x66, 0x00, 0x00, 0x00 },
    { 0x33, 0x00, 0xff, 0x00 },
    { 0x33, 0x00, 0xcb, 0x00 },
    { 0x33, 0x00, 0x98, 0x00 },
    { 0x33, 0x00, 0x66, 0x00 },
    { 0x33, 0x00, 0x33, 0x00 },
    { 0x33, 0x00, 0x00, 0x00 },
    { 0xff, 0xcb, 0x33, 0x00 },
    { 0xff, 0xcb, 0x00, 0x00 },
    { 0xff, 0xff, 0x00, 0x00 },
    { 0xff, 0xff, 0x33, 0x00 },
    { 0xff, 0xff, 0x66, 0x00 },
    { 0xff, 0xff, 0x98, 0x00 },
    { 0xff, 0xff, 0xcb, 0x00 },
    { 0xff, 0xff, 0xff, 0xff }
};


static area_id        g_nFrameBufArea = -1;
/*!
	\brief Sets up internal variables needed by the VesaInterface
*/
VesaInterface::VesaInterface(void) : BitmapHWInterface(new UtilityBitmap(BRect(0, 0, VesaInterface_WIDTH - 1, VesaInterface_HEIGHT - 1), B_RGBA32, 0))
{
	STRACE( "VesaInterface constructor\n" );

	drawsem = create_sem(1, "X11 draw semaphore");
}


VesaInterface::~VesaInterface()
{
	STRACE("VesaInterface::~VesaInterface\n");
}

/*!
	\brief Translate events into appserver events, as if they came
			right from the actual Input Server
*/
void EventTranslator(void *arg)
{
	VesaInterface *driver= (VesaInterface*)arg;
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
		//XNextEvent(driver->display, &event);
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
VesaInterface::Initialize(void)
{
	STRACE("VesaInterface::Initialize\n");

	Vesa_Info_s      sVesaInfo;
	VESA_Mode_Info_s sModeInfo;
	int              nModeCount;
	int              i=0;

	strcpy( sVesaInfo.VesaSignature, "VBE2" );

	nModeCount = get_vesa_info( &sVesaInfo );

	if ( nModeCount <= 0 )
	{
		STRACE(( "Error: VesaDriver::InitModes() no VESA20 modes found\n" ));
		return( false );
	}

    printf( "Found %d vesa modes\n", nModeCount );

	int nPagedCount  = 0;
	int nPlanarCount = 0;
	int nBadCount    = 0;

	for( i = 0 ; i < nModeCount ; ++i )
	{
		get_vesa_mode_info( &sModeInfo, anModes[i] );

		if( sModeInfo.PhysBasePtr == 0 ) // We must have a linear frame buffer
		{
			nPagedCount++;
			continue;
		}

		if( sModeInfo.BitsPerPixel < 8 )
		{
			nPlanarCount++;
			continue;
		}

		if ( sModeInfo.NumberOfPlanes != 1 )
		{
			nPlanarCount++;
			continue;
		}

		if ( sModeInfo.BitsPerPixel != 15 && sModeInfo.BitsPerPixel != 16 && sModeInfo.BitsPerPixel != 32 )
		{
			nBadCount++;
			continue;
		}

		if ( sModeInfo.BitsPerPixel == 32 && sModeInfo.RedMaskSize == 8 && sModeInfo.GreenMaskSize == 8 && sModeInfo.BlueMaskSize == 8 &&
					sModeInfo.RedFieldPosition == 16 && sModeInfo.GreenFieldPosition == 8 && sModeInfo.BlueFieldPosition == 0 )
		{
			m_cModeList.push_back( VesaMode( sModeInfo.XResolution, sModeInfo.YResolution, sModeInfo.BytesPerScanLine,
											B_RGB32, anModes[i] | 0x4000, sModeInfo.PhysBasePtr ) );
		}
		else
		{
			printf( "Found unsupported video mode: %dx%d %d BPP %d BPL - %d:%d:%d, %d:%d:%d\n",
					sModeInfo.XResolution, sModeInfo.YResolution, sModeInfo.BitsPerPixel, sModeInfo.BytesPerScanLine,
					sModeInfo.RedMaskSize, sModeInfo.GreenMaskSize, sModeInfo.BlueMaskSize,
					sModeInfo.RedFieldPosition, sModeInfo.GreenFieldPosition, sModeInfo.BlueFieldPosition );
			continue;
		}
#if 1
		printf( "Mode %04x: %dx%d %d BPP %d BPL - %d:%d:%d, %d:%d:%d (%p)\n", anModes[i],
				sModeInfo.XResolution, sModeInfo.YResolution, sModeInfo.BitsPerPixel, sModeInfo.BytesPerScanLine,
				sModeInfo.RedMaskSize, sModeInfo.GreenMaskSize, sModeInfo.BlueMaskSize,
				sModeInfo.RedFieldPosition, sModeInfo.GreenFieldPosition, sModeInfo.BlueFieldPosition, (void*)sModeInfo.PhysBasePtr );
#endif        
	}
	printf( "Found total of %d VESA modes. Valid: %d, Paged: %d, Planar: %d, Bad: %d\n",
			nModeCount, m_cModeList.size(), nPagedCount, nPlanarCount, nBadCount );

//
	if ( InitModes() )
	{
		m_nFrameBufferSize = 1024 * 1024 * 4;
		g_nFrameBufArea = create_area("framebuffer", NULL, B_ANY_ADDRESS, m_nFrameBufferSize,
									B_NO_LOCK, (B_READ_AREA | B_WRITE_AREA));
		return( g_nFrameBufArea );
	}
	return true;
}


// /*!
// 	\brief Refresh the framebuffer with the contents of the ServerBitmap
// 	\param r      The BRect rectangle to refresh
// */
// status_t VesaInterface::Invalidate(const BRect &r)
// {
// 	fprintf(stderr, "Driver::Invalidate(%.0f, %.0f, %.0f, %.0f)\n", r.left, r.top, r.right, r.bottom);

// 	// Limit damage rect to screen coordinates to avoid writing out of bound
// 	BRect damage(r & BRect(0, 0, FrontBuffer()->Bounds().Width(), FrontBuffer()->Bounds().Height()));

	
// 	acquire_sem(drawsem);
// //FIXME what goes here
// 	release_sem(drawsem);
// 	return B_OK;
// }


int fb_fd = -1;
struct fb_fix_screeninfo fb_fixsi;
struct fb_var_screeninfo fb_varsi;
size_t fb_len;
void* fb_ptr = NULL;


int get_vesa_mode_info( VESA_Mode_Info_s* psVesaModeInfo, uint32 nModeNr )
{
	memset( psVesaModeInfo, 0, sizeof(VESA_Mode_Info_s) );
	if( nModeNr != 1 || !fb_ptr )
	{
		dbprintf( "get_vesa_mode_info(): unknown mode %u\n", nModeNr );
		return EINVAL;
	}

	psVesaModeInfo->ModeAttributes = 0; /* XXX */
	psVesaModeInfo->WinAAttributes = 0; /* XXX */
	psVesaModeInfo->WinBAttributes = 0; /* XXX */
	psVesaModeInfo->WinGranularity = 0; /* XXX */
	psVesaModeInfo->WinSize = 0;        /* XXX */
	psVesaModeInfo->WinASegment = 0;    /* XXX */
	psVesaModeInfo->WinBSegment = 0;    /* XXX */
	psVesaModeInfo->WinFuncPtr = 0;     /* XXX */

	psVesaModeInfo->BytesPerScanLine = fb_fixsi.line_length;
	psVesaModeInfo->XResolution = fb_varsi.xres;
	psVesaModeInfo->YResolution = fb_varsi.yres;
	psVesaModeInfo->XCharSize = 0; /* XXX */
	psVesaModeInfo->YCharSize = 0; /* XXX */
	psVesaModeInfo->NumberOfPlanes = 1;
	psVesaModeInfo->BitsPerPixel = fb_varsi.bits_per_pixel;
	psVesaModeInfo->NumberOfBanks = 1;
	psVesaModeInfo->MemoryModel = 0;
	psVesaModeInfo->BankSize = 0;
	psVesaModeInfo->NumberOfImagePages = 0;
	psVesaModeInfo->Reserved = 0;

	/* TODO: figure out other depth mappings .. probably need a table */
	switch( fb_varsi.bits_per_pixel )
	{
	case 16:
		psVesaModeInfo->RedMaskSize = 5; psVesaModeInfo->RedFieldPosition = 11;
		psVesaModeInfo->GreenMaskSize = 6; psVesaModeInfo->GreenFieldPosition = 5;
		psVesaModeInfo->BlueMaskSize = 5; psVesaModeInfo->BlueFieldPosition = 0;
		psVesaModeInfo->RsvdMaskSize = 0; psVesaModeInfo->RsvdFieldPosition = 0;
		psVesaModeInfo->DirectColorModeInfo = 0; /* XXX */
		break;
	case 32:
		psVesaModeInfo->RedMaskSize = 8; psVesaModeInfo->RedFieldPosition = 16;
		psVesaModeInfo->GreenMaskSize = 8; psVesaModeInfo->GreenFieldPosition = 8;
		psVesaModeInfo->BlueMaskSize = 8; psVesaModeInfo->BlueFieldPosition = 0;
		psVesaModeInfo->RsvdMaskSize = 8; psVesaModeInfo->RsvdFieldPosition = 24;
		psVesaModeInfo->DirectColorModeInfo = 0; /* XXX */
		break;
	default:
		dbprintf( "get_vesa_mode_info(): unknown color depth %i\n", fb_varsi.bits_per_pixel );
		return EINVAL;
	}
	psVesaModeInfo->DirectColorModeInfo = 0;
	psVesaModeInfo->PhysBasePtr = (int32)fb_ptr;
	psVesaModeInfo->OffScreenMemOffset = 0;
	psVesaModeInfo->OffScreenMemSize = 0;

	return 0;
}


int get_vesa_info( Vesa_Info_s* psVesaInfo )
{
	memset( psVesaInfo, 0, sizeof(Vesa_Info_s) );

	/* open framebuffer device */
	if( fb_fd == -1 )
	{
		const char* fbdev = "/dev/fb0";
		fb_fd = open( fbdev, O_RDWR );
		if( -1 == fb_fd )
		{
			dbprintf( "get_vesa_info(): open( %s ) failed: %s\n", fbdev, strerror(errno) );
			return 0;
		}
	}

	if( fb_ptr == NULL )
	{
		/* get relevant info */
		if( 0 == ioctl( fb_fd, FBIOGET_FSCREENINFO, &fb_fixsi ) &&
			0 == ioctl( fb_fd, FBIOGET_VSCREENINFO, &fb_varsi ) )
		{
			fb_len = fb_fixsi.line_length * fb_varsi.yres_virtual;

			/* okay now map the buffer */
			fb_ptr = mmap( NULL, fb_len, PROT_READ|PROT_WRITE, MAP_FILE|MAP_SHARED, fb_fd, 0 );
			if( MAP_FAILED == fb_ptr )
			{
				dbprintf( "get_vesa_info(): mmap() failed: %s\n", strerror(errno) );
				fb_ptr = NULL;
				return 0;
			}

			/* the psVesaInfo struct appears totally unused on return */
			memcpy( psVesaInfo->VesaSignature, "VBE2", 4 );
			psVesaInfo->VesaVersion = 2;
		}
	}

	return 1;
}


int set_vesa_mode( uint32 nMode )
{
	dbprintf( "set_vesa_mode( %u )\n", nMode );

	/* we only support one mode: 0 */
	return ( nMode == 0 );
}