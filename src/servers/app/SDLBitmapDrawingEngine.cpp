#include "SDLBitmapDrawingEngine.h"
#include "BitmapHWInterface.h"
#include "ServerBitmap.h"
#include <new>

#include <stdio.h>

#include <PortLink.h>
#include <ServerProtocol.h>


#define DEBUG_SDL_DRIVER

#ifdef DEBUG_SDL_DRIVER
#include <stdio.h>
	#define STRACE(a) printf(a)
#else
	#define STRACE(x) ;
#endif


/*!
	\brief Translate SDL events into appserver events, as if they came
			right from the actual Input Server
*/
void SDLBitmapEventTranslator(void *arg)
{
	SDLBitmapDrawingEngine *driver= (SDLBitmapDrawingEngine*)arg;
	SDL_Event event;
	int quit = 0;
	float x, y;
	uint32 buttons = 0;
	port_id fInputPort = find_port(SERVER_INPUT_PORT);

	if (fInputPort < 0)
		printf("Could not find SIP");

	/* Loop until an SDL_QUIT event is found */
	while( !quit )
	{
		/* Poll for events */
		while( SDL_PollEvent( &event ) )
		{
			switch(event.type)
			{
				case SDL_MOUSEMOTION:
				{
					x=(float)event.motion.x;
					y=(float)event.motion.y;

					//STRACE("SDLDriver::MouseMoved\n");

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
					STRACE("MouseDown/Up\n");
					uint32 buttons = event.button.button;
					uint32 clicks = 1;		// can't get the # of clicks without a *lot* of extra work :(
					uint32 mod = 0;

					BMessage mc(event.type == SDL_MOUSEBUTTONDOWN ? B_MOUSE_DOWN : B_MOUSE_UP);
					mc.AddInt64("when", real_time_clock());
					mc.AddInt32("buttons", buttons);
					mc.AddPoint("where", BPoint(x,y));
					mc.AddInt32("clicks", clicks);
					
					size_t length = mc.FlattenedSize();
					char stream[length];

					if (mc.Flatten(stream, length) == B_OK)
						write_port(fInputPort, 0, stream, length);

					break;
				}

				/* Keyboard event */
				case SDL_KEYDOWN:
				{
					STRACE("KeyDown\n");

					int32 scancode, repeatcount,modifiers;
					modifiers = event.key.keysym.mod;
					int64 time = (int64)real_time_clock();
					
					repeatcount = 1;
					scancode = event.key.keysym.sym;
					// driver->serverlink->StartMessage(B_KEY_DOWN);
					// driver->serverlink->Attach<int64>(time);
					// driver->serverlink->Attach<int32>(scancode);
					// driver->serverlink->Attach<int32>(repeatcount);
					// driver->serverlink->Attach<int32>(modifiers);
					//driver->serverlink->Attach(utf8data,sizeof(int8)*3);
					// //driver->serverlink->Attach(keyarray,sizeof(int8)*16);
					// driver->serverlink->Flush();
					
					/* the Escape quits Cosmoe, for now... */
					if(event.key.keysym.sym == SDLK_ESCAPE)
						quit = 1;
					break;
				}
				
				case SDL_KEYUP:
				{
					STRACE("KeyUp\n");

					int32 scancode, repeatcount,modifiers;
					modifiers = event.key.keysym.mod;
					repeatcount = 1;
					scancode = event.key.keysym.sym;
					int64 time = (int64)real_time_clock();
					
					// driver->serverlink->StartMessage(B_KEY_DOWN);
					// driver->serverlink->Attach<int64>(time);
					// driver->serverlink->Attach<int32>(scancode);
					// driver->serverlink->Attach<int32>(repeatcount);
					// driver->serverlink->Attach<int32>(modifiers);
					//driver->serverlink->Attach(utf8data,sizeof(int8)*3);
					//driver->serverlink->Attach(keyarray,sizeof(int8)*16);
					// driver->serverlink->Flush();

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


SDLBitmapDrawingEngine::SDLBitmapDrawingEngine()
	:	BitmapDrawingEngine()
{
	// This link for sending mouse messages to the AppServer.
	// This is only to take the place of the Input Server for testing purposes.
	serverlink = new BPrivate::PortLink(find_port(SERVER_INPUT_PORT));

	drawsem = create_sem(1, "SDL draw semaphore");
}


SDLBitmapDrawingEngine::~SDLBitmapDrawingEngine()
{
}

bool SDLBitmapDrawingEngine::Initialize(void)
{
	status_t err = SetSize(800, 600);
	if (err != B_OK) {
		printf("SetSize failed with error %ld\n", err);
		return false;
	}

	return true;
}

status_t
SDLBitmapDrawingEngine::SetSize(int32 newWidth, int32 newHeight)
{
	status_t status;

	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		printf("Couldn't initialize SDL: %s\n", SDL_GetError());
		return B_ERROR;
	}

	mWindow = SDL_CreateWindow("Cosmoe",
					SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
					800, 600,
					SDL_SWSURFACE);
	
	if (mWindow == NULL)
	{
		printf("Couldn't set 800x600x32 video mode: %s\n", SDL_GetError());
		return B_ERROR;
	}

	mScreen = SDL_GetWindowSurface(mWindow);

	// Create a new thread for mouse and key events
	pthread_t input_thread;
	pthread_create (&input_thread,
					NULL,
					(void *(*) (void *))&SDLBitmapEventTranslator,
					(void *) this);

	// The bitmap should point to the SDL window's pixels
	fBitmap = (UtilityBitmap*) new(std::nothrow) SDLBitmap((uint8*)mScreen->pixels, newWidth, newHeight, B_RGB32);
	if (fBitmap == NULL)
		return B_NO_MEMORY;

	SDL_ShowCursor(1);

	printf("in SetSize %d x %d\n", newWidth, newHeight);
	if (fBitmap != NULL && newWidth > 0 && newHeight > 0
		&& fBitmap->Bounds().IntegerWidth() >= newWidth
		&& fBitmap->Bounds().IntegerHeight() >= newHeight) {
		return B_OK;
	}

	printf("About to clear HWInterface\n");
	
	SetHWInterface(NULL);
	if (fHWInterface) {
		fHWInterface->LockExclusiveAccess();
		fHWInterface->Shutdown();
		fHWInterface->UnlockExclusiveAccess();
		delete fHWInterface;
		fHWInterface = NULL;
	}

	printf("About to create new HWInterface\n");

	fHWInterface = new(std::nothrow) BitmapHWInterface(fBitmap);
	if (fHWInterface == NULL)
		return B_NO_MEMORY;

	printf("About to init new HWInterface\n");

	status_t result = fHWInterface->Initialize();
	if (result != B_OK)
		return result;

	printf("About to set clipping\n");

	// we have to set a valid clipping first
	fClipping.Set(fBitmap->Bounds());
	ConstrainClippingRegion(&fClipping);
	SetHWInterface(fHWInterface);

	printf("Done\n");
	return B_OK;
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


/*!
	\brief Refresh the framebuffer with the contents of the ServerBitmap
	\param r      The BRect rectangle to refresh
*/
void SDLBitmapDrawingEngine::Invalidate(const BRect &r)
{
	SDL_Rect aRect;
	BRect damage(r /* fBitmap->Bounds() */);
	RectToSDLRect(damage, aRect);

	_InvalidateSDL(aRect);
}


/*!
	\brief Refresh the framebuffer with the contents of the ServerBitmap
	\param r      The SDLRect rectangle to refresh
*/
void SDLBitmapDrawingEngine::_InvalidateSDL(const SDL_Rect &r)
{
	acquire_sem(drawsem);
	SDL_UpdateWindowSurfaceRects(mWindow, &r, 1);
	release_sem(drawsem);
}


//	#pragma mark -


SDLBitmap::SDLBitmap(uint8* bits, uint32 width,
		uint32 height, color_space format)
	:
	ServerBitmap(BRect(0, 0, width - 1, height - 1), format, 0)
{
	fBuffer = bits;
}


SDLBitmap::~SDLBitmap()
{
}
