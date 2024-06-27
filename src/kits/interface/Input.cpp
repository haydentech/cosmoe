/*
 * Copyright (c) 2001-2008, Haiku, Inc.
 * Distributed under the terms of the MIT license.
 *
 * Authors:
 *		Marc Flerackers (mflerackers@androme.be)
 */

//!	Functions and class to manage input devices.

#include <stdlib.h>
#include <string.h>
#include <new>

#include <Input.h>
#include <Errors.h>
#include <List.h>
#include <Message.h>


status_t _control_input_server_(BMessage *command, BMessage *reply);


status_t
_control_input_server_(BMessage *command, BMessage *reply)
{
	return B_OK;
}
//------------------------------------------------------------------------------
