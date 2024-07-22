#include <stdio.h>
#include <string>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>

#include <Directory.h>
#include <Entry.h>
#include <Path.h>

int main(void)
{
	BDirectory dir("/cosmoe/test");
	BEntry entry;

	printf("- Iterating over directory\n");
	while (dir.GetNextEntry(&entry) == B_OK) {
		printf("    Found an Entry: %s\n", entry.Name());
		BPath path;
		status_t err = entry.GetPath(&path);
		if (err == B_OK)
			printf("        at path: %s\n", path.Path());
		else
			printf("        but GetPath returned %d\n", err);
	}

	printf("\n- Rewinding directory\n\n");
	status_t err = dir.Rewind();

	if (err != B_OK) {
		printf("Rewind failed: %s\n", strerror(err));
	}

	printf("- Iterating over directory again\n");
	while (dir.GetNextEntry(&entry) == B_OK) {
		printf("    Found an Entry: %s\n", entry.Name());
		BPath path;
		status_t err = entry.GetPath(&path);
		if (err == B_OK)
			printf("        at path: %s\n", path.Path());
		else
			printf("        but GetPath returned %d\n", err);
	}

	printf("- Counting directory entries\n");
	printf("    found %d\n", dir.CountEntries());

	printf("\n- Rewinding directory\n\n");
	err = dir.Rewind();

	if (err != B_OK) {
		printf("Rewind failed: %s\n", strerror(err));
	}

	printf("- Iterating over directory with entry_refs\n");
	entry_ref ref;
	while (dir.GetNextRef(&ref) == B_OK) {
		printf("    Found an entry_ref: %s\n", ref.name);
		printf("        at path: %s\n", ref.dirpath);
	}

}