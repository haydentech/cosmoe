/*
 * Copyright 2001-2015 Haiku, inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Axel Dörfler, axeld@pinc-software.de
 *		Jerome Duval
 *		Erik Jaesler, erik@cgsoftware.com
 */


#include <Application.h>

#include <new>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

// #include <Alert.h>
// #include <AppFileInfo.h>
// #include <Cursor.h>
// #include <Debug.h>
// #include <Entry.h>
// #include <File.h>
#include <Locker.h>
#include <MessageRunner.h>
#include <ObjectList.h>
// #include <Path.h>
// #include <PropertyInfo.h>
// #include <RegistrarDefs.h>
// #include <Resources.h>
// #include <Roster.h>
#include <Window.h>

// #include <AppMisc.h>
// #include <AppServerLink.h>
#include <AutoLocker.h>
// #include <BitmapPrivate.h>
// #include <DraggerPrivate.h>
// #include <LaunchDaemonDefs.h>
// #include <LaunchRoster.h>
#include <LooperList.h>
// #include <MenuWindow.h>
// #include <PicturePrivate.h>
// #include <RosterPrivate.h>


using namespace BPrivate;


static const char* kDefaultLooperName = "AppLooperPort";

BApplication* be_app = NULL;
BObjectList<BLooper> sOnQuitLooperList;

#define RUN_WITHOUT_REGISTRAR 1


enum {
	kWindowByIndex,
	kWindowByName,
	kLooperByIndex,
	kLooperByID,
	kLooperByName,
	kApplication
};

// argc/argv
extern const int __libc_argc;
extern const char* const *__libc_argv;


// debugging
#define DBG(x) x
//#define DBG(x)
#define OUT	printf


//	#pragma mark - static helper functions


/*!
	\brief Checks whether the supplied string is a valid application signature.

	An error message is printed, if the string is no valid app signature.

	\param signature The string to be checked.

	\return A status code.
	\retval B_OK \a signature is a valid app signature.
	\retval B_BAD_VALUE \a signature is \c NULL or no valid app signature.
*/
static status_t
check_app_signature(const char* signature)
{
	bool isValid = false;
	return (isValid ? B_OK : B_BAD_VALUE);
}
//	#pragma mark - BApplication


BApplication::BApplication(const char* signature)
	:
	BLooper(kDefaultLooperName)
{
    printf("BApplication::BApplication\n");
	_InitData(signature, true, NULL);
}


BApplication::BApplication(const char* signature, status_t* _error)
	:
	BLooper(kDefaultLooperName)
{
	_InitData(signature, true, _error);
}



BApplication::~BApplication()
{
	// uninitialize be_app, the be_app_messenger is invalidated automatically
	be_app = NULL;
}


void
BApplication::_InitData(const char* signature, bool initGUI, status_t* _error)
{
	DBG(OUT("BApplication::InitData(`%s', %p)\n", signature, _error));
	// check whether there exists already an application
	if (be_app != NULL)
		debugger("2 BApplication objects were created. Only one is allowed.");

	fInitialWorkspace = 0;
	fReadyToRunCalled = false;

	// initially, there is no pulse
	//fPulseRunner = NULL;
	fPulseRate = 0;

	// check signature
	fInitError = check_app_signature(signature);
	fAppName = signature;

	// init be_app and be_app_messenger
	be_app = this;
    //be_app_messenger = BMessenger(NULL, this);
}


status_t
BApplication::InitCheck() const
{
	return fInitError;
}


thread_id
BApplication::Run()
{
	if (fInitError != B_OK)
		return fInitError;

	Loop();

	//delete fPulseRunner;
	return fThread;
}


void
BApplication::Quit()
{
}


bool
BApplication::QuitRequested()
{
	return _QuitAllWindows(false);
}


void
BApplication::Pulse()
{
	// supposed to be implemented by subclasses
}


void
BApplication::ReadyToRun()
{
	// supposed to be implemented by subclasses
}


void
BApplication::ArgvReceived(int32 argc, char** argv)
{
	// supposed to be implemented by subclasses
}


void
BApplication::AppActivated(bool active)
{
	// supposed to be implemented by subclasses
}


void
BApplication::AboutRequested()
{
	// supposed to be implemented by subclasses
}


void
BApplication::ShowCursor()
{
}

void
BApplication::HideCursor()
{
}

void
BApplication::ObscureCursor()
{
}

bool
BApplication::IsCursorHidden() const
{
	return false;
}


void
BApplication::SetCursor(const void* cursorData)
{
}


int32
BApplication::CountWindows() const
{
	return _CountWindows(false);
		// we're ignoring menu windows
}


BWindow*
BApplication::WindowAt(int32 index) const
{
	return _WindowAt(index, false);
		// we're ignoring menu windows
}


int32
BApplication::CountLoopers() const
{
	AutoLocker<BLooperList> ListLock(gLooperList);
	if (ListLock.IsLocked())
		return gLooperList.CountLoopers();

	// Some bad, non-specific thing has happened
	return B_ERROR;
}


BLooper*
BApplication::LooperAt(int32 index) const
{
	BLooper* looper = NULL;
	AutoLocker<BLooperList> listLock(gLooperList);
	if (listLock.IsLocked())
		looper = gLooperList.LooperAt(index);

	return looper;
}


status_t
BApplication::RegisterLooper(BLooper* looper)
{
	BWindow* window = dynamic_cast<BWindow*>(looper);
	if (window != NULL)
		return B_BAD_VALUE;

	if (sOnQuitLooperList.HasItem(looper))
		return B_ERROR;

	if (sOnQuitLooperList.AddItem(looper) != true)
		return B_ERROR;

	return B_OK;
}


status_t
BApplication::UnregisterLooper(BLooper* looper)
{
	BWindow* window = dynamic_cast<BWindow*>(looper);
	if (window != NULL)
		return B_BAD_VALUE;

	if (!sOnQuitLooperList.HasItem(looper))
		return B_ERROR;

	if (sOnQuitLooperList.RemoveItem(looper) != true)
		return B_ERROR;

	return B_OK;
}


bool
BApplication::IsLaunching() const
{
	return !fReadyToRunCalled;
}


const char*
BApplication::Signature() const
{
	return fAppName;
}


status_t
BApplication::GetAppInfo(app_info* info) const
{
	return B_OK;
}


void
BApplication::SetPulseRate(bigtime_t rate)
{
	if (rate < 0)
		rate = 0;

	// BeBook states that we have only 100,000 microseconds granularity
	rate -= rate % 100000;

	fPulseRate = rate;
}

void BApplication::_ReservedApplication1() {}
void BApplication::_ReservedApplication2() {}
void BApplication::_ReservedApplication3() {}
void BApplication::_ReservedApplication4() {}
void BApplication::_ReservedApplication5() {}
void BApplication::_ReservedApplication6() {}
void BApplication::_ReservedApplication7() {}
void BApplication::_ReservedApplication8() {}

void
BApplication::BeginRectTracking(BRect rect, bool trackWhole)
{
}

void
BApplication::EndRectTracking()
{
}

status_t
BApplication::_SetupServerAllocator()
{
	return B_OK;
}


status_t
BApplication::_InitGUIContext()
{
	// An app_server connection is necessary for a lot of stuff, so get that first.
	status_t error = _ConnectToServer();
	if (error != B_OK)
		return error;

	// Initialize the IK after we have set be_app because of a construction
	// of a AppServerLink (which depends on be_app) nested inside the call
	// to get_menu_info.
	error = _init_interface_kit_();
	if (error != B_OK)
		return error;

	return B_OK;
}


status_t
BApplication::_ConnectToServer()
{
	return B_OK;
}


void
BApplication::_ReconnectToServer()
{
}

bool
BApplication::_WindowQuitLoop(bool quitFilePanels, bool force)
{
	int32 index = 0;
	while (true) {
		 BWindow* window = WindowAt(index);
		 if (window == NULL)
		 	break;

		// NOTE: the window pointer might be stale, in case the looper
		// was already quit by quitting an earlier looper... but fortunately,
		// we can still call Lock() on the invalid pointer, and it
		// will return false...
		if (!window->Lock())
			continue;

		// don't quit file panels if we haven't been asked for it
		if (!quitFilePanels && window->IsFilePanel()) {
			window->Unlock();
			index++;
			continue;
		}

		if (!force && !window->QuitRequested()
			&& !(quitFilePanels && window->IsFilePanel())) {
			// the window does not want to quit, so we don't either
			window->Unlock();
			return false;
		}

		// Re-lock, just to make sure that the user hasn't done nasty
		// things in QuitRequested(). Quit() unlocks fully, thus
		// double-locking is harmless.
		if (window->Lock())
			window->Quit();

		index = 0;
			// we need to continue at the start of the list again - it
			// might have changed
	}

	return true;
}


bool
BApplication::_QuitAllWindows(bool force)
{
	AssertLocked();

	// We need to unlock here because BWindow::QuitRequested() must be
	// allowed to lock the application - which would cause a deadlock
	Unlock();

	bool quit = _WindowQuitLoop(false, force);
	if (quit)
		quit = _WindowQuitLoop(true, force);

	Lock();

	return quit;
}

uint32
BApplication::InitialWorkspace()
{
	return 0;
}


int32
BApplication::_CountWindows(bool includeMenus) const
{
	uint32 count = 0;
	for (int32 i = 0; i < gLooperList.CountLoopers(); i++) {
		BWindow* window = dynamic_cast<BWindow*>(gLooperList.LooperAt(i));
		if (window != NULL)
		// && !window->fOffscreen && (includeMenus
		//|| dynamic_cast<BMenuWindow*>(window) == NULL))
		{
			count++;
		}
	}

	return count;
}


BWindow*
BApplication::_WindowAt(uint32 index, bool includeMenus) const
{
	AutoLocker<BLooperList> listLock(gLooperList);
	if (!listLock.IsLocked())
		return NULL;

	uint32 count = gLooperList.CountLoopers();
	for (uint32 i = 0; i < count && index < count; i++) {
		BWindow* window = dynamic_cast<BWindow*>(gLooperList.LooperAt(i));
		if (window == NULL)
		// || (window != NULL && window->fOffscreen)
		//	|| (!includeMenus && dynamic_cast<BMenuWindow*>(window) != NULL))
		{
			index++;
			continue;
		}

		if (i == index)
			return window;
	}

	return NULL;
}
