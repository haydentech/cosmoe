/*
 * Linux compatibility shim for BSD sockaddr_dl structure
 * This provides a minimal implementation to allow BSD network code to compile on Linux
 */
#ifndef _SOCKADDR_DL_LINUX_H
#define _SOCKADDR_DL_LINUX_H

#ifndef __HAIKU__
#ifndef __BSD__

#include <sys/types.h>

/* AF_LINK is typically 18 on BSD systems, but not defined on Linux */
#ifndef AF_LINK
#define AF_LINK 18
#endif

/*
 * Structure of a Link-Level sockaddr (BSD compatibility)
 * This is a compatibility shim - on Linux you would normally use sockaddr_ll from <linux/if_packet.h>
 */
struct sockaddr_dl {
	u_int8_t    sdl_len;        /* Total length of sockaddr */
	u_int8_t    sdl_family;     /* AF_LINK */
	u_int16_t   sdl_index;      /* if != 0, system given index for interface */
	u_int8_t    sdl_type;       /* interface type */
	u_int8_t    sdl_nlen;       /* interface name length, no trailing 0 reqd. */
	u_int8_t    sdl_alen;       /* link level address length */
	u_int8_t    sdl_slen;       /* link layer selector length */
	char        sdl_data[46];   /* contains both if name and ll address */
	u_int16_t   sdl_e_type;     /* Ethernet protocol type (extension) */
};

/* Macro to get link-level address from sockaddr_dl */
#define LLADDR(s) ((uint8_t *)((s)->sdl_data + (s)->sdl_nlen))

#endif /* __BSD__ */
#endif /* __HAIKU__ */

#endif /* _SOCKADDR_DL_LINUX_H */
