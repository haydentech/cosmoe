/*
 * Copyright 2026, Cosmoe Project
 * Distributed under the terms of the MIT License.
 *
 * vicn2png - Convert VICN (vector icon) resources to PNG icons
 *
 * This tool reads the first VICN resource from a .rsrc file and renders it at
 * multiple sizes as PNG files suitable for freedesktop.org icon themes (used
 * by both X11 and Wayland).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#include <png.h>

#include <Bitmap.h>
#include <File.h>
#include <Resources.h>
#include <IconUtils.h>

// Standard freedesktop.org icon sizes (for X11 and Wayland)
// https://specifications.freedesktop.org/icon-theme-spec/icon-theme-spec-latest.html
static const int kIconSizes[] = {
	16, 22, 24, 32, 48, 64, 128, 256, 512, 0
};


static void
print_usage(const char* program)
{
	fprintf(stderr, "Usage: %s <input.rsrc> <output-directory> <app-name>\n", program);
	fprintf(stderr, "\n");
	fprintf(stderr, "Converts a VICN (vector icon) resource to PNG icons for freedesktop systems.\n");
	fprintf(stderr, "The input .rsrc file must contain at least one VICN resource.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Output files will be created as:\n");
	fprintf(stderr, "  <output-directory>/hicolor/{size}x{size}/apps/<app-name>.png\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Example:\n");
	fprintf(stderr, "  %s Icon-O-Matic.rsrc /usr/share/icons icon-o-matic\n", program);
	fprintf(stderr, "  Creates: /usr/share/icons/hicolor/48x48/apps/icon-o-matic.png\n");
	fprintf(stderr, "           /usr/share/icons/hicolor/256x256/apps/icon-o-matic.png\n");
	fprintf(stderr, "           etc.\n");
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
create_directory_recursive(const char* path)
{
	char tmp[1024];
	char* p = NULL;
	size_t len;

	snprintf(tmp, sizeof(tmp), "%s", path);
	len = strlen(tmp);
	if (tmp[len - 1] == '/')
		tmp[len - 1] = 0;

	for (p = tmp + 1; *p; p++) {
		if (*p == '/') {
			*p = 0;
			if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
				fprintf(stderr, "Error: Failed to create directory '%s': %s\n",
					tmp, strerror(errno));
				return B_ERROR;
			}
			*p = '/';
		}
	}
	
	if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
		fprintf(stderr, "Error: Failed to create directory '%s': %s\n",
			tmp, strerror(errno));
		return B_ERROR;
	}
	
	return B_OK;
}


static status_t
render_icon_sizes(const uint8* vectorData, size_t dataSize,
	const char* outputDir, const char* appName)
{
	status_t result;
	
	for (int i = 0; kIconSizes[i] != 0; i++) {
		int size = kIconSizes[i];
		
		// Create bitmap at this size
		BRect bounds(0, 0, size - 1, size - 1);
		BBitmap* bitmap = new BBitmap(bounds, B_BITMAP_NO_SERVER_LINK, B_RGBA32);
		if (bitmap == NULL || bitmap->InitCheck() != B_OK) {
			fprintf(stderr, "Error: Failed to create bitmap for size %dx%d: %s\n",
				size, size, strerror(bitmap ? bitmap->InitCheck() : B_NO_MEMORY));
			delete bitmap;
			return B_NO_MEMORY;
		}

		// Render vector icon to bitmap
		result = BIconUtils::GetVectorIcon(vectorData, dataSize, bitmap);
		if (result != B_OK) {
			fprintf(stderr, "Error: Failed to render icon at size %dx%d: %s\n",
				size, size, strerror(result));
			delete bitmap;
			return result;
		}

		// Create output directory structure
		char dirPath[1024];
		snprintf(dirPath, sizeof(dirPath), "%s/hicolor/%dx%d/apps",
			outputDir, size, size);
		
		result = create_directory_recursive(dirPath);
		if (result != B_OK) {
			delete bitmap;
			return result;
		}

		// Save as PNG
		char pngPath[1024];
		snprintf(pngPath, sizeof(pngPath), "%s/%s.png", dirPath, appName);
		
		result = save_bitmap_as_png(bitmap, pngPath);
		delete bitmap;
		
		if (result != B_OK)
			return result;
		
		printf("Created: %s\n", pngPath);
	}
	
	return B_OK;
}


int
main(int argc, char** argv)
{
	if (argc != 4) {
		print_usage(argv[0]);
		return 1;
	}

	const char* inputPath = argv[1];
	const char* outputDir = argv[2];
	const char* appName = argv[3];

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

	// Render icon at all required sizes
	result = render_icon_sizes((const uint8*)data, dataSize, outputDir, appName);
	if (result != B_OK) {
		return 1;
	}

	printf("\nSuccessfully created PNG icons for '%s' in '%s'\n",
		appName, outputDir);
	printf("You may need to run 'gtk-update-icon-cache %s/hicolor' to update the icon cache.\n",
		outputDir);
	
	return 0;
}
