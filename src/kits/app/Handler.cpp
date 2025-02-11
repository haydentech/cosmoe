/*
 * Copyright 2001-2014 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Axel Dörfler, axeld@pinc-software.de
 *		Erik Jaesler, erik@cgsoftware.com
 */



#include <Handler.h>

#include <algorithm>
#include <new>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <vector>


BHandler::BHandler(const char* name)
	: fName(NULL)
{
	_InitData(name);
}


BHandler::~BHandler()
{
	free(fName);
}


void
BHandler::SetName(const char* name)
{
	if (fName != NULL) {
		free(fName);
		fName = NULL;
	}

	if (name != NULL)
		fName = strdup(name);
}


const char*
BHandler::Name() const
{
	return fName;
}


void
BHandler::_InitData(const char* name)
{
	SetName(name);
}

