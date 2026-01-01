#include "disapp.h"
#include "diswindow.h"
#include "disview.h"

#ifdef _WIN32
#include <windows.h>
#include <string>
#define DEBUG_MSG(msg) { \
MessageBoxA(NULL, msg, "Minimal Debug", MB_OK | MB_TOPMOST | MB_SETFOREGROUND); \
}
#else
#include <stdio.h>
#define DEBUG_MSG(msg) printf("%s\n", msg); fflush(stdout);
#endif


DisApplication::DisApplication()
        : BApplication ("application/x-vnd.minimal")
{
DEBUG_MSG("DisApplication: Constructor called");

        DisWindow *window;
        BRect rect(30, 100, 330, 160);

DEBUG_MSG("DisApplication: About to create window");

        window = new DisWindow(rect);

DEBUG_MSG("DisApplication: Window created, about to show");

        window->Show();

DEBUG_MSG("DisApplication: Window->Show() called");
}
