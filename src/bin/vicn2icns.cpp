/*
 * Copyright 2026, Cosmoe Project
 * Distributed under the terms of the MIT License.
 *
 * vicn2icns - Convert VICN (vector icon) resources to macOS .icns format
 *
 * This tool reads the first VICN resource from a .rsrc file, renders it at multiple
 * sizes as PNGs, and uses Apple's iconutil to create a .icns file for macOS apps.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <png.h>

#include <Bitmap.h>
#include <File.h>
#include <Resources.h>
#include <IconUtils.h>

// Forward declare interface kit init function
extern "C" status_t _init_interface_kit_();

// Standard macOS icon sizes for .icns
struct IconSize {
	const char* name;
	int size;
};

static const IconSize kIconSizes[] = {
	{ "icon_16x16", 16 },
	{ "icon_16x16@2x", 32 },
	{ "icon_32x32", 32 },
	{ "icon_32x32@2x", 64 },
	{ "icon_128x128", 128 },
	{ "icon_128x128@2x", 256 },
	{ "icon_256x256", 256 },
	{ "icon_256x256@2x", 512 },
	{ "icon_512x512", 512 },
	{ "icon_512x512@2x", 1024 },
	{ NULL, 0 }
};


static void
print_usage(const char* program)
{
	fprintf(stderr, "Usage: %s <input.rsrc> <output.icns>\n", program);
	fprintf(stderr, "\n");
	fprintf(stderr, "Converts a VICN (vector icon) resource to macOS .icns format.\n");
	fprintf(stderr, "The input .rsrc file must contain at least one VICN resource.\n");
	fprintf(stderr, "\n");
}


static status_t
save_bitmap_as_png(BBitmap* bitmap, const char* path)
{
	FILE* fp = fopen(path, "wb");
	if (!fp) {
		fprintf(stderr, "Error: Failed to create output file '%s': %s\n",
			path, strerror(errno));
		return B_ERROR;
	}

	png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png) {
		fclose(fp);
		return B_NO_MEMORY;
	}

	png_infop info = png_create_info_struct(png);
	if (!info) {
		png_destroy_write_struct(&png, NULL);
		fclose(fp);
		return B_NO_MEMORY;
	}

	if (setjmp(png_jmpbuf(png))) {
		png_destroy_write_struct(&png, &info);
		fclose(fp);
		return B_ERROR;
	}

	png_init_io(png, fp);

	// Get bitmap dimensions
	uint32 width = bitmap->Bounds().IntegerWidth() + 1;
	uint32 height = bitmap->Bounds().IntegerHeight() + 1;

	// Write PNG header (RGBA, 8-bit per channel)
	png_set_IHDR(png, info, width, height, 8,
		PNG_COLOR_TYPE_RGBA,
		PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT,
		PNG_FILTER_TYPE_DEFAULT);

	png_write_info(png, info);

	// BBitmap B_RGBA32 is stored as BGRA in memory (little-endian)
	// PNG expects RGBA, so we need to swap B and R channels
	png_set_bgr(png);

	// Write image data row by row
	uint8* bits = (uint8*)bitmap->Bits();
	uint32 bpr = bitmap->BytesPerRow();
	
	for (uint32 y = 0; y < height; y++) {
		png_write_row(png, bits + (y * bpr));
	}

	png_write_end(png, NULL);
	png_destroy_write_struct(&png, &info);
	fclose(fp);

	return B_OK;
}


static status_t
render_icon_sizes(const uint8* vectorData, size_t dataSize, const char* iconsetPath)
{
	status_t result;
	
	for (int i = 0; kIconSizes[i].name != NULL; i++) {
		const IconSize& iconSize = kIconSizes[i];
		
		// Create bitmap at this size
		BRect bounds(0, 0, iconSize.size - 1, iconSize.size - 1);
		BBitmap* bitmap = new BBitmap(bounds, B_BITMAP_NO_SERVER_LINK, B_RGBA32);
		if (bitmap == NULL || bitmap->InitCheck() != B_OK) {
			fprintf(stderr, "Error: Failed to create bitmap for size %dx%d: %s\n",
				iconSize.size, iconSize.size, strerror(bitmap ? bitmap->InitCheck() : B_NO_MEMORY));
			delete bitmap;
			return B_NO_MEMORY;
		}

		// Render vector icon to bitmap
		result = BIconUtils::GetVectorIcon(vectorData, dataSize, bitmap);
		if (result != B_OK) {
			fprintf(stderr, "Error: Failed to render icon at size %dx%d: %s\n",
				iconSize.size, iconSize.size, strerror(result));
			delete bitmap;
			return result;
		}

		// Save as PNG
		char pngPath[1024];
		snprintf(pngPath, sizeof(pngPath), "%s/%s.png",
			iconsetPath, iconSize.name);
		
		result = save_bitmap_as_png(bitmap, pngPath);
		delete bitmap;
		
		if (result != B_OK)
			return result;
	}
	
	return B_OK;
}


static status_t
create_icns_from_iconset(const char* iconsetPath, const char* icnsPath)
{
	// Use iconutil to convert iconset to icns
	char command[2048];
	snprintf(command, sizeof(command),
		"iconutil -c icns -o \"%s\" \"%s\"",
		icnsPath, iconsetPath);
	
	int result = system(command);
	if (result != 0) {
		fprintf(stderr, "Error: iconutil command failed with code %d\n", result);
		return B_ERROR;
	}
	
	return B_OK;
}


int
main(int argc, char** argv)
{
	if (argc != 3) {
		print_usage(argv[0]);
		return 1;
	}

	const char* inputPath = argv[1];
	const char* outputPath = argv[2];

	// Open the resource file
	BFile file(inputPath, B_READ_ONLY);
	status_t result = file.InitCheck();
	if (result != B_OK) {
		fprintf(stderr, "Error: Failed to open input file '%s': %s\n",
			inputPath, strerror(result));
		return 1;
	}

	BResources resources;
	result = resources.SetTo(&file);
	if (result != B_OK) {
		fprintf(stderr, "Error: Failed to read resources from '%s': %s\n",
			inputPath, strerror(result));
		return 1;
	}

	// Load the first VICN resource
	size_t dataSize;
	const void* data = resources.LoadResource(B_VECTOR_ICON_TYPE, (int32)101, &dataSize);
	if (data == NULL) {
		// Try ID 0
		data = resources.LoadResource(B_VECTOR_ICON_TYPE, (int32)0, &dataSize);
	}
	if (data == NULL) {
		// Try by name if index didn't work
		data = resources.LoadResource(B_VECTOR_ICON_TYPE, "BEOS:ICON", &dataSize);
	}
	
	if (data == NULL || dataSize == 0) {
		fprintf(stderr, "Error: No VICN resource found in '%s'\n", inputPath);
		return 1;
	}
	
	// Create temporary iconset directory
	char iconsetPath[1024];
	snprintf(iconsetPath, sizeof(iconsetPath), "/tmp/vicn2icns_%d.iconset", getpid());
	
	if (mkdir(iconsetPath, 0755) != 0) {
		fprintf(stderr, "Error: Failed to create iconset directory '%s'\n", iconsetPath);
		return 1;
	}

	// Render icon at all required sizes
	result = render_icon_sizes((const uint8*)data, dataSize, iconsetPath);
	if (result != B_OK) {
		// Clean up iconset directory
		char rmCommand[1024];
		snprintf(rmCommand, sizeof(rmCommand), "rm -rf \"%s\"", iconsetPath);
		system(rmCommand);
		return 1;
	}

	// Convert iconset to icns using iconutil
	result = create_icns_from_iconset(iconsetPath, outputPath);
	
	// Clean up iconset directory
	char rmCommand[1024];
	snprintf(rmCommand, sizeof(rmCommand), "rm -rf \"%s\"", iconsetPath);
	system(rmCommand);

	if (result != B_OK) {
		return 1;
	}

	printf("Successfully created '%s' from VICN resource in '%s'\n",
		outputPath, inputPath);
	
	return 0;
}
