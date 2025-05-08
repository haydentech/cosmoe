#include "disapp.h"
#include <Path.h>
#include <VolumeRoster.h>
#include <Directory.h>



DisApplication::DisApplication()
	: BApplication ("application/x-vnd.minimal-noui")
{
	BVolumeRoster roster;
	BVolume volume;
	char name[256];

	printf("- Iterating over volumes\n");
	while (roster.GetNextVolume(&volume) == B_OK) {
		volume.GetName(name);
		printf("    Found a volume: %s\n", name);
		BDirectory dir;
		status_t err = volume.GetRootDirectory(&dir);
		if (err == B_OK)
		{
			BEntry entry;
			char dirname[B_FILE_NAME_LENGTH];
			dir.GetEntry(&entry);
			entry.GetName(dirname);
			printf("        at path: %s\n", dirname);
		}
		else
			printf("        but GetRootDirectory returned %d\n", err);
	}

	printf("\n- Rewinding directory\n\n");
	roster.Rewind();

	printf("- Iterating over volumes again\n");
	while (roster.GetNextVolume(&volume) == B_OK) {
		volume.GetName(name);
		printf("    Found a volume: %s\n", name);
		BDirectory dir;
		status_t err = volume.GetRootDirectory(&dir);
		if (err == B_OK)
		{
			BEntry entry;
			char dirname[B_FILE_NAME_LENGTH];
			dir.GetEntry(&entry);
			entry.GetName(dirname);
			printf("        at path: %s\n", dirname);
		}
		else
			printf("        but GetRootDirectory returned %d\n", err);
	}

	printf("- Looking for boot volume\n");
	status_t err = roster.GetBootVolume(&volume);
	char dirname[B_FILE_NAME_LENGTH];
	BDirectory dir;
	BEntry entry;
	dir.GetEntry(&entry);
	entry.GetName(dirname);
	if (err == B_OK)
	{
		volume.GetName(name);
		printf("    Found boot volume: %s\n", name);
	}
	else
		printf("        but GetBootVolume returned %d\n", err);

	PostMessage(new BMessage(B_QUIT_REQUESTED));
}
