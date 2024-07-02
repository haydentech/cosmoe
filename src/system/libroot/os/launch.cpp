/*
 * Copyright 2015, Axel Dörfler, axeld@pinc-software.de.
 * Distributed under the terms of the MIT License.
 */


#include <launch.h>

#include <TokenSpace.h>


static port_id sLaunchDaemonPort = -1;


port_id
BPrivate::get_launch_daemon_port()
{
	if (sLaunchDaemonPort < 0) {
		sLaunchDaemonPort = find_port(B_LAUNCH_DAEMON_PORT_NAME);

		port_info info;
		if (get_port_info(sLaunchDaemonPort, &info) == B_OK
			&& info.team == find_thread(NULL)) {
			// Make sure that the launch_daemon doesn't wait on itself
			sLaunchDaemonPort = -1;
			return -1;
		}
	}

	return sLaunchDaemonPort;
}
