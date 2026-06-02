#include "DisplayScaleManager.h"
#include <Window.h>
#include <Application.h>
#include <CosmoeBackendAPI.h>

float 
BDisplayScaleManager::GetScaleForWindow(BWindow *window)
{
	if (!window)
		return 1.0f;
	
	int32_t token = window->WindowToken();
	if (token == B_NULL_TOKEN)
		return 1.0f;
	
	// Delegate to the backend's platform-specific scale detection
	int32 rawScale = cosmoe_window_get_display_scale(be_app->Display(), token);
	if (rawScale < 1)
		return 1.0f;

	// Windows backend reports percentage (e.g. 125), others report integer factors (1, 2, ...).
	if (rawScale > 10)
		return (float)rawScale / 100.0f;

	return (float)rawScale;
}
