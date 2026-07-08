#include "MiniTrackerApp.h"

#include <MacOSCompatibility.h>

int
main(int argc, char** argv)
{
	const char* startupPath = argc > 1 ? argv[1] : NULL;
	MiniTrackerApp app(startupPath);
	app.Run();
	return 0;
}