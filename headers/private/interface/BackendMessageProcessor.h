/*
 * Copyright 2025, Cosmoe Project
 * Distributed under the terms of the MIT License.
 *
 * Backend message processor - handles PortLink protocol for all backends
 */

#ifndef _BACKEND_MESSAGE_PROCESSOR_H
#define _BACKEND_MESSAGE_PROCESSOR_H

#include <OS.h>
#include "CosmoeBackend.h"

namespace BPrivate {

class BackendMessageProcessor {
public:
	// Process one message from the backend port
	// Called by backend's display loop to handle PortLink messages
	static void ProcessMessages(CosmoeBackend* backend, 
	                           port_id backend_port, 
	                           port_id app_port);
};

} // namespace BPrivate

#endif // _BACKEND_MESSAGE_PROCESSOR_H
