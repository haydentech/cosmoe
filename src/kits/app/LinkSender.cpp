/*
 * Copyright 2001-2005, Haiku.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Pahtz <pahtz@yahoo.com.au>
 *		Axel Dörfler, axeld@pinc-software.de
 */

/** Class for low-overhead port-based messaging */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <new>
#include <errno.h>

#include <ServerProtocol.h>
#include <LinkSender.h>

#include "link_message.h"


//#define DEBUG_BPORTLINK
#ifdef DEBUG_BPORTLINK
#	include <stdio.h>
#	define STRACE(x) printf x
#else
#	define STRACE(x) ;
#endif

static const size_t kMaxStringSize = 4096;
static const size_t kWatermark = kInitialBufferSize - 24;
	// if a message is started after this mark, the buffer is flushed automatically

namespace BPrivate {

LinkSender::LinkSender(port_id port)
{
}


LinkSender::~LinkSender()
{
}


void
LinkSender::SetPort(port_id port)
{
}


status_t
LinkSender::StartMessage(int32 code, size_t minSize)
{
	return B_OK;
}


status_t
LinkSender::EndMessage(bool needsReply)
{
	return B_OK;
}


void
LinkSender::CancelMessage()
{
}


status_t
LinkSender::Attach(const void *passedData, size_t passedSize)
{
	return B_OK;
}


status_t
LinkSender::AttachString(const char *string, int32 length)
{
	return B_OK;
}


status_t
LinkSender::AdjustBuffer(size_t newSize, char **_oldBuffer)
{
	return B_OK;
}


status_t
LinkSender::FlushCompleted(size_t newBufferSize)
{
	return B_OK;
}


status_t
LinkSender::Flush(bigtime_t timeout, bool needsReply)
{
	return B_OK;
}

}	// namespace BPrivate
