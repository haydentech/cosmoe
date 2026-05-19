#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <Directory.h>
#include <File.h>
#include <Path.h>
#include <Resources.h>

#ifdef _WIN32
static const char* __progname = "mime_db_builder";
#else
extern const char* __progname;
#endif

static int
usage(int status)
{
	FILE* stream = status == 0 ? stdout : stderr;
	fprintf(stream, "Usage: %s [--directory] <input.rsrc> <output-path>\n",
		__progname);
	return status;
}

int
main(int argc, const char** argv)
{
	bool createDirectory = false;
	int argi = 1;
	if (argc > 1 && strcmp(argv[1], "--directory") == 0) {
		createDirectory = true;
		argi++;
	}

	if (argc - argi != 2)
		return usage(1);

	const char* inputPath = argv[argi++];
	const char* outputPath = argv[argi++];

	BResources resources;
	status_t status = resources.SetTo(inputPath, false);
	if (status != B_OK) {
		fprintf(stderr, "%s: failed to open resources \"%s\": %s\n",
			__progname, inputPath, strerror(status));
		return 1;
	}

	BFile outputFile;
	BDirectory outputDirectory;
	BNode* outputNode = NULL;
	if (createDirectory) {
		status = create_directory(outputPath, 0755);
		if (status != B_OK && status != B_FILE_EXISTS) {
			fprintf(stderr, "%s: failed to create directory \"%s\": %s\n",
				__progname, outputPath, strerror(status));
			return 1;
		}

		status = outputDirectory.SetTo(outputPath);
		if (status != B_OK) {
			fprintf(stderr, "%s: failed to open directory \"%s\": %s\n",
				__progname, outputPath, strerror(status));
			return 1;
		}
		outputNode = &outputDirectory;
	} else {
		outputFile.SetTo(outputPath,
			B_READ_WRITE | B_CREATE_FILE | B_ERASE_FILE);
		status = outputFile.InitCheck();
		if (status != B_OK) {
			fprintf(stderr, "%s: failed to create \"%s\": %s\n",
				__progname, outputPath, strerror(status));
			return 1;
		}
		outputNode = &outputFile;
	}

	for (int32 index = 0;; index++) {
		type_code type;
		int32 id;
		const char* name;
		size_t size;
		if (!resources.GetResourceInfo(index, &type, &id, &name, &size))
			break;

		if (name == NULL || name[0] == '\0')
			continue;

		size_t resourceSize;
		const void* data = resources.LoadResource(type, id, &resourceSize);
		if (data == NULL) {
			fprintf(stderr, "%s: failed to load resource %ld from \"%s\"\n",
				__progname, (long)id, inputPath);
			return 1;
		}

		outputNode->RemoveAttr(name);
		ssize_t bytesWritten = outputNode->WriteAttr(name, type, 0, data,
			resourceSize);
		if (bytesWritten < 0) {
			fprintf(stderr, "%s: failed to write attribute \"%s\" to \"%s\": %s\n",
				__progname, name, outputPath, strerror((int)bytesWritten));
			return 1;
		}
		if ((size_t)bytesWritten != resourceSize) {
			fprintf(stderr, "%s: short write for attribute \"%s\" to \"%s\"\n",
				__progname, name, outputPath);
			return 1;
		}
	}

	mode_t mode = createDirectory ? 0755 : 0644;
	if (chmod(outputPath, mode) != 0) {
		fprintf(stderr, "%s: failed to chmod \"%s\": %s\n", __progname,
			outputPath, strerror(errno));
		return 1;
	}

	return 0;
}
