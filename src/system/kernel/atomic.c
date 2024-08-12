//------------------------------------------------------------------------------
//	Copyright (c) 2004-2024, Bill Hayden
//
//	Permission is hereby granted, free of charge, to any person obtaining a
//	copy of this software and associated documentation files (the "Software"),
//	to deal in the Software without restriction, including without limitation
//	the rights to use, copy, modify, merge, publish, distribute, sublicense,
//	and/or sell copies of the Software, and to permit persons to whom the
//	Software is furnished to do so, subject to the following conditions:
//
//	The above copyright notice and this permission notice shall be included in
//	all copies or substantial portions of the Software.
//
//	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//	FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//	DEALINGS IN THE SOFTWARE.
//
//	File Name:		atomic.c
//	Author:			Bill Hayden (hayden@haydentech.com)
//	Description:	atomic functions.
//------------------------------------------------------------------------------

#include <SupportDefs.h>

#define __gcc_noalias__(x) (*(volatile struct { int value; } *)x)



static inline int32 atomic_exchange(vint32 *value, int32 oldval, int32 newval)
{
#ifdef __i386__
	__asm__ __volatile__(
		"lock; cmpxchgl %1, %2"
			: "+a" (oldval)
			: "r" (newval), "m" (__gcc_noalias__(value))
			: "memory");

	return oldval;
#else
	return __sync_val_compare_and_swap(value, oldval, newval);
#endif
}


int32 atomic_add(vint32 *value, int32 addvalue)
{
	register int32 oldval;

	do {
		oldval = *value;
	} while (atomic_exchange(value, oldval, oldval + addvalue) != oldval);
     
	return oldval;
}


int32 atomic_or(vint32 *value, int32 orvalue)
{
	register int32 oldval;

	do {
		oldval = *value;
	} while (atomic_exchange(value, oldval, oldval | orvalue) != oldval);
     
	return oldval;
}


int32 atomic_and(vint32 *value, int32 andvalue)
{
	register int32 oldval;

	do {
		oldval = *value;
	} while (atomic_exchange(value, oldval, oldval & andvalue) != oldval);
     
	return oldval;
}


int32 atomic_get(vint32 *value)
{
	return *value;
}


int32
atomic_set(vint32 *value, int32 newValue)
{
	register int32 oldval;

	do {
		oldval = *value;
	} while (atomic_exchange(value, oldval, newValue) != oldval);
     
	return oldval;
}


int32
atomic_test_and_set(vint32 *value, int32 newValue, int32 testAgainst)
{
	int32 oldValue = *value;
	if (oldValue == testAgainst)
		*value = newValue;
	return oldValue;
}
