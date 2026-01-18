/*
 * Copyright 2026, Bill Hayden
 * Distributed under the terms of the MIT License.
 *
 * vicn2ico - Convert VICN (vector icon) resources to Windows .ico format
 *
 * This tool reads the first VICN resource from a .rsrc file, renders it at multiple
 * sizes, and creates a .ico file for Windows apps.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <Bitmap.h>
#include <File.h>
#include <Resources.h>
#include <IconUtils.h>

// Standard Windows icon sizes for .ico files
static const int kIconSizes[] = { 16, 32, 48, 256, 0 };

#pragma pack(push, 1)

// Windows ICO file format structures
struct ICONDIR {
	uint16_t reserved;    // Reserved (must be 0)
	uint16_t type;        // Resource type (1 for icons)
	uint16_t count;       // Number of images
};

struct ICONDIRENTRY {
	uint8_t  width;          // Width in pixels (0 means 256)
	uint8_t  height;         // Height in pixels (0 means 256)
	uint8_t  colorCount;     // Number of colors in palette (0 if >= 8bpp)
	uint8_t  reserved;       // Reserved (must be 0)
	uint16_t planes;         // Color planes
	uint16_t bitCount;       // Bits per pixel
	uint32_t bytesInRes;     // Size of image data in bytes
	uint32_t imageOffset;    // Offset to image data from beginning of file
};

struct BITMAPINFOHEADER {
	uint32_t size;           // Size of this header (40 bytes)
	int32_t  width;          // Width of bitmap in pixels
	int32_t  height;         // Height of bitmap in pixels (2x actual for icon)
	uint16_t planes;         // Number of color planes (must be 1)
	uint16_t bitCount;       // Bits per pixel (32 for RGBA)
	uint32_t compression;    // Compression method (0 = uncompressed)
	uint32_t sizeImage;      // Size of image data
	int32_t  xPelsPerMeter;  // Horizontal resolution
	int32_t  yPelsPerMeter;  // Vertical resolution
	uint32_t clrUsed;        // Number of colors in palette
	uint32_t clrImportant;   // Important colors
};

#pragma pack(pop)


static void
print_usage(const char* program)
{
	fprintf(stderr, "Usage: %s <input.rsrc> <output.ico>\n", program);
	fprintf(stderr, "\n");
	fprintf(stderr, "Converts a VICN (vector icon) resource to Windows .ico format.\n");
	fprintf(stderr, "The input .rsrc file must contain at least one VICN resource.\n");
	fprintf(stderr, "\n");
}


static status_t
write_ico_image(FILE* fp, BBitmap* bitmap)
{
	uint32 width = bitmap->Bounds().IntegerWidth() + 1;
	uint32 height = bitmap->Bounds().IntegerHeight() + 1;
	
	// Write BITMAPINFOHEADER
	BITMAPINFOHEADER bih;
	memset(&bih, 0, sizeof(bih));
	bih.size = sizeof(BITMAPINFOHEADER);
	bih.width = width;
	bih.height = height * 2;  // Double height for icon (includes AND mask)
	bih.planes = 1;
	bih.bitCount = 32;  // RGBA
	bih.compression = 0;  // BI_RGB
	bih.sizeImage = width * height * 4;
	
	if (fwrite(&bih, sizeof(bih), 1, fp) != 1) {
		return B_ERROR;
	}
	
	// Write pixel data (bottom-up, BGRA format for Windows)
	uint8* bits = (uint8*)bitmap->Bits();
	uint32 bpr = bitmap->BytesPerRow();
	
	// Windows DIB format is bottom-up, so we write rows in reverse order
	for (int y = height - 1; y >= 0; y--) {
		uint8* row = bits + (y * bpr);
		if (fwrite(row, width * 4, 1, fp) != 1) {
			return B_ERROR;
		}
	}
	
	// Write AND mask (all zeros since we use alpha channel)
	uint32 maskRowBytes = ((width + 31) / 32) * 4;  // Align to 32-bit boundary
	uint8* maskRow = (uint8*)calloc(maskRowBytes, 1);
	if (!maskRow) {
		return B_NO_MEMORY;
	}
	
	for (uint32 y = 0; y < height; y++) {
		if (fwrite(maskRow, maskRowBytes, 1, fp) != 1) {
			free(maskRow);
			return B_ERROR;
		}
	}
	
	free(maskRow);
	return B_OK;
}


static size_t
calculate_image_size(uint32 width, uint32 height)
{
	uint32 maskRowBytes = ((width + 31) / 32) * 4;
	return sizeof(BITMAPINFOHEADER) + 
	       (width * height * 4) +           // RGBA pixel data
	       (maskRowBytes * height);         // AND mask
}


static status_t
create_ico_file(const uint8* vectorData, size_t dataSize, const char* outputPath)
{
	status_t result;
	
	// Count how many sizes we'll generate
	int numSizes = 0;
	for (int i = 0; kIconSizes[i] != 0; i++) {
		numSizes++;
	}
	
	// Open output file
	FILE* fp = fopen(outputPath, "wb");
	if (!fp) {
		fprintf(stderr, "Error: Failed to create output file '%s': %s\n",
			outputPath, strerror(errno));
		return B_ERROR;
	}
	
	// Write ICONDIR header
	ICONDIR header;
	header.reserved = 0;
	header.type = 1;  // 1 = icon
	header.count = numSizes;
	
	if (fwrite(&header, sizeof(header), 1, fp) != 1) {
		fclose(fp);
		return B_ERROR;
	}
	
	// Calculate offsets for each image
	uint32 currentOffset = sizeof(ICONDIR) + (numSizes * sizeof(ICONDIRENTRY));
	
	// Write directory entries (placeholders for now)
	ICONDIRENTRY* entries = (ICONDIRENTRY*)calloc(numSizes, sizeof(ICONDIRENTRY));
	if (!entries) {
		fclose(fp);
		return B_NO_MEMORY;
	}
	
	for (int i = 0; i < numSizes; i++) {
		int size = kIconSizes[i];
		entries[i].width = (size == 256) ? 0 : size;   // 0 means 256
		entries[i].height = (size == 256) ? 0 : size;
		entries[i].colorCount = 0;  // 0 for >= 8bpp
		entries[i].reserved = 0;
		entries[i].planes = 1;
		entries[i].bitCount = 32;  // RGBA
		entries[i].bytesInRes = calculate_image_size(size, size);
		entries[i].imageOffset = currentOffset;
		currentOffset += entries[i].bytesInRes;
		
		if (fwrite(&entries[i], sizeof(ICONDIRENTRY), 1, fp) != 1) {
			free(entries);
			fclose(fp);
			return B_ERROR;
		}
	}
	
	// Now render and write each icon size
	for (int i = 0; i < numSizes; i++) {
		int size = kIconSizes[i];
		
		// Create bitmap at this size
		BRect bounds(0, 0, size - 1, size - 1);
		BBitmap* bitmap = new BBitmap(bounds, B_BITMAP_NO_SERVER_LINK, B_RGBA32);
		if (bitmap == NULL || bitmap->InitCheck() != B_OK) {
			fprintf(stderr, "Error: Failed to create bitmap for size %dx%d: %s\n",
				size, size, strerror(bitmap ? bitmap->InitCheck() : B_NO_MEMORY));
			delete bitmap;
			free(entries);
			fclose(fp);
			return B_NO_MEMORY;
		}
		
		// Render vector icon to bitmap
		result = BIconUtils::GetVectorIcon(vectorData, dataSize, bitmap);
		if (result != B_OK) {
			fprintf(stderr, "Error: Failed to render icon at size %dx%d: %s\n",
				size, size, strerror(result));
			delete bitmap;
			free(entries);
			fclose(fp);
			return result;
		}
		
		// Write icon data
		result = write_ico_image(fp, bitmap);
		delete bitmap;
		
		if (result != B_OK) {
			free(entries);
			fclose(fp);
			return result;
		}
	}
	
	free(entries);
	fclose(fp);
	
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

	// Create the ICO file directly
	result = create_ico_file((const uint8*)data, dataSize, outputPath);
	
	if (result != B_OK) {
		return 1;
	}

	printf("Successfully created '%s' from VICN resource in '%s'\n",
		outputPath, inputPath);
	
	return 0;
}
