/*
 * Copyright 2007 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _DEBUG_H
#define _DEBUG_H


#include <BeBuild.h>
#include <OS.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>



	#define SET_DEBUG_ENABLED(FLAG)			(void)0
	#define	IS_DEBUG_ENABLED()				(void)0
	
	#define SERIAL_PRINT(ARGS)				(void)0
	#define PRINT(ARGS)						(void)0
	#define PRINT_OBJECT(OBJ)				(void)0
	#define TRACE()							(void)0
	#define SERIAL_TRACE()					(void)0

	#define DEBUGGER(MSG)					(void)0
	#if !defined(ASSERT)
		#define ASSERT(E)					(void)0
	#endif
	#define ASSERT_WITH_MESSAGE(expr, msg)	(void)0
	#define TRESPASS()						(void)0
	#define DEBUG_ONLY(x)

/* STATIC_ASSERT is a compile-time check that can be used to             */
/* verify static expressions such as: STATIC_ASSERT(sizeof(int64) == 8); */
#define STATIC_ASSERT(x)								\
	do {												\
		struct __staticAssertStruct__ {					\
			char __static_assert_failed__[2*(x) - 1];	\
		};												\
	} while (false)


#endif	/* _DEBUG_H */
