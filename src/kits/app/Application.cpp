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

// #include <pthread.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <strings.h>
// #include <unistd.h>

// #include <Alert.h>
// #include <AppFileInfo.h>
// #include <Cursor.h>
// #include <Debug.h>
// #include <Entry.h>
// #include <File.h>
// #include <Locker.h>
// #include <MessageRunner.h>
// #include <ObjectList.h>
// #include <Path.h>
// #include <PropertyInfo.h>
// #include <RegistrarDefs.h>
// #include <Resources.h>
// #include <Roster.h>
// #include <Window.h>

// #include <AppMisc.h>
// #include <AppServerLink.h>
// #include <AutoLocker.h>
// #include <BitmapPrivate.h>
// #include <DraggerPrivate.h>
// #include <LaunchDaemonDefs.h>
// #include <LaunchRoster.h>
// #include <LooperList.h>
// #include <MenuWindow.h>
// #include <PicturePrivate.h>
// #include <RosterPrivate.h>


static const char* kDefaultLooperName = "AppLooperPort";

BApplication* be_app = NULL;

#define RUN_WITHOUT_REGISTRAR 1




// argc/argv
extern const int __libc_argc;
extern const char* const *__libc_argv;


// debugging
#define DBG(x) x
//#define DBG(x)
#define OUT	printf

BApplication::BApplication(char const* signature)
{
    printf("BApplication::BApplication\n");
    _InitData(signature, true, NULL);
}


BApplication::~BApplication()
{

}

void
BApplication::_InitData(const char* signature, bool initGUI, status_t* _error)
{
    DBG(OUT("BApplication::InitData(`%s', %p)\n", signature, _error));
	// check whether there exists already an application
	if (be_app != NULL)
		debugger("2 BApplication objects were created. Only one is allowed.");

	fReadyToRunCalled = false;

	// initially, there is no pulse
	//fPulseRunner = NULL;
	fPulseRate = 0;

	// check signature
	//fInitError = check_app_signature(signature);
	fAppName = signature;

    // init be_app and be_app_messenger
    be_app = this;
    //be_app_messenger = BMessenger(NULL, this);
}


thread_id
BApplication::Run()
{
    return 0;
}

void
BApplication::Quit()
{
}

bool
BApplication::QuitRequested()
{
	return false;
}

void
BApplication::Pulse()
{
}

void
BApplication::ReadyToRun()
{
}


void
BApplication::ArgvReceived(int32 argc, char** argv)
{
}

void
BApplication::AppActivated(bool active)
{
}


void
BApplication::AboutRequested()
{
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
	return 0;
}

BWindow*
BApplication::WindowAt(int32 index) const
{
	return NULL;
}


bool
BApplication::IsLaunching() const
{
	return false;
}

const char*
BApplication::Signature() const
{
	return NULL;
}

status_t
BApplication::GetAppInfo(app_info* info) const
{
	return B_OK;
}


void
BApplication::SetPulseRate(bigtime_t rate)
{
    fPulseRate = rate;
}

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
	return false;
}

bool
BApplication::_QuitAllWindows(bool force)
{
	return false;
}

uint32
BApplication::InitialWorkspace()
{
	return 0;
}

int32
BApplication::_CountWindows(bool includeMenus) const
{
	return 0;
}

BWindow*
BApplication::_WindowAt(uint32 index, bool includeMenus) const
{
	return NULL;
}
