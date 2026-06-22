#include "DisplayScaleManager.h"
#include <Window.h>
#include <Application.h>
#include <CosmoeBackendAPI.h>

static int32 sLastScalePercent = 100;

int32 
BDisplayScaleManager::GetScaleForWindow(BWindow *window)
{
	if (!window)
		return DefaultScale();
	
	int32_t token = window->WindowToken();
	if (token == B_NULL_TOKEN)
		return DefaultScale();
	
	// Delegate to the backend's platform-specific scale detection
	int32 scale = cosmoe_window_get_display_scale(be_app->Display(), token);
	if (scale >= 100)
		sLastScalePercent = scale;

	return scale;
}


int32
BDisplayScaleManager::DefaultScale()
{
	return sLastScalePercent;
}
