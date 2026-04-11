/*
 * Copyright 2001-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Christopher ML Zumwalt May (zummy@users.sf.net)
 *		Jérôme Duval
 */


#include <PushGameSound.h>

#include <Errors.h>


BPushGameSound::BPushGameSound(size_t inBufferFrameCount,
	const gs_audio_format *format, size_t inBufferCount,
	BGameSoundDevice *device)
	:
	BStreamingGameSound(device),
	fLock(B_BAD_SEM_ID),
	fPageLocked(NULL),
	fLockPos(0),
	fPlayPos(0),
	fBuffer(NULL),
	fPageSize(0),
	fPageCount(0),
	fBufferSize(0)
{
	SetInitError(SetParameters(inBufferFrameCount, format, inBufferCount));
}


BPushGameSound::BPushGameSound(BGameSoundDevice * device)
		:	BStreamingGameSound(device),
			fLock(B_BAD_SEM_ID),
			fPageLocked(NULL),
			fLockPos(0),
			fPlayPos(0),
			fBuffer(NULL),
			fPageSize(0),
			fPageCount(0),
			fBufferSize(0)
{
}


BPushGameSound::~BPushGameSound()
{
}


BPushGameSound::lock_status
BPushGameSound::LockNextPage(void **out_pagePtr, size_t *out_pageSize)
{
	(void)out_pagePtr;
	(void)out_pageSize;
	return lock_failed;
}


status_t
BPushGameSound::UnlockPage(void *in_pagePtr)
{
	(void)in_pagePtr;
	return B_UNSUPPORTED;
}


BPushGameSound::lock_status
BPushGameSound::LockForCyclic(void **out_basePtr, size_t *out_size)
{
	(void)out_basePtr;
	(void)out_size;
	return lock_failed;
}


status_t
BPushGameSound::UnlockCyclic()
{
	return B_UNSUPPORTED;
}


size_t
BPushGameSound::CurrentPosition()
{
	return fPlayPos;
}


BGameSound *
BPushGameSound::Clone() const
{
	return NULL;
}


status_t
BPushGameSound::Perform(int32 selector, void *data)
{
	(void)selector;
	(void)data;
	return B_ERROR;
}


status_t
BPushGameSound::SetParameters(size_t inBufferFrameCount,
	const gs_audio_format *format, size_t inBufferCount)
{
	return BStreamingGameSound::SetParameters(inBufferFrameCount, format, inBufferCount);
}


status_t
BPushGameSound::SetStreamHook(void (*hook)(void* inCookie, void* buffer,
	size_t byteCount, BStreamingGameSound* me), void* cookie)
{
	return BStreamingGameSound::SetStreamHook(hook, cookie);
}


void
BPushGameSound::FillBuffer(void *inBuffer, size_t inByteCount)
{
	BStreamingGameSound::FillBuffer(inBuffer, inByteCount);
}


bool
BPushGameSound::BytesReady(size_t * bytes)
{
	if (bytes != NULL)
		*bytes = 0;
	return false;
}


/* unimplemented for protection of the user:
 *
 * BPushGameSound::BPushGameSound()
 * BPushGameSound::BPushGameSound(const BPushGameSound &)
 * BPushGameSound &BPushGameSound::operator=(const BPushGameSound &)
 */


status_t
BPushGameSound::_Reserved_BPushGameSound_0(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BPushGameSound::_Reserved_BPushGameSound_1(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BPushGameSound::_Reserved_BPushGameSound_2(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BPushGameSound::_Reserved_BPushGameSound_3(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BPushGameSound::_Reserved_BPushGameSound_4(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BPushGameSound::_Reserved_BPushGameSound_5(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BPushGameSound::_Reserved_BPushGameSound_6(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BPushGameSound::_Reserved_BPushGameSound_7(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BPushGameSound::_Reserved_BPushGameSound_8(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BPushGameSound::_Reserved_BPushGameSound_9(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BPushGameSound::_Reserved_BPushGameSound_10(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BPushGameSound::_Reserved_BPushGameSound_11(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BPushGameSound::_Reserved_BPushGameSound_12(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BPushGameSound::_Reserved_BPushGameSound_13(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BPushGameSound::_Reserved_BPushGameSound_14(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BPushGameSound::_Reserved_BPushGameSound_15(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BPushGameSound::_Reserved_BPushGameSound_16(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BPushGameSound::_Reserved_BPushGameSound_17(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BPushGameSound::_Reserved_BPushGameSound_18(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BPushGameSound::_Reserved_BPushGameSound_19(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BPushGameSound::_Reserved_BPushGameSound_20(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BPushGameSound::_Reserved_BPushGameSound_21(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BPushGameSound::_Reserved_BPushGameSound_22(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BPushGameSound::_Reserved_BPushGameSound_23(int32 arg, ...)
{
	return B_ERROR;
}
