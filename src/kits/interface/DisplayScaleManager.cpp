#include "DisplayScaleManager.h"
#include <Window.h>
#include <CosmoeBackendAPI.h>

int32 
BDisplayScaleManager::GetScaleForWindow(BWindow *window)
{
	if (!window)
		return 1;
	
	cosmoe_window_t backend_window = window->BackendWindow();
	if (!backend_window)
		return 1;
	
	// Delegate to the backend's platform-specific scale detection
	return cosmoe_window_get_display_scale(backend_window);
}
