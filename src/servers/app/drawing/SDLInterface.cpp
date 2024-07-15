//------------------------------------------------------------------------------
//	Copyright (c) 2004-2007, Bill Hayden
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
//	File Name:	SDLInterface.cpp
//	Author:		Bill Hayden <hayden@haydentech.com>
//	Description:	Display driver which renders to SDL
//
//------------------------------------------------------------------------------


#include <pthread.h>
#include <new>

#include <SupportDefs.h>
#include <Bitmap.h>
#include <AppDefs.h>
#include <Message.h>


#include "SDLInterface.h"
#include "ServerBitmap.h"
#include "ServerConfig.h"

#include <PortLink.h>
#include <ServerProtocol.h>

#include "BitmapBuffer.h"
#include "BBitmapBuffer.h"

#define DEBUG_SDL_DRIVER

#ifdef DEBUG_SDL_DRIVER
#include <stdio.h>
	#define STRACE(a) fprintf(stderr, a)
#else
	#define STRACE(x) ;
#endif

#define WIDTH 1024
#define HEIGHT 768



using std::nothrow;

static void RectToSDLRect(const BRect& r, SDL_Rect& outRect);
static void ClippingRectToSDLRect(const clipping_rect r, SDL_Rect& outRect);




/*!
	\brief Sets up internal variables needed by the SDLInterface
*/
SDLInterface::SDLInterface()
	:
	BitmapHWInterface(new UtilityBitmap(BRect(0, 0, WIDTH - 1, HEIGHT - 1), B_RGBA32, 0)),
	fModeCount(0),
	fModeList(NULL)
{
	STRACE( "SDLInterface constructor\n" );

	mScreen = NULL;

	fprintf(stderr, "SDLInterface::SDLInterface thread id = %lu\n", pthread_self());
	
	drawsem = create_sem(1, "SDL draw semaphore");
}


SDLInterface::~SDLInterface()
{
	STRACE( "SDLDriver destructor\n" );
}

uint32 GetModifiers(SDL_Event& event)
{
	// B_SHIFT_KEY			= 0x00000001,
	// B_COMMAND_KEY		= 0x00000002,
	// B_CONTROL_KEY		= 0x00000004,
	// B_CAPS_LOCK			= 0x00000008,
	// B_SCROLL_LOCK		= 0x00000010,
	// B_NUM_LOCK			= 0x00000020,
	// B_OPTION_KEY			= 0x00000040,
	// B_MENU_KEY			= 0x00000080,
	// B_LEFT_SHIFT_KEY		= 0x00000100,
	// B_RIGHT_SHIFT_KEY	= 0x00000200,
	// B_LEFT_COMMAND_KEY	= 0x00000400,
	// B_RIGHT_COMMAND_KEY	= 0x00000800,
	// B_LEFT_CONTROL_KEY	= 0x00001000,
	// B_RIGHT_CONTROL_KEY	= 0x00002000,
	// B_LEFT_OPTION_KEY	= 0x00004000,
	// B_RIGHT_OPTION_KEY	= 0x00008000

	uint32 mod = 0;

	if (event.key.keysym.mod & KMOD_LCTRL)
		mod |= B_LEFT_CONTROL_KEY | B_CONTROL_KEY;
	if (event.key.keysym.mod & KMOD_RCTRL)
		mod |= B_RIGHT_CONTROL_KEY | B_CONTROL_KEY;
	if (event.key.keysym.mod & KMOD_LSHIFT)
		mod |= B_LEFT_SHIFT_KEY | B_SHIFT_KEY;
	if (event.key.keysym.mod & KMOD_RSHIFT)
		mod |= B_RIGHT_SHIFT_KEY | B_SHIFT_KEY;
	if (event.key.keysym.mod & KMOD_LALT)
		mod |= B_LEFT_OPTION_KEY | B_OPTION_KEY;
	if (event.key.keysym.mod & KMOD_RALT)
		mod |= B_RIGHT_OPTION_KEY | B_OPTION_KEY;
	if (event.key.keysym.mod & KMOD_CAPS)
		mod |= B_CAPS_LOCK;

	return mod;
}


void SendKeyEvent(port_id port, uint32 what, char key, uint32 modifiers, uint32 repeatCount)
{
	char string[2];
	string[0] = key;
	string[1] = 0;
	BMessage msg(what);
	msg.AddInt64("when", real_time_clock());
	msg.AddInt32("key", key);
	msg.AddInt32("modifiers", modifiers);
	msg.AddInt8("byte", (int8)string[0]);
	msg.AddData("bytes", B_STRING_TYPE, string, 2);

	if (what == B_KEY_DOWN)
		msg.AddInt32("be:key_repeat", repeatCount);

	size_t length = msg.FlattenedSize();
	char stream[length];

	if (msg.Flatten(stream, length) == B_OK)
		write_port(port, 0, stream, length);
}


void SendModifiersEvent(port_id port, uint32 modifiers, uint32 oldModifiers)
{
	BMessage message(B_MODIFIERS_CHANGED);

	message.AddInt64("when", real_time_clock());
	message.AddInt32("be:old_modifiers", oldModifiers);
	message.AddInt32("modifiers", modifiers);

	size_t length = message.FlattenedSize();
	char stream[length];

	if (message.Flatten(stream, length) == B_OK)
		write_port(port, 0, stream, length);
}


/*!
	\brief Translate X11 events into appserver events, as if they came
			right from the actual Input Server
*/
void SDLEventTranslator(void *arg)
{
	SDLInterface *driver= (SDLInterface*)arg;
	SDL_Event event;
	int quit = 0;
	float x, y;
	uint32 buttons = 0;
	uint32 mod = 0;
	port_id fInputPort = create_port(200, SERVER_INPUT_PORT);
	int repeatCount = 1;
	int lastKey = 0;
	uint32 oldModifiers = 0;

	if (fInputPort < 0)
		printf("Could not find SERVER_INPUT_PORT");

	SDL_StartTextInput();

	/* Loop until an SDL_QUIT event is found */
	while(!quit)
	{
		/* Poll for events */
		while(SDL_PollEvent(&event))
		{
			switch(event.type)
			{
				case SDL_MOUSEMOTION:
				{
					x=(float)event.motion.x;
					y=(float)event.motion.y;

					//STRACE("Driver->MouseMoved\n");
					//fprintf(stderr, "Initialize thread id = %d\n", pthread_self());

					BMessage mm(B_MOUSE_MOVED);
					mm.AddInt64("when", real_time_clock());
					mm.AddInt32("buttons", buttons);
					mm.AddPoint("where", BPoint(x,y));
					
					size_t length = mm.FlattenedSize();
					char stream[length];

					if (mm.Flatten(stream, length) == B_OK)
						write_port(fInputPort, 0, stream, length);
					break;
				}

				case SDL_MOUSEBUTTONDOWN:
				case SDL_MOUSEBUTTONUP:{
					STRACE(event.type == SDL_MOUSEBUTTONDOWN ? "MouseDown\n" : "MouseUp\n");
					uint32 buttons = event.type == SDL_MOUSEBUTTONDOWN ? event.button.button : 0;
					uint32 clicks = 1;		// can't get the # of clicks without a *lot* of extra work :(
					mod = 0;
					x=(float)event.motion.x;
					y=(float)event.motion.y;

					BMessage mc(event.type == SDL_MOUSEBUTTONDOWN ? B_MOUSE_DOWN : B_MOUSE_UP);
					mc.AddInt64("when", real_time_clock());
					mc.AddInt32("buttons", buttons);
					mc.AddInt32("modifiers", mod);
					mc.AddPoint("where", BPoint(x,y));
					mc.AddInt32("clicks", clicks);
					
					size_t length = mc.FlattenedSize();
					char stream[length];

					if (mc.Flatten(stream, length) == B_OK)
						write_port(fInputPort, 0, stream, length);

					break;
				}

				/* Keyboard event */
				case SDL_TEXTINPUT:
				{
					mod = GetModifiers(event);

					if (mod != oldModifiers) {
						SendModifiersEvent(fInputPort, mod, oldModifiers);
					}

					if (((mod & B_SHIFT_KEY) != 0) || ((mod & B_CAPS_LOCK) != 0))
						SendKeyEvent(fInputPort, B_KEY_DOWN, event.text.text[0], mod, lastKey);
					break;
				}

				case SDL_KEYDOWN:
				case SDL_KEYUP:
				{
					mod = GetModifiers(event);

					bool isKeyDown = (event.type == SDL_KEYDOWN);

					if (isKeyDown && event.key.keysym.sym == lastKey)
						repeatCount++;
					else
						repeatCount = 1;

					if (isKeyDown && (mod != oldModifiers))
						SendModifiersEvent(fInputPort, mod, oldModifiers);

					if (((mod & B_SHIFT_KEY) == 0) && ((mod & B_CAPS_LOCK) == 0))
						SendKeyEvent(fInputPort, event.type == SDL_KEYDOWN ? B_KEY_DOWN : B_KEY_UP, event.key.keysym.sym, mod, repeatCount);

					lastKey = event.key.keysym.sym;
					oldModifiers = mod;

					/* the Escape quits Cosmoe, for now... */
					if(event.key.keysym.sym == SDLK_ESCAPE) {
						driver->Invalidate(BRect(0,0,WIDTH - 1,HEIGHT - 1));
						STRACE("Invalidate\n");
						//quit = 1;
					}
					break;
				}

				/* SDL_QUIT event (window close) */
				case SDL_QUIT:
					STRACE("SDL_QUIT\n");
					quit = 1;
					break;

				default:
					break;
			}
		}
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
SDLInterface::Initialize(void)
{
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		printf("Couldn't initialize SDL: %s\n", SDL_GetError());
		return false;
	}

	SDL_SetHintWithPriority(SDL_HINT_FRAMEBUFFER_ACCELERATION, "software", SDL_HINT_OVERRIDE);

	mWindow = SDL_CreateWindow("Cosmoe",
					SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
					WIDTH, HEIGHT,
					SDL_WINDOW_SHOWN);
	
	if (mWindow == NULL)
	{
		printf("Couldn't set 800x600x32 video mode: %s\n", SDL_GetError());
		return B_ERROR;
	}

	mScreen = SDL_GetWindowSurface(mWindow);

	fFrontBuffer->fBitmap->_SetBuffer(mScreen->pixels);

	// Create a new thread for mouse and key events
	pthread_t input_thread;
	pthread_create (&input_thread,
					NULL,
					(void *(*) (void *))&SDLEventTranslator,
					(void *) this);

	SDL_ShowCursor(0);

	status_t result = BitmapHWInterface::Initialize();

	fprintf(stderr, "SDLInterface::Initialize thread id = %lu\n", pthread_self());

	return result;
}


/*!
	\brief Shuts down the driver's video subsystem

	Any work done by Initialize() should be undone here. Note that Shutdown() is
	called even if Initialize() was unsuccessful.
*/
status_t
SDLInterface::Shutdown()
{
    SDL_DestroyWindow(mWindow);

	STRACE( "Driver::Shutdown()\n" );
	SDL_Quit();

	return BitmapHWInterface::Shutdown();
}


void
SDLInterface::CopyRegion(const clipping_rect* sortedRectList,
		uint32 count,
		int32 xOffset, int32 yOffset)
{
	SDL_Rect source, destination;
	bool success;
	
	//STRACE("SDLInterface::CopyRegion()\n");

	for (uint32 i = 0; i < count; i++)
	{
		ClippingRectToSDLRect(sortedRectList[i], source);
		ClippingRectToSDLRect(sortedRectList[i], destination);
		destination.x += xOffset;
		destination.y += yOffset;
		success = (SDL_BlitSurface(mScreen, &source, mScreen, &destination) == 0);
		
		if (success)
			_InvalidateSDL(destination);
	}
}


void
SDLInterface::FillRegion(/*const*/ BRegion& region,
	   const rgb_color& col, bool autoSync)
{
	Uint32	aColor = SDL_MapRGB(mScreen->format, col.red, col.green, col.blue);
	SDL_Rect source;
	bool success;
	int32 count = region.CountRects();

	//STRACE("SDLInterface::FillRegion()\n");

	for (int32 i = 0; i < count; i++)
	{
		ClippingRectToSDLRect(region.RectAtInt(i), source);
		success = (SDL_FillRect(mScreen, &source, aColor) == 0);
		
		if (success)
			_InvalidateSDL(source);
	}
}


status_t
SDLInterface::Invalidate(const BRect& frame)
{
	SDL_Rect aRect;
	RectToSDLRect(frame, aRect);
	_InvalidateSDL(aRect);
	return B_OK;
}


/*!
	\brief Refresh the framebuffer with the contents of the ServerBitmap
	\param r      The BRect rectangle to refresh
*/
void SDLInterface::_CopyBackToFront(/*const*/ BRegion& region)
{
	//fprintf(stderr, "Driver::_CopyBackToFront(%.0f, %.0f, %.0f, %.0f)\n", r.left, r.top, r.right, r.bottom);
	region.PrintToStream();

	fprintf(stderr, "Driver::_CopyBackToFront thread id = %lu\n", pthread_self());
	
	// Limit damage rect to screen coordinates to avoid writing out of bound
	//BRect damage(r & BRect(0, 0, FrontBuffer()->Bounds().Width(), FrontBuffer()->Bounds().Height()));
	SDL_Rect aRect;

	int32 count = region.CountRects();
	for (int32 i = 0; i < count; i++) {
		RectToSDLRect(region.RectAt(i), aRect);
		_InvalidateSDL(aRect);
	}
	//return B_OK;
}


/*!
	\brief Refresh the framebuffer with the contents of the ServerBitmap
	\param r      The SDLRect rectangle to refresh
*/
void SDLInterface::_InvalidateSDL(const SDL_Rect &r)
{
	acquire_sem(drawsem);
	//SDL_UpdateRect(mScreen, r.x, r.y, r.w, r.h);
	//SDL_UpdateWindowSurface(mWindow);
	SDL_UpdateWindowSurfaceRects(mWindow, &r, 1);
	release_sem(drawsem);
}


/*!
	\brief Convert a BRect to an SDL_Rect
	\param r        The source BRect
	\param outRect  The SDL_Rect to make equal to the BRect
*/
static void RectToSDLRect(const BRect& r, SDL_Rect& outRect)
{
	outRect.w = r.IntegerWidth() + 1;
	outRect.h = r.IntegerHeight() + 1;
	outRect.x = (int)r.left;
	outRect.y = (int)r.top;
}

static void ClippingRectToSDLRect(const clipping_rect r, SDL_Rect& outRect)
{
	outRect.w = (r.right - r.left) + 1;
	outRect.h = (r.bottom - r.top) + 1;
	outRect.x = r.left;
	outRect.y = r.top;
}


status_t
SDLInterface::SetMode(const display_mode& mode)
{
	printf("SDL Interface: SetMode\n");
	return B_OK;
}


void SDLInterface::GetMode(display_mode* mode)
{
	mode->virtual_height = HEIGHT;
	mode->virtual_width = WIDTH;
	mode->space = B_RGB32;
	mode->h_display_start = 0;
	mode->v_display_start = 0;
	mode->timing.h_display = 60.0f;
	mode->timing.v_display = 60.0f;
	mode->flags = 0;
}


status_t
SDLInterface::GetModeList(display_mode** _modes, uint32 *_count)
{
	AutoReadLocker _(this);

	if (_count == NULL || _modes == NULL)
		return B_BAD_VALUE;

	status_t status = B_OK;

	if (fModeList == NULL)
		status = _UpdateModeList();

	if (status >= B_OK) {
		*_modes = new(nothrow) display_mode[fModeCount];
		if (*_modes) {
			*_count = fModeCount;
			memcpy(*_modes, fModeList, sizeof(display_mode) * fModeCount);
		} else {
			*_count = 0;
			status = B_NO_MEMORY;
		}
	}
	return status;
}


status_t
SDLInterface::GetPreferredMode(display_mode* mode)
{
	status_t status = B_OK;

	if (mode == NULL)
		return B_BAD_VALUE;

	if (fModeList == NULL)
		status = _UpdateModeList();

	memcpy(mode, &fModeList[0], sizeof(display_mode));

	return status;
}


status_t
SDLInterface::_UpdateModeList()
{
	fModeCount = 2;

	delete[] fModeList;
	fModeList = new(nothrow) display_mode[fModeCount];
	if (!fModeList)
		return B_NO_MEMORY;

	display_mode* mode = &fModeList[0];
	mode->virtual_height = 600;
	mode->virtual_width = 800;
	mode->space = B_RGB32;
	mode->h_display_start = 0;
	mode->v_display_start = 0;
	mode->timing.h_display = 60.0f;
	mode->timing.v_display = 60.0f;
	mode->flags = 0;

	mode = &fModeList[1];
	mode->virtual_height = 768;
	mode->virtual_width = 1024;
	mode->space = B_RGB32;
	mode->h_display_start = 0;
	mode->v_display_start = 0;
	mode->timing.h_display = 60.0f;
	mode->timing.v_display = 60.0f;
	mode->flags = 0;

	return B_OK;
}