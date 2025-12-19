/*
 * Apple-specific compatibility shims.
 *
 * This header provides short-type aliases (int32/uint64/etc.) when building
 * Cocoa backend translation units on macOS. It tries to avoid redefining
 * existing names by checking for prior typedefs or macros.
 *
 * This is a targeted shim for the Cocoa backend only. It should be kept
 * minimal and temporary until a proper refactor to stdint types is done.
 */

#ifndef _APPLE_COMPAT_H
#define _APPLE_COMPAT_H

#if defined(__APPLE__)
#include <stdint.h>

/* This shim should only create the project's short typedefs when the
 * consumer explicitly requests to disable the normal SupportDefs short
 * typedefs. That is done by defining COSMOE_NO_SUPPORT_TYPES before
 * including project headers. When that macro is set, provide the
 * short names mapped to the standard intN_t names. Otherwise be a no-op
 * to avoid colliding with SupportDefs.h or system headers.
 */
#if defined(COSMOE_NO_SUPPORT_TYPES)
typedef int8_t int8;
typedef uint8_t uint8;
typedef int16_t int16;
typedef uint16_t uint16;
typedef int32_t int32;
typedef uint32_t uint32;
typedef int64_t int64;
typedef uint64_t uint64;
#endif

#endif /* __APPLE__ */

#endif /* _APPLE_COMPAT_H */
