#include "disapp.h"
#include <Path.h>
#include <TranslatorFormats.h>
#include <TranslationUtils.h>


DisApplication::DisApplication()
	: BApplication ("application/x-vnd.minimal-noui")
{
	BBitmap* image = BTranslationUtils::GetBitmap(B_PNG_FORMAT, "walter_logo.png");
	if (image == NULL) {
		fprintf(stderr, "Failed to load walter_logo.png\n");
		return;
	}
	PostMessage(new BMessage(B_QUIT_REQUESTED));
}
