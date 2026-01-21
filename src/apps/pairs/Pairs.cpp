/*
 * Copyright 2008 Ralf Schülke, ralf.schuelke@googlemail.com.
 * Copyright 2014 Haiku, Inc. All rights reserved.
 *
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Ralf Schülke, ralf.schuelke@googlemail.com
 *		John Scipione, jscipione@gmail.com
 */


#include "Pairs.h"

#include <stdio.h>
	// for snprintf()
#include <stdlib.h>

#include <Alert.h>
#include <Catalog.h>
#include <Message.h>
#include <MimeType.h>
#include <Path.h>
#include <String.h>
#include <Resources.h>

#include "PairsWindow.h"
#include <FindDirectory.h>



#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Pairs"


const char* kSignature = "application/x-vnd.Haiku-Pairs";

static const size_t kMinIconCount = 64;
static const size_t kMaxIconCount = 384;


//	#pragma mark - Pairs


Pairs::Pairs()
	:
	BApplication(kSignature),
	fWindow(NULL)
{
	_GetVectorIcons();
}


Pairs::~Pairs()
{
}


void
Pairs::ReadyToRun()
{
	fWindow = new PairsWindow();
	fWindow->Show();
}


void
Pairs::RefsReceived(BMessage* message)
{
	fWindow->PostMessage(message);
}


void
Pairs::MessageReceived(BMessage* message)
{
	BApplication::MessageReceived(message);
}


bool
Pairs::QuitRequested()
{
	// delete vector icons
	for (IconMap::iterator iter = fIconMap.begin(); iter != fIconMap.end();
			++iter) {
		vector_icon* icon = fIconMap[iter->first];
		free(icon->data);
		free(icon);
	}

	return true;
}


//	#pragma mark - Pairs private methods

uint8*
Pairs::GetNextRawSystemIcon(size_t* size)
{
	static BResources resources;
	static bool resourcesAreLoaded = false;
	static int32 index = 0;
	const char* name;
	int32 id;

	if (!resourcesAreLoaded) {
		BPath path;
		status_t status = find_directory(B_SYSTEM_LIB_DIRECTORY, &path);
		if (status != B_OK) {
			return NULL;
		}

#if defined(__APPLE__)
		path.Append("libbe.dylib");
#elif defined(_WIN32) || defined(WIN32)
		path.Append("libbe.dll");
#else
		path.Append("libbe.so");
#endif
		BFile file;
		status = file.SetTo(path.Path(), B_READ_ONLY);
		if (status != B_OK) {
			return NULL;
		}

		status = resources.SetTo(&file);
		if (status != B_OK) {
			return NULL;
		}

		resourcesAreLoaded = true;
	}

	resources.GetResourceInfo(B_VECTOR_ICON_TYPE, index++, &id, &name, size);

	// Try to load vector icon
	return (uint8*)resources.LoadResource(B_VECTOR_ICON_TYPE, name, size);
}


void
Pairs::_GetVectorIcons()
{
	BResources resources;
	size_t size;
	uint8* data;

	// Load vector icons from libbe resources and add a pointer to them
	// into a std::map keyed by a generated hash.

	while ((data = GetNextRawSystemIcon(&size))) {

		size_t hash = 0xdeadbeef;
		for (size_t i = 0; i < size; i++)
			hash = 31 * hash + data[i];

		if (fIconMap.find(hash) != fIconMap.end()) {
			// key has already been added to the map (data is owned by BResources, don't delete)
			continue;
		}

		vector_icon* icon = (vector_icon*)malloc(sizeof(vector_icon));
		if (icon == NULL) {
			free(icon);
			continue;
		}

		// Copy the data since BResources owns the original
		icon->data = (uint8*)malloc(size);
		if (icon->data == NULL) {
			free(icon);
			continue;
		}
		memcpy(icon->data, data, size);
		icon->size = size;

		// found a vector icon, add it to the list
		fIconMap[hash] = icon;
		if (fIconMap.size() >= kMaxIconCount) {
			// this is enough to choose from, stop eating memory...
			return;
		}
	}

	if (fIconMap.size() < kMinIconCount) {
		char buffer[512];
		snprintf(buffer, sizeof(buffer),
			B_TRANSLATE_COMMENT("%s did not find enough vector icons "
			"to start; it needs at least %zu, found %zu.\n",
			"Don't translate \"%s\" and \"%zu\", but make sure to keep them."),
			B_TRANSLATE_SYSTEM_NAME("Pairs"), kMinIconCount, fIconMap.size());
		BString messageString(buffer);
		BAlert* alert = new BAlert("Fatal", messageString.String(),
			B_TRANSLATE("OK"), NULL, NULL, B_WIDTH_FROM_WIDEST,
			B_STOP_ALERT);
		alert->SetFlags(alert->Flags() | B_CLOSE_ON_ESCAPE);
		alert->Go();
		exit(1);
	}
}


//	#pragma mark - main


int
main(void)
{
	Pairs pairs;
	pairs.Run();

	return 0;
}
