/*
 * Copyright 2001-2015, Haiku.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Ingo Weinhold (bonefish@users.sf.net)
 */
#ifndef _ROSTER_PRIVATE_H
#define _ROSTER_PRIVATE_H


#include <Messenger.h>
#include <Roster.h>


const uint32 kMsgAppServerStarted = '_ASt';


class BRoster::Private {
	public:
		Private() : fRoster(const_cast<BRoster*>(be_roster)) {}
		Private(BRoster &roster) : fRoster(&roster) {}
		Private(BRoster *roster) : fRoster(roster) {}

#if 0
		void SetTo(BMessenger mainMessenger, BMessenger mimeMessenger);

		status_t SendTo(BMessage *message, BMessage *reply, bool mime);
		bool IsMessengerValid(bool mime) const;
#endif
		status_t Launch(const char* mimeType, const entry_ref* ref,
					const BList* messageList, int argc, const char* const* args,
					const char** environment, team_id* appTeam,
					thread_id* appThread, port_id* appPort, uint32* appToken,
					bool launchSuspended)
			{ return fRoster->_LaunchApp(mimeType, ref, messageList, argc,
					args, environment, appTeam, appThread, appPort, appToken,
					launchSuspended); }

		status_t ShutDown(bool reboot, bool confirm, bool synchronous)
			{ return fRoster->_ShutDown(reboot, confirm, synchronous); }
		status_t LogOut(bool confirm)
			{ return fRoster->_Logout(confirm); }
		status_t Suspend(bool confirm)
			{ return fRoster->_Suspend(confirm); }

		// needed by BApplication

		status_t AddApplication(const char *mimeSig, const entry_ref *ref,
					uint32 flags, team_id team, thread_id thread,
					port_id port, bool fullReg, uint32 *token,
					team_id *otherTeam) const
			{ return fRoster->_AddApplication(mimeSig, ref, flags, team, thread,
					port, fullReg, token, otherTeam); }

		// needed by GetRecentTester

		void AddToRecentApps(const char *appSig) const
			{ fRoster->_AddToRecentApps(appSig); }

		void ClearRecentDocuments() const
			{ fRoster->_ClearRecentDocuments(); }

		void ClearRecentFolders() const
			{ fRoster->_ClearRecentFolders(); }

		void ClearRecentApps() const
			{ fRoster->_ClearRecentApps(); }

		void LoadRecentLists(const char *file) const
			{ fRoster->_LoadRecentLists(file); }

		void SaveRecentLists(const char *file) const
			{ fRoster->_SaveRecentLists(file); }

		static void InitBeRoster();
		static void DeleteBeRoster();

	private:
		BRoster	*fRoster;
};

#endif	// _ROSTER_PRIVATE_H
