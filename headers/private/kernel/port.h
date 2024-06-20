/*
 * Copyright 2005-2009, Haiku Inc. All Rights Reserved.
 * Distributed under the terms of the MIT license.
 */
#ifndef _KERNEL_PORT_H
#define _KERNEL_PORT_H


//#include <thread.h>
//#include <iovec.h>

//struct kernel_args;
struct select_info;


#define PORT_FLAG_USE_USER_MEMCPY 0x80000000

// port flags
enum {
	// read_port_etc() flags
	B_PEEK_PORT_MESSAGE		= 0x100	// read the message, but don't remove it;
						// kernel-only; memory must be locked
};

// port notifications
#define PORT_MONITOR	'_Pm_'
#define PORT_ADDED		0x01
#define PORT_REMOVED	0x02

#ifdef __cplusplus
extern "C" {
#endif

status_t port_init(struct kernel_args *args);
int delete_owned_ports(team_id owner);
int32 port_max_ports(void);
int32 port_used_ports(void);

status_t select_port(int32 object, struct select_info *info, bool kernel);
status_t deselect_port(int32 object, struct select_info *info, bool kernel);

// currently private API
status_t writev_port_etc(port_id id, int32 msgCode, const iovec *msgVecs,
			size_t vecCount, size_t bufferSize, uint32 flags,
			bigtime_t timeout);

// temp: test
void port_test(void);

#ifdef __cplusplus
}
#endif

#endif	/* _KERNEL_PORT_H */
