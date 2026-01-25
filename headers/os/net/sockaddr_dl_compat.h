/*
 * Compatibility shim for BSD sockaddr_dl structure
 * This provides a minimal implementation to allow BSD network code to compile
 * on Linux and Windows (any platform without native sockaddr_dl support)
 */
#ifndef _SOCKADDR_DL_COMPAT_H
#define _SOCKADDR_DL_COMPAT_H

#if !defined(__HAIKU__) && !defined(__BSD__) && !defined(__APPLE__)

#include <stdint.h>

/* AF_LINK is typically 18 on BSD systems, but not defined on Linux/Windows */
#ifndef AF_LINK
#define AF_LINK 18
#endif

/*
 * Structure of a Link-Level sockaddr (BSD compatibility)
 * This is a compatibility shim - on Linux you would normally use sockaddr_ll from <linux/if_packet.h>
 */
struct sockaddr_dl {
	uint8_t     sdl_len;        /* Total length of sockaddr */
	uint8_t     sdl_family;     /* AF_LINK */
	uint16_t    sdl_index;      /* if != 0, system given index for interface */
	uint8_t     sdl_type;       /* interface type */
	uint8_t     sdl_nlen;       /* interface name length, no trailing 0 reqd. */
	uint8_t     sdl_alen;       /* link level address length */
	uint8_t     sdl_slen;       /* link layer selector length */
	char        sdl_data[46];   /* contains both if name and ll address */
	uint16_t    sdl_e_type;     /* Ethernet protocol type (extension) */
};

/* Macro to get link-level address from sockaddr_dl */
#define LLADDR(s) ((uint8_t *)((s)->sdl_data + (s)->sdl_nlen))

#endif /* !__HAIKU__ && !__BSD__ && !__APPLE__ */

#endif /* _SOCKADDR_DL_COMPAT_H */
