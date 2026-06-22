#ifndef _DISPLAY_SCALE_MANAGER_H
#define _DISPLAY_SCALE_MANAGER_H

#include <SupportDefs.h>

class BWindow;

class BDisplayScaleManager {
public:
	// Returns display scale in percent (100, 200, 300, etc.).
	static int32 GetScaleForWindow(BWindow *window);
	static int32 DefaultScale();
};

#endif // _DISPLAY_SCALE_MANAGER_H
