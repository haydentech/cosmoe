//------------------------------------------------------------------------------
//	Copyright (c) 2004, Bill Hayden
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
//	File Name:		real_time_clock.c
//	Author:			Bill Hayden (hayden@haydentech.com)
//	Description:	Implement clock functions.
//------------------------------------------------------------------------------

#include <OS.h>
#include <sys/time.h>

void
set_real_time_clock(unsigned long currentTime)
{
	struct timeval tv;

	tv.tv_sec = (long)currentTime;
	tv.tv_usec = 0;

	settimeofday(&tv, NULL);
}


unsigned long
real_time_clock(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);

	// Handles possible tv_usec overflow (i.e. > 1 second)
	return tv.tv_sec + tv.tv_usec / 1000000;
}


bigtime_t
real_time_clock_usecs(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);

	return (bigtime_t)tv.tv_sec * 1000000LL + tv.tv_usec;
}
