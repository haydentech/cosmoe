#ifndef _DISPLAY_SCALE_MANAGER_H
#define _DISPLAY_SCALE_MANAGER_H

#include <SupportDefs.h>

class BWindow;

class BDisplayScaleManager {
public:
	static int32 GetScaleForWindow(BWindow *window);
};

#endif // _DISPLAY_SCALE_MANAGER_H
