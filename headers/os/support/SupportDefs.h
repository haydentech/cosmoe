/*
 * Copyright 2004-2010 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Author:
 *		Erik Jaesler (erik@cgsoftware.com)
 */
#ifndef _SUPPORT_DEFS_H
#define _SUPPORT_DEFS_H


#include <BeBuild.h>
#include <Errors.h>

#include <inttypes.h>
#include <sys/types.h>

#include "config/types.h"

/* fixed-size integer types */
/* The project defines a set of short integer type names (int8/uint8/...
	that may collide with platform headers on some systems (notably macOS).
	Consumers that want to use system typedefs (for example when including
	Cocoa/Foundation headers) can define COSMOE_NO_SUPPORT_TYPES before
	including this header to skip these typedefs.
*/
/* fixed-size integer types */
#ifndef COSMOE_NO_SUPPORT_TYPES
typedef	__haiku_int8			int8;
typedef __haiku_uint8			uint8;
typedef	__haiku_int16			int16;
typedef __haiku_uint16			uint16;
typedef	__haiku_int32			int32;
typedef __haiku_uint32			uint32;
typedef	__haiku_int64			int64;
typedef __haiku_uint64			uint64;
#endif

/* shorthand types */
typedef volatile int8			vint8;
typedef volatile uint8			vuint8;
typedef volatile int16			vint16;
typedef volatile uint16			vuint16;
typedef volatile int32			vint32;
typedef volatile uint32			vuint32;
typedef volatile int64			vint64;
typedef volatile uint64			vuint64;

typedef volatile long			vlong;
typedef volatile int			vint;
typedef volatile short			vshort;
typedef volatile char			vchar;

typedef volatile unsigned long	vulong;
typedef volatile unsigned int	vuint;
typedef volatile unsigned short	vushort;
typedef volatile unsigned char	vuchar;

typedef unsigned long			ulong;
typedef unsigned char			uchar;

/* Provide POSIX style uid_t/gid_t/mode_t when compiling on Windows
 * toolchains that don't provide them. Define `mode_t` only for MSVC
 * where it's missing; for MinGW/MSYS (non-MSVC) provide `uid_t`/`gid_t`
 * but avoid redefining `mode_t` which MinGW typically provides.
 */
#if defined(_MSC_VER)
#include <stdint.h>
typedef uint32_t uid_t;
typedef uint32_t gid_t;
typedef uint32_t mode_t;
#elif defined(_WIN32) && !defined(__CYGWIN__)
#include <stdint.h>
typedef uint32_t uid_t;
typedef uint32_t gid_t;
#endif

/* descriptive types */
typedef int32					status_t;
typedef int64					bigtime_t;
typedef int64					nanotime_t;
typedef uint32					type_code;
typedef uint32					perform_code;

/* Use the proper address type from types.h (handles Windows LLP64 vs POSIX LP64) */
typedef __haiku_addr_t			addr_t;
typedef __haiku_phys_addr_t		phys_addr_t;
typedef __haiku_phys_saddr_t	phys_saddr_t;
typedef __haiku_generic_addr_t	generic_addr_t;


/* printf()/scanf() format strings for [u]int* types */
#define B_PRId8			"d"
#define B_PRIi8			"i"
#define B_PRId16		"d"
#define B_PRIi16		"i"
#define B_PRId32		__HAIKU_PRI_PREFIX_32 "d"
#define B_PRIi32		__HAIKU_PRI_PREFIX_32 "i"
#define B_PRId64		__HAIKU_PRI_PREFIX_64 "d"
#define B_PRIi64		__HAIKU_PRI_PREFIX_64 "i"
#define B_PRIu8			"u"
#define B_PRIo8			"o"
#define B_PRIx8			"x"
#define B_PRIX8			"X"
#define B_PRIu16		"u"
#define B_PRIo16		"o"
#define B_PRIx16		"x"
#define B_PRIX16		"X"
#define B_PRIu32		__HAIKU_PRI_PREFIX_32 "u"
#define B_PRIo32		__HAIKU_PRI_PREFIX_32 "o"
#define B_PRIx32		__HAIKU_PRI_PREFIX_32 "x"
#define B_PRIX32		__HAIKU_PRI_PREFIX_32 "X"
#define B_PRIu64		__HAIKU_PRI_PREFIX_64 "u"
#define B_PRIo64		__HAIKU_PRI_PREFIX_64 "o"
#define B_PRIx64		__HAIKU_PRI_PREFIX_64 "x"
#define B_PRIX64		__HAIKU_PRI_PREFIX_64 "X"

#define B_SCNd8			"hhd"
#define B_SCNi8			"hhi"
#define B_SCNd16		"hd"
#define B_SCNi16		"hi"
#define B_SCNd32		__HAIKU_PRI_PREFIX_32 "d"
#define B_SCNi32		__HAIKU_PRI_PREFIX_32 "i"
#define B_SCNd64		__HAIKU_PRI_PREFIX_64 "d"
#define B_SCNi64		__HAIKU_PRI_PREFIX_64 "i"
#define B_SCNu8			"hhu"
#define B_SCNo8			"hho"
#define B_SCNx8			"hhx"
#define B_SCNu16		"hu"
#define B_SCNo16		"ho"
#define B_SCNx16		"hx"
#define B_SCNu32		__HAIKU_PRI_PREFIX_32 "u"
#define B_SCNo32		__HAIKU_PRI_PREFIX_32 "o"
#define B_SCNx32		__HAIKU_PRI_PREFIX_32 "x"
#define B_SCNu64		__HAIKU_PRI_PREFIX_64 "u"
#define B_SCNo64		__HAIKU_PRI_PREFIX_64 "o"
#define B_SCNx64		__HAIKU_PRI_PREFIX_64 "x"

/* printf()/scanf() format strings for some standard types */
/* size_t */
#define B_PRIuSIZE		__HAIKU_PRI_PREFIX_ADDR "u"
#define B_PRIoSIZE		__HAIKU_PRI_PREFIX_ADDR "o"
#define B_PRIxSIZE		__HAIKU_PRI_PREFIX_ADDR "x"
#define B_PRIXSIZE		__HAIKU_PRI_PREFIX_ADDR "X"

#define B_SCNuSIZE		__HAIKU_PRI_PREFIX_ADDR "u"
#define B_SCNoSIZE		__HAIKU_PRI_PREFIX_ADDR "o"
#define B_SCNxSIZE		__HAIKU_PRI_PREFIX_ADDR "x"

/* ssize_t */
#define B_PRIdSSIZE		__HAIKU_PRI_PREFIX_ADDR "d"
#define B_PRIiSSIZE		__HAIKU_PRI_PREFIX_ADDR "i"

#define B_SCNdSSIZE		__HAIKU_PRI_PREFIX_ADDR "d"
#define B_SCNiSSIZE		__HAIKU_PRI_PREFIX_ADDR "i"

/* addr_t */
#define B_PRIuADDR		__HAIKU_PRI_PREFIX_ADDR "u"
#define B_PRIoADDR		__HAIKU_PRI_PREFIX_ADDR "o"
#define B_PRIxADDR		__HAIKU_PRI_PREFIX_ADDR "x"
#define B_PRIXADDR		__HAIKU_PRI_PREFIX_ADDR "X"

#define B_SCNuADDR		__HAIKU_PRI_PREFIX_ADDR "u"
#define B_SCNoADDR		__HAIKU_PRI_PREFIX_ADDR "o"
#define B_SCNxADDR		__HAIKU_PRI_PREFIX_ADDR "x"

/* phys_addr_t */
#define B_PRIuPHYSADDR	__HAIKU_PRI_PREFIX_PHYS_ADDR "u"
#define B_PRIoPHYSADDR	__HAIKU_PRI_PREFIX_PHYS_ADDR "o"
#define B_PRIxPHYSADDR	__HAIKU_PRI_PREFIX_PHYS_ADDR "x"
#define B_PRIXPHYSADDR	__HAIKU_PRI_PREFIX_PHYS_ADDR "X"

#define B_SCNuPHYSADDR	__HAIKU_PRI_PREFIX_PHYS_ADDR "u"
#define B_SCNoPHYSADDR	__HAIKU_PRI_PREFIX_PHYS_ADDR "o"
#define B_SCNxPHYSADDR	__HAIKU_PRI_PREFIX_PHYS_ADDR "x"

/* generic_addr_t */
#define B_PRIuGENADDR	__HAIKU_PRI_PREFIX_GENERIC_ADDR "u"
#define B_PRIoGENADDR	__HAIKU_PRI_PREFIX_GENERIC_ADDR "o"
#define B_PRIxGENADDR	__HAIKU_PRI_PREFIX_GENERIC_ADDR "x"
#define B_PRIXGENADDR	__HAIKU_PRI_PREFIX_GENERIC_ADDR "X"

#define B_SCNuGENADDR	__HAIKU_PRI_PREFIX_GENERIC_ADDR "u"
#define B_SCNoGENADDR	__HAIKU_PRI_PREFIX_GENERIC_ADDR "o"
#define B_SCNxGENADDR	__HAIKU_PRI_PREFIX_GENERIC_ADDR "x"

/* off_t */
#define B_PRIdOFF		__HAIKU_PRI_PREFIX_OFF "d"
#define B_PRIiOFF		__HAIKU_PRI_PREFIX_OFF "i"
#define B_PRIxOFF		__HAIKU_PRI_PREFIX_OFF "x"

#define B_SCNdOFF		__HAIKU_PRI_PREFIX_OFF "d"
#define B_SCNiOFF		__HAIKU_PRI_PREFIX_OFF "i"
#define B_SCNxOFF		__HAIKU_PRI_PREFIX_OFF "x"

/* dev_t */
#if defined(_WIN32) || defined(__APPLE__)
#define B_PRIdDEV		B_PRId32
#define B_PRIiDEV		B_PRIi32
#else
#define B_PRIdDEV		B_PRId64
#define B_PRIiDEV		B_PRIi64
#endif

/* ino_t */
#if defined(_WIN32)
#define B_PRIdINO		B_PRId32
#define B_PRIiINO		B_PRIi32
#else
#define B_PRIdINO		__HAIKU_PRI_PREFIX_INO "d"
#define B_PRIiINO		__HAIKU_PRI_PREFIX_INO "i"
#endif

/* time_t */
#if defined(__i386__) && !defined(__x86_64__)
#	define B_PRIdTIME	B_PRId32
#	define B_PRIiTIME	B_PRIi32
#else
#	define B_PRIdTIME	B_PRId64
#	define B_PRIiTIME	B_PRIi64
#endif

/* bigtime_t */
#define B_PRIdBIGTIME	B_PRId64
#define B_PRIiBIGTIME	B_PRIi64


/* Printed width of a pointer with the %p format (minus 0x prefix). */
#ifdef B_HAIKU_64_BIT
#	define B_PRINTF_POINTER_WIDTH	16
#else
#	define B_PRINTF_POINTER_WIDTH	8
#endif


/* Empty string ("") */
#ifdef __cplusplus
extern const char *B_EMPTY_STRING;
#endif


/* min and max comparisons */
#ifndef __cplusplus
#	ifndef min
#		define min(a,b) ((a)>(b)?(b):(a))
#	endif
#	ifndef max
#		define max(a,b) ((a)>(b)?(a):(b))
#	endif
#endif

/* min() and max() are functions in C++ */
#define min_c(a,b) ((a)>(b)?(b):(a))
#define max_c(a,b) ((a)>(b)?(a):(b))

/* Uppercase MAX/MIN helpers used in legacy code */
#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif
#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

/* Math constants commonly missing on some Windows toolchains */
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif
#ifndef M_SQRT1_2
#define M_SQRT1_2 0.70710678118654752440
#endif

/* Provide gmtime_r and localtime_r for Windows toolchains that only expose gmtime_s/localtime_s. */
#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(_LOCALTIME_R_DEFINED)
#define _LOCALTIME_R_DEFINED
#include <time.h>
static inline struct tm*
gmtime_r(const time_t* timep, struct tm* result)
{
	if (result == NULL || timep == NULL)
		return NULL;
#if defined(_MSC_VER) || defined(__MINGW32__)
	/* gmtime_s(result, timep) returns 0 on success */
	return (gmtime_s(result, timep) == 0) ? result : NULL;
#else
	return NULL;
#endif
}

static inline struct tm*
localtime_r(const time_t* timep, struct tm* result)
{
	if (result == NULL || timep == NULL)
		return NULL;
#if defined(_MSC_VER) || defined(__MINGW32__)
	/* localtime_s(result, timep) returns 0 on success */
	return (localtime_s(result, timep) == 0) ? result : NULL;
#else
	return NULL;
#endif
}
#endif

/* Provide usleep for Windows toolchains that lack it. Avoid including
 * <windows.h> here to prevent pulling a large API surface (and C++
 * inline operators) into every translation unit. Forward-declare the
 * `Sleep()` function instead.
 */
#if defined(_WIN32) && !defined(__CYGWIN__)
/* Define useconds_t only for MSVC (MinGW provides it in sys/types.h). */
#if defined(_MSC_VER)
typedef unsigned long useconds_t;
/* Forward-declare Sleep only for MSVC; MinGW already declares it with
 * import attributes and warns on redeclaration without dllimport. */
#ifdef __cplusplus
extern "C" {
#endif
typedef unsigned long DWORD;
void __stdcall Sleep(DWORD dwMilliseconds);
#ifdef __cplusplus
}
#endif
static inline int
usleep(useconds_t usec)
{
	/* Sleep takes milliseconds; round up to avoid zero sleep for small usec */
	DWORD ms = (DWORD)((usec + 999) / 1000);
	Sleep(ms);
	return 0;
}
#endif
#endif


/* Grandfathering */
#ifndef __cplusplus
#	include <stdbool.h>
#endif

#ifndef NULL
#	define NULL (0)
#endif


#ifdef __cplusplus
extern "C" {
#endif

/* Other stuff */
extern void*	get_stack_frame(void);

#ifdef __cplusplus
}
#endif

/* Count items in an array, count_of is a common define */
#define B_COUNT_OF(a) (sizeof(a) / sizeof(a[0]))

/* Obsolete or discouraged API */

/* use 'true' and 'false' */
#ifndef FALSE
#	define FALSE	0
#endif
#ifndef TRUE
#	define TRUE		1
#endif


/* Use the built-in atomic functions, if requested and available. */

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7) || defined(__clang__)
#ifndef _ATOMIC_INLINE_FUNCS_DEFINED
#define _ATOMIC_INLINE_FUNCS_DEFINED

static __inline__ void
atomic_set(int32* value, int32 newValue)
{
	__atomic_store_n(value, newValue, __ATOMIC_RELEASE);
}


static __inline__ int32
atomic_get_and_set(int32* value, int32 newValue)
{
	return __atomic_exchange_n(value, newValue, __ATOMIC_SEQ_CST);
}


static __inline__ int32
atomic_test_and_set(int32* value, int32 newValue, int32 testAgainst)
{
	__atomic_compare_exchange_n(value, &testAgainst, newValue, 1,
		__ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
	return testAgainst;
}


static __inline__ int32
atomic_add(int32* value, int32 addValue)
{
	return __atomic_fetch_add(value, addValue, __ATOMIC_SEQ_CST);
}


static __inline__ int32
atomic_and(int32* value, int32 andValue)
{
	return __atomic_fetch_and(value, andValue, __ATOMIC_SEQ_CST);
}


static __inline__ int32
atomic_or(int32* value, int32 orValue)
{
	return __atomic_fetch_or(value, orValue, __ATOMIC_SEQ_CST);
}


static __inline__ int32
atomic_get(int32* value)
{
	return __atomic_load_n(value, __ATOMIC_ACQUIRE);
}


static __inline__ void
atomic_set64(int64* value, int64 newValue)
{
	__atomic_store_n(value, newValue, __ATOMIC_RELEASE);
}


static __inline__ int64
atomic_get_and_set64(int64* value, int64 newValue)
{
	return __atomic_exchange_n(value, newValue, __ATOMIC_SEQ_CST);
}


static __inline__ int64
atomic_test_and_set64(int64* value, int64 newValue, int64 testAgainst)
{
	__atomic_compare_exchange_n(value, &testAgainst, newValue, 1,
		__ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
	return testAgainst;
}


static __inline__ int64
atomic_add64(int64* value, int64 addValue)
{
	return __atomic_fetch_add(value, addValue, __ATOMIC_SEQ_CST);
}


static __inline__ int64
atomic_and64(int64* value, int64 andValue)
{
	return __atomic_fetch_and(value, andValue, __ATOMIC_SEQ_CST);
}


static __inline__ int64
atomic_or64(int64* value, int64 orValue)
{
	return __atomic_fetch_or(value, orValue, __ATOMIC_SEQ_CST);
}


static __inline__ int64
atomic_get64(int64* value)
{
	return __atomic_load_n(value, __ATOMIC_ACQUIRE);
}

#endif /* _ATOMIC_INLINE_FUNCS_DEFINED */
#else	/* __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7) */

#ifdef __cplusplus
extern "C" {
#endif

/* Atomic functions; previous value is returned */
extern void		atomic_set(int32* value, int32 newValue);
extern int32	atomic_get_and_set(int32* value, int32 newValue);
extern int32	atomic_test_and_set(int32 *value, int32 newValue, int32 testAgainst);
extern int32	atomic_add(int32 *value, int32 addValue);
extern int32	atomic_and(int32 *value, int32 andValue);
extern int32	atomic_or(int32 *value, int32 orValue);
extern int32	atomic_get(int32 *value);

extern void		atomic_set64(int64* value, int64 newValue);
extern int64	atomic_get_and_set64(int64* value, int64 newValue);
extern int64	atomic_test_and_set64(int64 *value, int64 newValue, int64 testAgainst);
extern int64	atomic_add64(int64 *value, int64 addValue);
extern int64	atomic_and64(int64 *value, int64 andValue);
extern int64	atomic_or64(int64 *value, int64 orValue);
extern int64	atomic_get64(int64 *value);

#ifdef __cplusplus
}
#endif

#endif


#endif	/* _SUPPORT_DEFS_H */
