/*
 * Copyright 2001-2006, Haiku.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Marc Flerackers (mflerackers@androme.be)
 *		Stefano Ceccherini (burton666@libero.it)
 */

#include "TokenHandler.h"
#include "ServerPicture.h"

TokenHandler picture_token_handler;

ServerPicture::ServerPicture(void)
{
	_token=picture_token_handler.GetToken();
	
	_initialized=false;
	
	int8 *ptr;
	_area=create_area("ServerPicture",(void**)&ptr,B_ANY_ADDRESS,B_PAGE_SIZE,
		B_NO_LOCK,B_READ_AREA | B_WRITE_AREA);
	
	if(_area!=B_BAD_VALUE && _area!=B_NO_MEMORY && _area!=B_ERROR)
		_initialized=true;
}

ServerPicture::~ServerPicture(void)
{
}
