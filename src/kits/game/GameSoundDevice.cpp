/*
 * Copyright 2001-2002, Haiku. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Stefano Ceccherini (stefano.ceccherini@gmail.com)
 *		Adrien Destugues (pulkomandy)
 *		Jérôme Duval (korli)
 *		Christopher ML Zumwalt May (zummy@users.sf.net)
 */

//	Description:	Manages the game producer. The class may change without
//					notice and was only intended for use by the GameKit at
//					this time. Use at your own risk.

#include "GameSoundDevice.h"

#include "GameAudioBackendMiniaudio.h"

#include <Errors.h>

BGameSoundDevice*
BGameSoundDevice::GetDefaultDevice()
{
	static BGameSoundDevice sDevice;
	return &sDevice;
}


void
BGameSoundDevice::ReleaseDevice()
{
}


BGameSoundDevice::BGameSoundDevice()
	:
	fInitError(B_OK),
	fIsConnected(false),
	fSoundCount(0),
	fSounds(nullptr)
{
	fFormat = BPrivate::GameAudio::DeviceFormat();
	status_t err = BPrivate::GameAudio::InitBackend();
	if (err != B_OK)
		fInitError = err;
}


BGameSoundDevice::~BGameSoundDevice()
{
}


status_t
BGameSoundDevice::InitCheck() const
{
	return fInitError;
}


const gs_audio_format&
BGameSoundDevice::Format() const
{
	return BPrivate::GameAudio::DeviceFormat();
}


const gs_audio_format&
BGameSoundDevice::Format(gs_id sound) const
{
	return BPrivate::GameAudio::SoundFormat(sound);
}


void
BGameSoundDevice::SetInitError(status_t error)
{
	fInitError = error;
}


status_t
BGameSoundDevice::CreateBuffer(gs_id* sound, const gs_audio_format* format,
	const void* data, int64 frames)
{
	return BPrivate::GameAudio::CreateBufferFromPcm(format, data, frames, sound);
}


status_t
BGameSoundDevice::CreateBuffer(gs_id* sound, const void* object,
	const gs_audio_format* format, size_t inBufferFrameCount,
	size_t inBufferCount)
{
	return BPrivate::GameAudio::CreateStreamingBuffer(object, format,
		inBufferFrameCount, inBufferCount, sound);
}


void
BGameSoundDevice::ReleaseBuffer(gs_id sound)
{
	BPrivate::GameAudio::ReleaseBuffer(sound);
}


status_t
BGameSoundDevice::Buffer(gs_id sound, gs_audio_format* format, void*& data)
{
	return BPrivate::GameAudio::Buffer(sound, format, data);
}

bool
BGameSoundDevice::IsPlaying(gs_id sound)
{
	return BPrivate::GameAudio::IsPlaying(sound);
}


status_t
BGameSoundDevice::StartPlaying(gs_id sound)
{
	return BPrivate::GameAudio::StartPlaying(sound);
}


status_t
BGameSoundDevice::StopPlaying(gs_id sound)
{
	return BPrivate::GameAudio::StopPlaying(sound);
}

status_t
BGameSoundDevice::GetAttributes(gs_id sound, gs_attribute* attributes,
	size_t attributeCount)
{
	return BPrivate::GameAudio::GetAttributes(sound, attributes, attributeCount);
}


status_t
BGameSoundDevice::SetAttributes(gs_id sound, gs_attribute* attributes,
	size_t attributeCount)
{
	return BPrivate::GameAudio::SetAttributes(sound, attributes, attributeCount);
}


int32
BGameSoundDevice::AllocateSound()
{
	return 0;
}

