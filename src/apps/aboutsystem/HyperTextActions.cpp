/*
 * Copyright 2008, Ingo Weinhold, ingo_weinhold@gmx.de.
 * Distributed under the terms of the MIT license.
 */

#include "HyperTextActions.h"

#include <Entry.h>
#include <Message.h>
#include <Roster.h>

#include <cstdlib>


// #pragma mark - URLAction


URLAction::URLAction(const BString& url)
	:
	fURL(url)
{
}


URLAction::~URLAction()
{
}


void
URLAction::Clicked(HyperTextView* view, BPoint where, BMessage* message)
{
	BString cmd;
#ifdef _WIN32
	cmd << "start \"\" \"" << fURL << "\"";
#elif __APPLE__
	cmd << "open \"" << fURL << "\"";
#else
	cmd << "xdg-open \"" << fURL << "\"";
#endif
	std::system(cmd.String());
}


// #pragma mark - OpenFileAction


OpenFileAction::OpenFileAction(const BString& file)
	:
	fFile(file)
{
}


OpenFileAction::~OpenFileAction()
{
}


void
OpenFileAction::Clicked(HyperTextView* view, BPoint where, BMessage* message)
{
	BString cmd;
#ifdef _WIN32
	cmd << "start \"\" \"" << fFile.String() << "\"";
#elif __APPLE__
	cmd << "open \"" << fFile.String() << "\"";
#else
	cmd << "xdg-open \"" << fFile.String() << "\"";
#endif
	std::system(cmd.String());
}
