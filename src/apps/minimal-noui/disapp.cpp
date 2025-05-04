#include "disapp.h"
#include <Path.h>
#include <TranslatorRoster.h>


DisApplication::DisApplication()
	: BApplication ("application/x-vnd.minimal-noui")
{
	BPath path("/usr/local/lib/aarch64-linux-gnu/PNGTranslator.so");
	image_id image = load_add_on(path.Path());
	if (image == NULL) {
		printf("Failed to load add-on: %s\n", path.Path());
		exit(EXIT_FAILURE);
	}

	// Function pointer used to create post R4.5 style translators
	BTranslator* (*makeNthTranslator)(int32 n, image_id you, uint32 flags, ...);

	status_t status = get_image_symbol(image, "make_nth_translator", B_SYMBOL_TYPE_TEXT, (void**)&makeNthTranslator);
	if (status < B_OK) {
		printf("Failed to get symbol make_nth_translator: %s\n", strerror(status));
		exit(EXIT_FAILURE);
	}

	printf("Successfully loaded add-on: %s\n", path.Path());
	unload_add_on(image);
	printf("Successfully unloaded add-on: %s\n", path.Path());
	PostMessage(new BMessage(B_QUIT_REQUESTED));
}
