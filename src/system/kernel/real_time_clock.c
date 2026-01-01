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

#if defined(_WIN32) && !defined(__CYGWIN__)
#include <windows.h>
#endif

void
set_real_time_clock(unsigned long currentTime)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
	/* Convert to UTC SYSTEMTIME and set system time via Win32 API. */
	time_t t = (time_t)currentTime;
	struct tm tm_buf;
	struct tm *tm_ptr = NULL;
#if defined(_MSC_VER)
	if (gmtime_s(&tm_buf, &t) == 0)
		tm_ptr = &tm_buf;
#else
	/* On MinGW/ucrt, gmtime_r may be missing; fall back to gmtime(). */
#if defined(__MINGW32__) || defined(__MINGW64__)
	/* Try gmtime_r if available, otherwise use gmtime(). */
#ifdef HAVE_GMTIME_R
	tm_ptr = gmtime_r(&t, &tm_buf);
#else
	{
		struct tm* tmp = gmtime(&t);
		if (tmp) {
			tm_buf = *tmp;
			tm_ptr = &tm_buf;
		}
	}
#endif
#else
	/* Generic fallback for other compilers: use gmtime() (not thread-safe)
	   but copy result into tm_buf. */
	{
		struct tm* tmp = gmtime(&t);
		if (tmp) {
			tm_buf = *tmp;
			tm_ptr = &tm_buf;
		}
	}
#endif
#endif
	if (tm_ptr != NULL) {
		SYSTEMTIME st;
		st.wYear = (WORD)(tm_ptr->tm_year + 1900);
		st.wMonth = (WORD)(tm_ptr->tm_mon + 1);
		st.wDay = (WORD)tm_ptr->tm_mday;
		st.wHour = (WORD)tm_ptr->tm_hour;
		st.wMinute = (WORD)tm_ptr->tm_min;
		st.wSecond = (WORD)tm_ptr->tm_sec;
		st.wMilliseconds = 0;
		/* SetSystemTime expects UTC values */
		SetSystemTime(&st);
	}
#else
	struct timeval tv;

	tv.tv_sec = (long)currentTime;
	tv.tv_usec = 0;

	settimeofday(&tv, NULL);
#endif
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
