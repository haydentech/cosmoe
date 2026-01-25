/*
 * Copyright 2010, Haiku.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Clemens Zeidler <haiku@clemens-zeidler.de>
 */


#include "WindowStack.h"

#include <new>

#include <Window.h>


using namespace BPrivate;


BWindowStack::BWindowStack(BWindow* window)
{
}


BWindowStack::~BWindowStack()
{

}


status_t
BWindowStack::AddWindow(const BWindow* window)
{
	return B_ERROR;
}


status_t
BWindowStack::AddWindow(const BMessenger& window)
{
	return AddWindowAt(window, -1);
}


status_t
BWindowStack::AddWindowAt(const BWindow* window, int32 position)
{
	return B_ERROR;
}


status_t
BWindowStack::AddWindowAt(const BMessenger& window, int32 position)
{
	return B_ERROR;
}


status_t
BWindowStack::RemoveWindow(const BWindow* window)
{
	return B_ERROR;
}


status_t
BWindowStack::RemoveWindow(const BMessenger& window)
{
	return B_ERROR;
}


status_t
BWindowStack::RemoveWindowAt(int32 position, BMessenger* window)
{
	return B_ERROR;
}


int32
BWindowStack::CountWindows()
{
	return -1;
}


status_t
BWindowStack::WindowAt(int32 position, BMessenger& messenger)
{
	return B_ERROR;
}


bool
BWindowStack::HasWindow(const BWindow* window)
{
	return false;
}


bool
BWindowStack::HasWindow(const BMessenger& window)
{
	return B_ERROR;
}


status_t
BWindowStack::_AttachMessenger(const BMessenger& window)
{
	return B_ERROR;
}


status_t
BWindowStack::_ReadMessenger(BMessenger& window)
{
	return B_ERROR;
}


status_t
BWindowStack::_StartMessage(int32 what)
{
	return B_ERROR;
}
