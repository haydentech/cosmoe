#include <stdio.h>
#include <string>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>

#include <Directory.h>

int main(void)
{
	size_t bufSize = (sizeof(dirent) + B_FILE_NAME_LENGTH) * 10;
	char buffer[bufSize];
	dirent *ents = (dirent *)buffer;
	BDirectory dir("/cosmoe/test");

	printf("- Iterating over directory\n");
	while (dir.GetNextDirents(ents, bufSize, 1) == 1) {
		printf("    Found an entry: %s\n", ents->d_name);
	}

	printf("- Rewinding directory\n");
	status_t err = dir.Rewind();

	if (err != B_OK) {
		printf("Rewind failed: %s\n", strerror(err));
	}

	printf("- Iterating over directory again\n");
	while (dir.GetNextDirents(ents, bufSize, 1) == 1) {
		printf("%s\n", ents->d_name);
	}

	printf("- Counting directory entries\n");
	printf("    found %d\n", dir.CountEntries());
}