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

// On Windows, DllMain is not called, so we need to call initialization explicitly
#ifdef _WIN32
extern "C" void __libbe_initialize_before();
#endif

// #include <Alert.h>
#include <AppFileInfo.h>
#include <Cursor.h>
#include <Debug.h>
#include <Entry.h>
#include <File.h>
#include <Locker.h>
#include <MessageRunner.h>
#include <ObjectList.h>
#include <Path.h>
#include <PropertyInfo.h>
#include <Resources.h>
#include <Roster.h>
#include <Window.h>

#include <AppMisc.h>
#include <AppServerLink.h>
#include <AutoLocker.h>
#include <BitmapPrivate.h>
#include <DraggerPrivate.h>
#include <LinuxRemoteAppMessenger.h>
#include <LooperList.h>
#include <MenuWindow.h>
#include <CosmoeBackendAPI.h>
// #include <PicturePrivate.h>

#ifdef _WIN32
// Include windows.h after all other headers to avoid conflicts
// Then undef the macros that conflict with Be API
#include <windows.h>
#undef PostMessage
#undef SendMessage
#undef DispatchMessage
#undef OUT

// On Windows, initialization functions are not called automatically
// They must be called explicitly from BApplication constructor
extern void initialize_before();
extern void terminate_after();
#endif


using namespace BPrivate;

// Forward declaration from Looper.cpp - get looper's port
extern port_id _get_looper_port_(const BLooper* looper);

static const char* kDefaultLooperName = "AppLooperPort";

BApplication* be_app = NULL;
BMessenger be_app_messenger;

#ifdef _WIN32
// On Windows, PTHREAD_ONCE_INIT can cause DLL load failures
// Use lazy initialization instead
static pthread_once_t* get_app_resources_once() {
	static pthread_once_t once = PTHREAD_ONCE_INIT;
	return &once;
}
#define sAppResourcesInitOnce (*get_app_resources_once())
#else
pthread_once_t sAppResourcesInitOnce = PTHREAD_ONCE_INIT;
#endif

BResources* BApplication::sAppResources = NULL;
cosmoe_display_t BApplication::fDisplay = NULL;
pthread_t BApplication::fDisplayThread;
port_id BApplication::fBackendPort = -1;
BObjectList<BLooper> sOnQuitLooperList;


enum {
	kWindowByIndex,
	kWindowByName,
	kLooperByIndex,
	kLooperByID,
	kLooperByName,
	kApplication
};


static property_info sPropertyInfo[] = {
	{
		"Window",
		{},
		{B_INDEX_SPECIFIER, B_REVERSE_INDEX_SPECIFIER},
		NULL, kWindowByIndex,
		{},
		{},
		{}
	},
	{
		"Window",
		{},
		{B_NAME_SPECIFIER},
		NULL, kWindowByName,
		{},
		{},
		{}
	},
	{
		"Looper",
		{},
		{B_INDEX_SPECIFIER, B_REVERSE_INDEX_SPECIFIER},
		NULL, kLooperByIndex,
		{},
		{},
		{}
	},
	{
		"Looper",
		{},
		{B_ID_SPECIFIER},
		NULL, kLooperByID,
		{},
		{},
		{}
	},
	{
		"Looper",
		{},
		{B_NAME_SPECIFIER},
		NULL, kLooperByName,
		{},
		{},
		{}
	},
	{
		"Name",
		{B_GET_PROPERTY},
		{B_DIRECT_SPECIFIER},
		NULL, kApplication,
		{B_STRING_TYPE},
		{},
		{}
	},
	{
		"Window",
		{B_COUNT_PROPERTIES},
		{B_DIRECT_SPECIFIER},
		NULL, kApplication,
		{B_INT32_TYPE},
		{},
		{}
	},
	{
		"Loopers",
		{B_GET_PROPERTY},
		{B_DIRECT_SPECIFIER},
		NULL, kApplication,
		{B_MESSENGER_TYPE},
		{},
		{}
	},
	{
		"Windows",
		{B_GET_PROPERTY},
		{B_DIRECT_SPECIFIER},
		NULL, kApplication,
		{B_MESSENGER_TYPE},
		{},
		{}
	},
	{
		"Looper",
		{B_COUNT_PROPERTIES},
		{B_DIRECT_SPECIFIER},
		NULL, kApplication,
		{B_INT32_TYPE},
		{},
		{}
	},

	{ 0 }
};


static status_t
send_scripting_message(BMessage* message, BMessenger messenger)
{
	if (!message->IsSourceWaiting())
		return messenger.SendMessage(message);

	BMessage forwarded(*message);
	BMessage reply;
	status_t err = messenger.SendMessage(&forwarded, &reply);
	if (err == B_OK)
		err = message->SendReply(&reply);
	return err;
}


// argc/argv
#ifdef __APPLE__
#include <crt_externs.h>
#define __libc_argc (*_NSGetArgc())
#define __libc_argv (*_NSGetArgv())
#else
extern const int __libc_argc;
extern const char* const *__libc_argv;
#endif


// debugging
//#define DBG(x) x
#define DBG(x)
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
	BMimeType type(signature);

	if (type.IsValid() && !type.IsSupertypeOnly()
		&& BMimeType("application").Contains(&type)) {
		isValid = true;
	}

	if (!isValid) {
		printf("bad signature (%s), must begin with \"application/\" and "
			   "can't conflict with existing registered mime types inside "
			   "the \"application\" media type.\n", signature);
	}

	return (isValid ? B_OK : B_BAD_VALUE);
}


static status_t
forward_scripting_message(BMessage* message, BHandler* target)
{
	if (message == NULL || target == NULL)
		return B_BAD_VALUE;

	BLooper* looper = target->Looper();
	if (looper == NULL || looper->Thread() < B_OK)
		return B_BAD_INDEX;

	status_t err = message->PopSpecifier();
	if (err != B_OK)
		return err;

	BMessenger messenger(target);
	return messenger.SendMessage(message, message->ReturnAddress());
}


static bool
is_scriptable_window(BWindow* window)
{
	return window != NULL && window->Thread() >= B_OK;
}


static int32
count_scriptable_windows(const BApplication* application)
{
	int32 count = 0;
	for (int32 i = 0; i < application->CountWindows(); i++) {
		if (is_scriptable_window(application->WindowAt(i)))
			count++;
	}

	return count;
}


static BWindow*
scriptable_window_at(const BApplication* application, int32 index)
{
	if (index < 0)
		return NULL;

	for (int32 i = 0; i < application->CountWindows(); i++) {
		BWindow* window = application->WindowAt(i);
		if (!is_scriptable_window(window))
			continue;

		if (index == 0)
			return window;

		index--;
	}

	return NULL;
}


cosmoe_display_t
BApplication::Display() const
{
	return fDisplay;
}


port_id
BApplication::BackendPort()
{
	return fBackendPort;
}


// Fills the passed BMessage with B_ARGV_RECEIVED infos.
static void
fill_argv_message(BMessage &message)
{
	message.what = B_ARGV_RECEIVED;

	int32 argc = __libc_argc;
	const char* const *argv = __libc_argv;

	// add argc
	message.AddInt32("argc", argc);

	// add argv
	for (int32 i = 0; i < argc; i++) {
		if (argv[i] != NULL)
			message.AddString("argv", argv[i]);
	}

	// add current working directory
	char cwd[B_PATH_NAME_LENGTH];
	if (getcwd(cwd, B_PATH_NAME_LENGTH))
		message.AddString("cwd", cwd);
}



//	#pragma mark - BApplication


BApplication::BApplication(const char* signature)
	:
	BLooper(kDefaultLooperName)
{
	_InitData(signature, true, NULL);
}


BApplication::BApplication(const char* signature, status_t* _error)
	:
	BLooper(kDefaultLooperName)
{
	_InitData(signature, true, _error);
}


BApplication::BApplication(BMessage* data)
	// Note: BeOS calls the private BLooper(int32, port_id, const char*)
	// constructor here, test if it's needed
	:
	BLooper(kDefaultLooperName)
{
	const char* signature = NULL;
	data->FindString("mime_sig", &signature);

	_InitData(signature, true, NULL);

	bigtime_t pulseRate;
	if (data->FindInt64("_pulse", &pulseRate) == B_OK)
		SetPulseRate(pulseRate);
}


#ifdef __HAIKU_BEOS_COMPATIBLE
BApplication::BApplication(uint32 signature)
{
}


BApplication::BApplication(const BApplication &rhs)
{
}


BApplication&
BApplication::operator=(const BApplication &rhs)
{
	return *this;
}
#endif


BApplication::~BApplication()
{
	Lock();

	// tell all loopers(usually windows) to quit. Also, wait for them.
	_QuitAllWindows(true);

	// quit registered loopers
	for (int32 i = 0; i < sOnQuitLooperList.CountItems(); i++) {
		BLooper* looper = sOnQuitLooperList.ItemAt(i);
		if (looper->Lock())
			looper->Quit();
	}

	// unregister from the roster
	BPrivate::UnregisterRemoteAppMessenger();

	// uninitialize be_app, the be_app_messenger is invalidated automatically
	be_app = NULL;
}


void
BApplication::_InitData(const char* signature, bool initGUI, status_t* _error)
{
#ifdef _WIN32
	// On Windows, DllMain is not being called by MinGW's loader, so we must
	// explicitly initialize libbe here. This registers the main thread and
	// initializes other core systems.
	// Note that this doesn't cover the case where libbe is used without
	// a BApplication, e.g. command-line tools.  This will need to be manually
	// initialized in those apps until a better solution is found.
	static bool initialized = false;
	if (!initialized) {
		initialized = true;
		__libbe_initialize_before();
	}
#endif
	DBG(OUT("BApplication::InitData(`%s', %p)\n", signature, _error));
	// check whether there exists already an application
	if (be_app != NULL)
		debugger("2 BApplication objects were created. Only one is allowed.");

	fServerLink = new BPrivate::PortLink(-1, -1);
	fInitialWorkspace = 0;
	fReadyToRunCalled = false;

	// initially, there is no pulse
	fPulseRunner = NULL;
	fPulseRate = 0;

	// check signature
	fInitError = check_app_signature(signature);
	fAppName = signature;

	// no custom cursor yet
	fCursorID = -1;

	bool registerApp = true;

	// get app executable ref
	entry_ref ref;
	if (fInitError == B_OK) {
		fInitError = BPrivate::get_app_ref(&ref);
		if (fInitError != B_OK) {
			DBG(OUT("BApplication::InitData(): Failed to get app ref: %s\n",
				strerror(fInitError)));
		}
	}

	if (fInitError == B_OK) {
			// Create a B_ARGV_RECEIVED message and send it to ourselves.
			// Do that even, if we are B_ARGV_ONLY.
			// TODO: When BLooper::AddMessage() is done, use that instead of
			// PostMessage().

			DBG(OUT("info: BApplication successfully registered.\n"));

			if (__libc_argc > 1) {
				BMessage argvMessage(B_ARGV_RECEIVED);
				fill_argv_message(argvMessage);
				PostMessage(&argvMessage, this);
			}
			// send a B_READY_TO_RUN message as well
			PostMessage(B_READY_TO_RUN, this);
	}

	if (fInitError == B_OK) {
		// TODO: Not completely sure about the order, but this should be close.

		// init be_app and be_app_messenger
		be_app = this;
		be_app_messenger = BMessenger(NULL, this);
		BPrivate::RegisterRemoteAppMessenger(Signature(), fMsgPort);

		// create meta MIME
		BPath path;
		if (registerApp && path.SetTo(&ref) == B_OK)
			create_app_meta_mime(path.Path(), false, true, false);

		if (initGUI)
			fInitError = _InitGUIContext();
	}

	// Return the error or exit, if there was an error and no error variable
	// has been supplied.
	if (_error != NULL) {
		*_error = fInitError;
	} else if (fInitError != B_OK) {
		DBG(OUT("BApplication::InitData() failed: %s\n", strerror(fInitError)));
		exit(0);
	}
DBG(OUT("BApplication::InitData() done\n"));
}


BArchivable*
BApplication::Instantiate(BMessage* data)
{
	if (validate_instantiation(data, "BApplication"))
		return new BApplication(data);

	return NULL;
}


status_t
BApplication::Archive(BMessage* data, bool deep) const
{
	status_t status = BLooper::Archive(data, deep);
	if (status < B_OK)
		return status;

	app_info info;
	status = GetAppInfo(&info);
	if (status < B_OK)
		return status;

	status = data->AddString("mime_sig", info.signature);
	if (status < B_OK)
		return status;

	return data->AddInt64("_pulse", fPulseRate);
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

	delete fPulseRunner;
	return fThread;
}


void
BApplication::Quit()
{
	bool unlock = false;
	if (!IsLocked()) {
		const char* name = Name();
		if (name == NULL)
			name = "no-name";

		printf("ERROR - you must Lock the application object before calling "
			   "Quit(), team=%" B_PRId32 ", looper=%s\n", Team(), name);
		unlock = true;
		if (!Lock())
			return;
	}
	// Delete the object, if not running only.
	if (!fRunCalled) {
		delete this;
	} else if (find_thread(NULL) != fThread) {
// ToDo: why shouldn't we set fTerminating to true directly in this case?
		// We are not the looper thread.
		// We push a _QUIT_ into the queue.
		// TODO: When BLooper::AddMessage() is done, use that instead of
		// PostMessage()??? This would overtake messages that are still at
		// the port.
		// NOTE: We must not unlock here -- otherwise we had to re-lock, which
		// may not work. This is bad, since, if the port is full, it
		// won't get emptier, as the looper thread needs to lock the object
		// before dispatching messages.
		while (PostMessage(_QUIT_, this) == B_WOULD_BLOCK)
			snooze(10000);
	} else {
		// We are the looper thread.
		// Just set fTerminating to true which makes us fall through the
		// message dispatching loop and return from Run().
		fTerminating = true;
	}

	// If we had to lock the object, unlock now.
	if (unlock)
		Unlock();

	if (fDisplay != NULL) {
		// Stop backend event loop before destroying backend display objects.
		// Destroying first can race with the display thread still running.
		cosmoe_display_exit(fDisplay);

		if (BWindow::sDisplayThread >= 0
			&& find_thread(NULL) != BWindow::sDisplayThread) {
			status_t threadResult = B_OK;
			wait_for_thread(BWindow::sDisplayThread, &threadResult);
			BWindow::sDisplayThread = -1;
		}

		cosmoe_display_destroy(fDisplay);
		fDisplay = NULL;
	}
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
BApplication::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case B_COUNT_PROPERTIES:
		case B_GET_PROPERTY:
		case B_SET_PROPERTY:
		{
			int32 index;
			BMessage specifier;
			int32 what;
			const char* property = NULL;
			if (message->GetCurrentSpecifier(&index, &specifier, &what,
					&property) < B_OK
				|| !ScriptReceived(message, index, &specifier, what,
					property)) {
				BLooper::MessageReceived(message);
			}
			break;
		}

		case B_SILENT_RELAUNCH:
			// Sent to a B_SINGLE_LAUNCH application when it's launched again
			// (see _InitData())
			break;

		default:
			BLooper::MessageReceived(message);
	}
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
BApplication::RefsReceived(BMessage* message)
{
	// supposed to be implemented by subclasses
}


void
BApplication::AboutRequested()
{
	// supposed to be implemented by subclasses
}


BHandler*
BApplication::ResolveSpecifier(BMessage* message, int32 index,
	BMessage* specifier, int32 what, const char* property)
{
	BPropertyInfo propInfo(sPropertyInfo);
	status_t err = B_OK;
	uint32 data;

	if (propInfo.FindMatch(message, 0, specifier, what, property, &data) >= 0) {
		switch (data) {
			case kWindowByIndex:
			{
				int32 index;
				err = specifier->FindInt32("index", &index);
				if (err != B_OK)
					break;

				if (what == B_REVERSE_INDEX_SPECIFIER)
					index = count_scriptable_windows(this) - index;

				BWindow* window = scriptable_window_at(this, index);
				if (window != NULL)
					err = forward_scripting_message(message, window);
				else
					err = B_BAD_INDEX;
				break;
			}

			case kWindowByName:
			{
				const char* name;
				err = specifier->FindString("name", &name);
				if (err != B_OK)
					break;

				for (int32 i = 0;; i++) {
					BWindow* window = WindowAt(i);
					if (window == NULL) {
						err = B_NAME_NOT_FOUND;
						break;
					}
					if (!is_scriptable_window(window))
						continue;
					if (window->Title() != NULL && !strcmp(window->Title(),
							name)) {
						err = forward_scripting_message(message, window);
						break;
					}
				}
				break;
			}

			case kLooperByIndex:
			{
				int32 index;
				err = specifier->FindInt32("index", &index);
				if (err != B_OK)
					break;

				if (what == B_REVERSE_INDEX_SPECIFIER)
					index = CountLoopers() - index;

				BLooper* looper = LooperAt(index);
				if (looper != NULL)
					err = forward_scripting_message(message, looper);
				else
					err = B_BAD_INDEX;

				break;
			}

			case kLooperByID:
				// TODO: implement getting looper by ID!
				break;

			case kLooperByName:
			{
				const char* name;
				err = specifier->FindString("name", &name);
				if (err != B_OK)
					break;

				for (int32 i = 0;; i++) {
					BLooper* looper = LooperAt(i);
					if (looper == NULL) {
						err = B_NAME_NOT_FOUND;
						break;
					}
					if (looper->Name() != NULL
						&& strcmp(looper->Name(), name) == 0) {
						err = forward_scripting_message(message, looper);
						break;
					}
				}
				break;
			}

			case kApplication:
				ScriptReceived(message, index, specifier, what, property);
				return NULL;
		}
	} else {
		return BLooper::ResolveSpecifier(message, index, specifier, what,
			property);
	}

	if (err != B_OK) {
		BMessage reply(B_MESSAGE_NOT_UNDERSTOOD);
		reply.AddInt32("error", err);
		reply.AddString("message", strerror(err));
		message->SendReply(&reply);
	}

	return NULL;

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
	BCursor cursor(cursorData);
	SetCursor(&cursor, true);
		// forces the cursor to be sync'ed
}


void
BApplication::SetCursor(const BCursor* cursor, bool sync)
{
	if (cursor)
		fCursorID = cursor->fServerToken;
	else
		fCursorID = -1;
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
	if (be_app == NULL || be_roster == NULL)
		return B_NO_INIT;
	return be_roster->GetRunningAppInfo(be_app->Team(), info);
}


BResources*
BApplication::AppResources()
{
	if (sAppResources == NULL)
		pthread_once(&sAppResourcesInitOnce, &_InitAppResources);

	return sAppResources;
}


void
BApplication::DispatchMessage(BMessage* message, BHandler* handler)
{
	if (handler != this) {
		// it's not ours to dispatch
		BLooper::DispatchMessage(message, handler);
		return;
	}

	switch (message->what) {
		case B_ARGV_RECEIVED:
			_ArgvReceived(message);
			break;

		case B_REFS_RECEIVED:
		{
			// this adds the refs that are part of this message to the recent
			// lists, but only folders and documents are handled here
			entry_ref ref;
			int32 i = 0;
			while (message->FindRef("refs", i++, &ref) == B_OK) {
				BEntry entry(&ref, true);
				if (entry.InitCheck() != B_OK)
					continue;

				if (entry.IsDirectory())
					BRoster().AddToRecentFolders(&ref);
				else {
					// filter out applications, we only want to have documents
					// in the recent files list
					BNode node(&entry);
					BNodeInfo info(&node);

					char mimeType[B_MIME_TYPE_LENGTH];
					if (info.GetType(mimeType) != B_OK
						|| strcasecmp(mimeType, B_APP_MIME_TYPE))
						BRoster().AddToRecentDocuments(&ref);
				}
			}

			RefsReceived(message);
			break;
		}

		case B_READY_TO_RUN:
			if (!fReadyToRunCalled) {
				ReadyToRun();
				fReadyToRunCalled = true;
			}
			break;

		case B_ABOUT_REQUESTED:
			AboutRequested();
			break;

		case B_PULSE:
			Pulse();
			break;

		case B_APP_ACTIVATED:
		{
			bool active;
			if (message->FindBool("active", &active) == B_OK)
				AppActivated(active);
			break;
		}

		case B_COLORS_UPDATED:
		{
			AutoLocker<BLooperList> listLock(gLooperList);
			if (!listLock.IsLocked())
				break;

			BWindow* window = NULL;
			uint32 count = gLooperList.CountLoopers();
			for (uint32 index = 0; index < count; ++index) {
				window = dynamic_cast<BWindow*>(gLooperList.LooperAt(index));
				if (window == NULL || (window != NULL && window->fOffscreen))
					continue;
				window->PostMessage(message);
			}
			break;
		}

		case _SHOW_DRAG_HANDLES_:
		{
			bool show;
			if (message->FindBool("show", &show) != B_OK)
				break;

			BDragger::Private::UpdateShowAllDraggers(show);
			break;
		}

		// TODO: Handle these as well
		case _DISPOSE_DRAG_:
		case _PING_:
			puts("not yet handled message:");
			DBG(message->PrintToStream());
			break;

		default:
			BLooper::DispatchMessage(message, handler);
			break;
	}
}


void
BApplication::SetPulseRate(bigtime_t rate)
{
	if (rate < 0)
		rate = 0;

	// BeBook states that we have only 100,000 microseconds granularity
	rate -= rate % 100000;

	if (!Lock())
		return;

	if (rate != 0) {
		// reset existing pulse runner, or create new one
		if (fPulseRunner == NULL) {
			BMessage pulse(B_PULSE);
			fPulseRunner = new BMessageRunner(be_app_messenger, &pulse, rate);
		} else
			fPulseRunner->SetInterval(rate);
	} else {
		// turn off pulse messages
		delete fPulseRunner;
		fPulseRunner = NULL;
	}

	fPulseRate = rate;
	Unlock();
}


status_t
BApplication::GetSupportedSuites(BMessage* data)
{
	if (data == NULL)
		return B_BAD_VALUE;

	status_t status = data->AddString("suites", "suite/vnd.Be-application");
	if (status == B_OK) {
		BPropertyInfo propertyInfo(sPropertyInfo);
		status = data->AddFlat("messages", &propertyInfo);
		if (status == B_OK)
			status = BLooper::GetSupportedSuites(data);
	}

	return status;
}


status_t
BApplication::Perform(perform_code d, void* arg)
{
	return BLooper::Perform(d, arg);
}


void BApplication::_ReservedApplication1() {}
void BApplication::_ReservedApplication2() {}
void BApplication::_ReservedApplication3() {}
void BApplication::_ReservedApplication4() {}
void BApplication::_ReservedApplication5() {}
void BApplication::_ReservedApplication6() {}
void BApplication::_ReservedApplication7() {}
void BApplication::_ReservedApplication8() {}


bool
BApplication::ScriptReceived(BMessage* message, int32 index,
	BMessage* specifier, int32 what, const char* property)
{
	BMessage reply(B_REPLY);
	status_t err = B_BAD_SCRIPT_SYNTAX;

	switch (message->what) {
		case B_GET_PROPERTY:
			if (strcmp("Loopers", property) == 0) {
				int32 count = CountLoopers();
				err = B_OK;
				for (int32 i=0; err == B_OK && i<count; i++) {
					BMessenger messenger(LooperAt(i));
					err = reply.AddMessenger("result", messenger);
				}
			} else if (strcmp("Windows", property) == 0) {
				int32 count = count_scriptable_windows(this);
				err = B_OK;
				for (int32 i=0; err == B_OK && i<count; i++) {
					BMessenger messenger(scriptable_window_at(this, i));
					err = reply.AddMessenger("result", messenger);
				}
			} else if (strcmp("Window", property) == 0) {
				switch (what) {
					case B_INDEX_SPECIFIER:
					case B_REVERSE_INDEX_SPECIFIER:
					{
						int32 index = -1;
						err = specifier->FindInt32("index", &index);
						if (err != B_OK)
							break;

						if (what == B_REVERSE_INDEX_SPECIFIER)
							index = count_scriptable_windows(this) - index;

						err = B_BAD_INDEX;
						BWindow* window = scriptable_window_at(this, index);
						if (window == NULL)
							break;

						BMessenger messenger(window);
						err = reply.AddMessenger("result", messenger);
						break;
					}

					case B_NAME_SPECIFIER:
					{
						const char* name;
						err = specifier->FindString("name", &name);
						if (err != B_OK)
							break;
						err = B_NAME_NOT_FOUND;
						for (int32 i = 0; i < CountWindows(); i++) {
							BWindow* window = WindowAt(i);
							if (is_scriptable_window(window) && window->Title() != NULL
								&& !strcmp(window->Title(), name)) {
								BMessenger messenger(window);
								err = reply.AddMessenger("result", messenger);
								break;
							}
						}
						break;
					}
				}
			} else if (strcmp("Looper", property) == 0) {
				switch (what) {
					case B_INDEX_SPECIFIER:
					case B_REVERSE_INDEX_SPECIFIER:
					{
						int32 index = -1;
						err = specifier->FindInt32("index", &index);
						if (err != B_OK)
							break;

						if (what == B_REVERSE_INDEX_SPECIFIER)
							index = CountLoopers() - index;

						err = B_BAD_INDEX;
						BLooper* looper = LooperAt(index);
						if (looper == NULL)
							break;

						BMessenger messenger(looper);
						err = reply.AddMessenger("result", messenger);
						break;
					}

					case B_NAME_SPECIFIER:
					{
						const char* name;
						err = specifier->FindString("name", &name);
						if (err != B_OK)
							break;
						err = B_NAME_NOT_FOUND;
						for (int32 i = 0; i < CountLoopers(); i++) {
							BLooper* looper = LooperAt(i);
							if (looper != NULL && looper->Name()
								&& strcmp(looper->Name(), name) == 0) {
								BMessenger messenger(looper);
								err = reply.AddMessenger("result", messenger);
								break;
							}
						}
						break;
					}

					case B_ID_SPECIFIER:
					{
						// TODO
						debug_printf("Looper's ID specifier used but not "
							"implemented.\n");
						break;
					}
				}
			} else if (strcmp("Name", property) == 0)
				err = reply.AddString("result", Name());

			break;

		case B_COUNT_PROPERTIES:
			if (strcmp("Looper", property) == 0)
				err = reply.AddInt32("result", CountLoopers());
			else if (strcmp("Window", property) == 0)
				err = reply.AddInt32("result", count_scriptable_windows(this));

			break;
	}
	if (err == B_BAD_SCRIPT_SYNTAX)
		return false;

	if (err < B_OK) {
		reply.what = B_MESSAGE_NOT_UNDERSTOOD;
		reply.AddString("message", strerror(err));
	}
	reply.AddInt32("error", err);
	message->SendReply(&reply);

	return true;
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
BApplication::_InitGUIContext()
{
	fDisplay = cosmoe_display_create(NULL, NULL);
	if (fDisplay == NULL) {
#ifdef _WIN32
		MessageBoxA(NULL, 
			"Failed to create display!\n\n"
			"The backend DLL failed to initialize.\n"
			"Check that libcosmoe-windows.dll and all its dependencies are present.",
			"Cosmoe Application Error", 
			MB_OK | MB_ICONERROR);
#endif
		fprintf(stderr, "BApplication::_InitGUIContext(): cosmoe_display_create() failed\n");
		return B_ERROR;
	}
	
	const char* backendName = cosmoe_backend_get_current_name();
	if (backendName != NULL) {
		DBG(OUT("Using %s backend\n", backendName));
	} else {
		DBG(OUT("Warning: Backend name is NULL\n"));
	}

	// An app_server connection is necessary for a lot of stuff, so get that first.
	status_t error = _ConnectToServer();
	if (error != B_OK)
		return error;

	error = _init_interface_kit_();
	if (error != B_OK)
		return error;

	// create global system cursors
	B_CURSOR_SYSTEM_DEFAULT = new BCursor(B_HAND_CURSOR);
	B_CURSOR_I_BEAM = new BCursor(B_I_BEAM_CURSOR);

	return B_OK;
}


// For Cosmoe, this is more accurately _ConnectToBackend() but we leave the name to keep things in sync
status_t
BApplication::_ConnectToServer()
{
	// Create backend communication port for window operations
	// The display thread will read from this port
	port_id backendPort = create_port(100, "backend_port");
	port_id appPort = create_port(100, "app_port");
	if (backendPort < 0 || appPort < 0) {
		fprintf(stderr, "BApplication::_InitGUIContext(): Failed to create backend or app port\n");
		return B_ERROR;
	}
	DBG(OUT("BApplication::_InitGUIContext(): Created backend port %d and app port %d\n", (int)backendPort, (int)appPort));
	fBackendPort = backendPort;
	
	// Initialize PortLink for sending messages to the backend
	// PortLink(send_port, receive_port)
	// - send_port: where we WRITE (backend reads) → backendPort
	// - receive_port: where we READ (backend writes) → appPort
	fServerLink = new BPrivate::PortLink(backendPort, appPort);
	
	// Tell the display to listen on this port
	cosmoe_display_set_port(fDisplay, backendPort, appPort);
	DBG(OUT("BApplication::_InitGUIContext(): Set display port to %d\n", (int)backendPort));

	return B_OK;
}


void
BApplication::_ReconnectToServer()
{
	// For Haiku compatibilty only - this should never be needed on Cosmoe.
}

bool
BApplication::_WindowQuitLoop(bool quitFilePanels, bool force)
{
	int32 index = 0;
	while (true) {
		BMessenger windowMessenger;
		bool foundCandidate = false;

		{
			AutoLocker<BLooperList> listLock(gLooperList);
			if (!listLock.IsLocked())
				return false;

			uint32 visibleIndex = 0;
			int32 count = gLooperList.CountLoopers();
			for (int32 i = 0; i < count; i++) {
				BWindow* candidate
					= dynamic_cast<BWindow*>(gLooperList.LooperAt(i));
				if (candidate == NULL || candidate->fOffscreen
					|| dynamic_cast<BMenuWindow*>(candidate) != NULL) {
					continue;
				}

				if (visibleIndex++ != (uint32)index)
					continue;

				foundCandidate = true;
				windowMessenger = BMessenger(candidate);
				break;
			}
		}

		if (!foundCandidate)
			break;

		if (!windowMessenger.LockTarget()) {
			index = 0;
			continue;
		}

		BLooper* looper = NULL;
		BWindow* window = dynamic_cast<BWindow*>(windowMessenger.Target(&looper));
		if (window == NULL || looper != window) {
			if (looper != NULL)
				looper->Unlock();
			index = 0;
			continue;
		}

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


void
BApplication::_ArgvReceived(BMessage* message)
{
	ASSERT(message != NULL);

	// build the argv vector
	status_t error = B_OK;
	int32 argc = 0;
	char** argv = NULL;
	if (message->FindInt32("argc", &argc) == B_OK && argc > 0) {
		// allocate a NULL terminated array
		argv = new(std::nothrow) char*[argc + 1];
		if (argv == NULL)
			return;

		// copy the arguments
		for (int32 i = 0; error == B_OK && i < argc; i++) {
			const char* arg = NULL;
			error = message->FindString("argv", i, &arg);
			if (error == B_OK && arg) {
				argv[i] = strdup(arg);
				if (argv[i] == NULL)
					error = B_NO_MEMORY;
			} else
				argc = i;
		}

		argv[argc] = NULL;
	}

	// call the hook
	if (error == B_OK && argc > 0)
		ArgvReceived(argc, argv);

	if (error != B_OK) {
		printf("Error parsing B_ARGV_RECEIVED message. Message:\n");
		message->PrintToStream();
	}

	// cleanup
	if (argv) {
		for (int32 i = 0; i < argc; i++)
			free(argv[i]);
		delete[] argv;
	}
}


uint32
BApplication::InitialWorkspace()
{
	return fInitialWorkspace;
}


int32
BApplication::_CountWindows(bool includeMenus) const
{
	uint32 count = 0;
	for (int32 i = 0; i < gLooperList.CountLoopers(); i++) {
		BWindow* window = dynamic_cast<BWindow*>(gLooperList.LooperAt(i));
		if (window != NULL && !window->fOffscreen && (includeMenus
				|| dynamic_cast<BMenuWindow*>(window) == NULL)) {
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
		if (window == NULL || (window != NULL && window->fOffscreen)
			|| (!includeMenus && dynamic_cast<BMenuWindow*>(window) != NULL)) {
			index++;
			continue;
		}

		if (i == index)
			return window;
	}

	return NULL;
}


/*static*/ void
BApplication::_InitAppResources()
{
	entry_ref ref;
	bool found = false;

	// App is already running. Get its entry ref with
	// GetAppInfo()
	app_info appInfo;
	if (be_app && be_app->GetAppInfo(&appInfo) == B_OK) {
		ref = appInfo.ref;
		found = true;
	} else {
		// Run() hasn't been called yet
		found = BPrivate::get_app_ref(&ref) == B_OK;
	}

	if (!found)
		return;

	BFile file(&ref, B_READ_ONLY);
	if (file.InitCheck() != B_OK)
		return;

	BResources* resources = new (std::nothrow) BResources(&file, false);
	if (resources == NULL || resources->InitCheck() != B_OK) {
		delete resources;
		return;
	}

	sAppResources = resources;
}
