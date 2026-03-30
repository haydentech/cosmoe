/*
 * Copyright 2002-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Christopher ML Zumwalt May (zummy@users.sf.net)
 */


#include <GameSound.h>

#include "GameSoundDevice.h"


using std::nothrow;


// Local Defines ---------------------------------------------------------------

// BGameSound class ------------------------------------------------------------
BGameSound::BGameSound(BGameSoundDevice *device)
	:
	fInitError(B_OK),
	fSound(B_GS_INVALID_SOUND)
{
	if (device != NULL)
		fDevice = device;
	else
		fDevice = BGameSoundDevice::GetDefaultDevice();

	if (fDevice != NULL)
		fInitError = fDevice->InitCheck();
	else
		fInitError = B_NO_INIT;
}


BGameSound::BGameSound(const BGameSound &other)
	:	fDevice(other.fDevice),
		fInitError(other.fInitError),
		fFormat(other.fFormat),
		fSound(B_GS_INVALID_SOUND)
{
}


BGameSound::~BGameSound()
{
	if (fDevice != NULL && fSound >= 0)
		fDevice->ReleaseBuffer(fSound);

	BGameSoundDevice::ReleaseDevice();
}


status_t
BGameSound::InitCheck() const
{
	return fInitError;
}


BGameSoundDevice *
BGameSound::Device() const
{
	// TODO: Must return NULL if default device is being used!
	return fDevice;
}


gs_id
BGameSound::ID() const
{
	// TODO: Should be 0 if no sound has been selected! But fSound
	// is initialized with -1 in the constructors.
	return fSound;
}


const gs_audio_format &
BGameSound::Format() const
{
	if (fDevice == NULL)
		return fFormat;
	return fDevice->Format(fSound);
}


status_t
BGameSound::StartPlaying()
{
	if (fDevice == NULL || fSound < 0)
		return B_BAD_VALUE;
	return fDevice->StartPlaying(fSound);
}


bool
BGameSound::IsPlaying()
{
	if (fDevice == NULL || fSound < 0)
		return false;
	return fDevice->IsPlaying(fSound);
}


status_t
BGameSound::StopPlaying()
{
	if (fDevice == NULL || fSound < 0)
		return B_BAD_VALUE;
	return fDevice->StopPlaying(fSound);
}


status_t
BGameSound::SetGain(float gain, bigtime_t duration)
{
	gs_attribute attribute;

	attribute.attribute = B_GS_GAIN;
	attribute.value = gain;
	attribute.duration = duration;
	attribute.flags = 0;
	return SetAttributes(&attribute, 1);
}


status_t
BGameSound::SetPan(float pan, bigtime_t duration)
{
	gs_attribute attribute;

	attribute.attribute = B_GS_PAN;
	attribute.value = pan;
	attribute.duration = duration;
	attribute.flags = 0;
	return SetAttributes(&attribute, 1);
}


float
BGameSound::Gain()
{
	gs_attribute attribute;

	attribute.attribute = B_GS_GAIN;
	attribute.flags = 0;
	if (GetAttributes(&attribute, 1) != B_OK)
		return 0.0f;
	return attribute.value;
}


float
BGameSound::Pan()
{
	gs_attribute attribute;

	attribute.attribute = B_GS_PAN;
	attribute.flags = 0;
	if (GetAttributes(&attribute, 1) != B_OK)
		return 0.0f;
	return attribute.value;
}


status_t
BGameSound::SetAttributes(gs_attribute* attributes, size_t attributeCount)
{
	if (fDevice == NULL || fSound < 0)
		return B_BAD_VALUE;
	return fDevice->SetAttributes(fSound, attributes, attributeCount);
}


status_t
BGameSound::GetAttributes(gs_attribute* attributes, size_t attributeCount)
{
	if (fDevice == NULL || fSound < 0)
		return B_BAD_VALUE;
	return fDevice->GetAttributes(fSound, attributes, attributeCount);
}


status_t
BGameSound::Perform(int32 selector,
					void *data)
{
	return B_ERROR;
}


void *
BGameSound::operator new(size_t size)
{
	return ::operator new(size);
}


void *
BGameSound::operator new(size_t size, const std::nothrow_t &nt) throw()
{
	return ::operator new(size, nt);
}


void
BGameSound::operator delete(void *ptr)
{
	::operator delete(ptr);
}


#if !__MWERKS__
//	there's a bug in MWCC under R4.1 and earlier
void
BGameSound::operator delete(void *ptr, const std::nothrow_t &nt) throw()
{
	::operator delete(ptr, nt);
}
#endif


status_t
BGameSound::SetMemoryPoolSize(size_t in_poolSize)
{
	return B_ERROR;
}


status_t
BGameSound::LockMemoryPool(bool in_lockInCore)
{
	return B_ERROR;
}


int32
BGameSound::SetMaxSoundCount(int32 in_maxCount)
{
	return in_maxCount;
}


status_t
BGameSound::SetInitError(status_t in_initError)
{
	fInitError = in_initError;
	return B_OK;
}


status_t
BGameSound::Init(gs_id handle)
{
	if (fSound < 0)
		fSound = handle;

	return B_OK;
}

BGameSound&
BGameSound::operator=(const BGameSound& other)
{
	if (this == &other)
		return *this;
	fDevice = other.fDevice;
	fInitError = other.fInitError;
	fFormat = other.fFormat;
	return *this;
}


/* unimplemented for protection of the user:
 *
 * BGameSound::BGameSound()
 */


status_t
BGameSound::_Reserved_BGameSound_0(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_1(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_2(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_3(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_4(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_5(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_6(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_7(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_8(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_9(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_10(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_11(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_12(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_13(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_14(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_15(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_16(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_17(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_18(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_19(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_20(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_21(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_22(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_23(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_24(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_25(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_26(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_27(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_28(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_29(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_30(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_31(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_32(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_33(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_34(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_35(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_36(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_37(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_38(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_39(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_40(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_41(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_42(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_43(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_44(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_45(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_46(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BGameSound::_Reserved_BGameSound_47(int32 arg, ...)
{
	return B_ERROR;
}
