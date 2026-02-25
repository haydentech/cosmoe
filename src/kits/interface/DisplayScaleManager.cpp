#include "DisplayScaleManager.h"
#include <Window.h>
#include <Application.h>
#include <CosmoeBackendAPI.h>

int32 
BDisplayScaleManager::GetScaleForWindow(BWindow *window)
{
	if (!window)
		return 1;
	
	int32_t token = window->WindowToken();
	if (token == B_NULL_TOKEN)
		return 1;
	
	// Delegate to the backend's platform-specific scale detection
	return cosmoe_window_get_display_scale(be_app->Display(), token);
}
