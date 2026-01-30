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

		// needed by GetRecentTester

		void ClearRecentDocuments() const
			{ fRoster->_ClearRecentDocuments(); }

		void ClearRecentFolders() const
			{ fRoster->_ClearRecentFolders(); }

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
