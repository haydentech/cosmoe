/*
 * Compatibility hooks for Tracker code paths that expect Haiku desktop
 * services not yet implemented by Cosmoe's kits.
 */

#include "MountMenu.h"

#include <InterfaceDefs.h>


bigtime_t
idle_time()
{
	return B_INFINITE_TIMEOUT;
}


status_t
get_deskbar_frame(BRect* frame)
{
	if (frame != NULL)
		*frame = BRect();
	return B_ERROR;
}


void
run_add_printer_panel()
{
}


namespace BPrivate {

MountMenu::MountMenu(const char* name)
	:
	BMenu(name)
{
}


bool
MountMenu::AddDynamicItem(add_state)
{
	return false;
}

} // namespace BPrivate
