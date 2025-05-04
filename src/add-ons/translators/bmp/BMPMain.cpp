/*****************************************************************************/
// BMPTranslator
//
// Version: 1.0.0 Beta
//
// This translator opens and writes BMP files.
//
//
// This application and all source files used in its construction, except 
// where noted, are licensed under the MIT License, and have been written 
// and are:
//
// Copyright (c) 2002 Haiku Project
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense, 
// and/or sell copies of the Software, and to permit persons to whom the 
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included 
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
/*****************************************************************************/

#include <Application.h>
#include <Catalog.h>

#include "BMPTranslator.h"
#include "TranslatorWindow.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "BMPMain"



#include <stdio.h>
#include <stdlib.h>

// Used to make this simultaneously an executable and a shared library
#ifdef __x86_64__
const char service_interp[] __attribute__((section(".interp"))) = "/lib/ld-linux-x86-64.so.2";
#elif __aarch64__
const char service_interp[] __attribute__((section(".interp"))) = "/lib/ld-linux-aarch64.so.1";
#else
#error "Unsupported architecture - add the appropriate path for your platform"
#endif


// See https://stackoverflow.com/questions/1449987/building-a-so-that-is-also-an-executable/68339111#68339111
#ifdef __GLIBC__
/* magic to make glibc work more reliably. */
extern "C" {
extern int _IO_stdin_used;
int _IO_stdin_used __attribute__((weak)) = 131073;
}
#endif


// ---------------------------------------------------------------
// main
//
// Creates a BWindow for displaying info about the BMPTranslator
//
// Preconditions:
//
// Parameters:
//
// Postconditions:
//
// Returns:
// ---------------------------------------------------------------
int
main()
{
	BApplication app("application/x-vnd.Haiku-BMPTranslator");
	status_t result;

	result = LaunchTranslatorWindow(new BMPTranslator,
		B_TRANSLATE("BMP Settings"), BRect(0, 0, 225, 175));
	if (result == B_OK) {
		app.Run();
		return 0;
	} else
		return 1;
}
