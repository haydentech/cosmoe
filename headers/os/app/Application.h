/*
 * Copyright 2001-2015 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _APPLICATION_H
#define _APPLICATION_H


#include <AppDefs.h>
#include <InterfaceDefs.h>
#include <Looper.h>
#include <Messenger.h>
#include <Point.h>
#include <Rect.h>

extern "C" {
#include "window.h"
}

class BList;
class BLocker;
class BMessageRunner;
class BResources;
class BWindow;

//struct app_info;


class BApplication : public BLooper {
public:
								BApplication(const char* signature);
								BApplication(const char* signature,
									status_t* error);
	virtual						~BApplication();

	// Archiving
								BApplication(BMessage* data);
	static	BArchivable*		Instantiate(BMessage* data);
	virtual	status_t			Archive(BMessage* data, bool deep = true) const;

			status_t			InitCheck() const;

	// App control and System Message handling
	virtual	thread_id			Run();
	virtual	void				Quit();
	virtual bool				QuitRequested();
	virtual	void				Pulse();
	virtual	void				ReadyToRun();
	virtual	void				MessageReceived(BMessage* message);
	virtual	void				ArgvReceived(int32 argc, char** argv);
	virtual	void				AppActivated(bool active);
	virtual	void				RefsReceived(BMessage* message);
	virtual	void				AboutRequested();

	// Scripting
	virtual BHandler*			ResolveSpecifier(BMessage* message, int32 index,
									BMessage* specifier, int32 form,
									const char* property);

	// Cursor control, window/looper list, and app info
			void				ShowCursor();
			void				HideCursor();
			void				ObscureCursor();
			bool				IsCursorHidden() const;
			void				SetCursor(const void* cursor);

			int32				CountWindows() const;
			BWindow*			WindowAt(int32 index) const;

			int32				CountLoopers() const;
			BLooper*			LooperAt(int32 index) const;
			bool				IsLaunching() const;
			const char*			Signature() const;
			//status_t			GetAppInfo(app_info* info) const;

	virtual	void				DispatchMessage(BMessage* message,
									BHandler* handler);
			void				SetPulseRate(bigtime_t rate);

	// Register a BLooper to be quit before the BApplication
	// object is destroyed.
			status_t			RegisterLooper(BLooper* looper);
			status_t			UnregisterLooper(BLooper* looper);
	class Private;

	// More scripting
	virtual status_t			GetSupportedSuites(BMessage* data);


	// Private or reserved
	virtual status_t			Perform(perform_code d, void* arg);

	class Private;

	display*			WaylandDisplay() const { return fWaylandDisplay; }

private:
	typedef BLooper _inherited;

	friend class Private;
	friend class BServer;

								BApplication(uint32 signature);
								BApplication(const BApplication&);
			BApplication&		operator=(const BApplication&);

	virtual	void				_ReservedApplication1();
	virtual	void				_ReservedApplication2();
	virtual	void				_ReservedApplication3();
	virtual	void				_ReservedApplication4();
	virtual	void				_ReservedApplication5();
	virtual	void				_ReservedApplication6();
	virtual	void				_ReservedApplication7();
	virtual	void				_ReservedApplication8();

	virtual	bool				ScriptReceived(BMessage* msg, int32 index,
									BMessage* specifier, int32 form,
									const char* property);
			void				_InitData(const char* signature, bool initGUI,
									status_t* error);
			void				BeginRectTracking(BRect r, bool trackWhole);
			void				EndRectTracking();
			status_t			_SetupServerAllocator();
			status_t			_InitGUIContext();
			status_t			_ConnectToServer();
			void				_ReconnectToServer();
			bool				_QuitAllWindows(bool force);
			bool				_WindowQuitLoop(bool quitFilePanels,
									bool force);
			void				_ArgvReceived(BMessage* message);

			uint32				InitialWorkspace();
			int32				_CountWindows(bool includeMenus) const;
			BWindow*			_WindowAt(uint32 index,
									bool includeMenus) const;

	static	void				_InitAppResources();

private:
	static	BResources*			sAppResources;

			const char*			fAppName;

			void*				fCursorData;
			bigtime_t			fPulseRate;
			uint32				fInitialWorkspace;
			status_t			fInitError;
			uint32				_reserved[12];

			bool				fReadyToRunCalled;

			// Wayland/Weston support

			static inline struct display *fWaylandDisplay = NULL;
};


// Global Objects

extern BApplication* be_app;
extern BMessenger be_app_messenger;


#endif	// _APPLICATION_H
