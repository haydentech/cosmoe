#ifndef _LINUX_REMOTE_APP_MESSAGING_H
#define _LINUX_REMOTE_APP_MESSAGING_H


#include <OS.h>


class BMessage;
class BMessenger;
struct app_info;

namespace BPrivate {

status_t RegisterRemoteAppMessenger(const char* signature, port_id port);
void UnregisterRemoteAppMessenger();

status_t FindRemoteAppMessenger(const char* signature, team_id team,
	app_info* info);

bool SendRemoteAppMessage(port_id port, team_id team, int32 token,
	BMessage* message, BMessenger replyTo, bigtime_t timeout,
	status_t* _result);

bool SendRemoteAppMessage(port_id port, team_id team, int32 token,
	BMessage* message, BMessage* reply, bigtime_t deliveryTimeout,
	bigtime_t replyTimeout, status_t* _result);

} // namespace BPrivate


#endif	// _LINUX_REMOTE_APP_MESSAGING_H
