/*
 * Copyright 2001-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Christopher ML Zumwalt May (zummy@users.sf.net)
 */


#include <SimpleGameSound.h>

#include <Entry.h>
#include <MediaDefs.h>
#include <Path.h>

#include "GameAudioBackendMiniaudio.h"
#include "GameSoundDevice.h"

#include <vector>

BSimpleGameSound::BSimpleGameSound(const entry_ref *inFile,
	BGameSoundDevice *device)
	:
	BGameSound(device)
{
	if (InitCheck() == B_OK)
		SetInitError(Init(inFile));
}

BSimpleGameSound::BSimpleGameSound(const char* file, BGameSoundDevice* device)
	:	BGameSound(device)
{
	if (InitCheck() != B_OK)
		return;
	if (file == NULL || file[0] == '\0') {
		SetInitError(B_BAD_VALUE);
		return;
	}

	std::vector<float> samples;
	int64 frames = 0;
	uint32 channels = 0;
	float sampleRate = 0;
	status_t err = BPrivate::GameAudio::DecodeFileToFloat32(file, samples,
		frames, channels, sampleRate);
	if (err != B_OK) {
		SetInitError(err);
		return;
	}

	gs_audio_format format;
	format.frame_rate = sampleRate;
	format.channel_count = channels;
	format.format = gs_audio_format::B_GS_F;
	format.byte_order = B_MEDIA_HOST_ENDIAN;
	format.buffer_size = channels * sizeof(float) * 1024;

	SetInitError(Init(samples.data(), frames, &format));
}

BSimpleGameSound::BSimpleGameSound(const void* data, size_t inFrameCount,
	const gs_audio_format* format, BGameSoundDevice* device)
	:	BGameSound(device)
{
	if (InitCheck() == B_OK)
		SetInitError(Init(data, (int64)inFrameCount, format));
}

BSimpleGameSound::BSimpleGameSound(const BSimpleGameSound &other)
	:
	BGameSound(other)
{
	gs_audio_format format;
	void *data = NULL;

	status_t error = other.Device()->Buffer(other.ID(), &format, data);
	if (error != B_OK) {
		SetInitError(error);
		return;
	}

	gs_id sound;
	error = Device()->CreateBuffer(&sound, &format, data,
		(int64)(format.buffer_size / (format.channel_count * sizeof(float))));
	free(data);
	if (error != B_OK)
		SetInitError(error);
	else
		BGameSound::Init(sound);
}


BSimpleGameSound::~BSimpleGameSound()
{
}


BGameSound *
BSimpleGameSound::Clone() const
{
	gs_audio_format format;
	void *data = NULL;

	status_t error = Device()->Buffer(ID(), &format, data);
	if (error != B_OK)
		return NULL;

	size_t frameSize = format.channel_count;
	switch (format.format) {
		case gs_audio_format::B_GS_U8:
			frameSize *= sizeof(uint8);
			break;
		case gs_audio_format::B_GS_S16:
			frameSize *= sizeof(int16);
			break;
		case gs_audio_format::B_GS_S32:
			frameSize *= sizeof(int32);
			break;
		case gs_audio_format::B_GS_F:
			frameSize *= sizeof(float);
			break;
		default:
			free(data);
			return NULL;
	}

	size_t frameCount = frameSize > 0 ? format.buffer_size / frameSize : 0;
	BSimpleGameSound *clone = new BSimpleGameSound(data, frameCount, &format,
		Device());
	free(data);

	return clone;
}


/* virtual */ status_t
BSimpleGameSound::Perform(int32 selector, void * data)
{
	return B_ERROR;
}


status_t
BSimpleGameSound::SetIsLooping(bool looping)
{
	gs_attribute attribute;

	attribute.attribute = B_GS_LOOPING;
	attribute.value = looping ? 1.0f : 0.0f;
	attribute.duration = 0;
	attribute.flags = 0;

	return SetAttributes(&attribute, 1);
}


bool
BSimpleGameSound::IsLooping() const
{
	gs_attribute attribute;

	attribute.attribute = B_GS_LOOPING;
	attribute.flags = 0;

	if (const_cast<BSimpleGameSound*>(this)->GetAttributes(&attribute, 1) != B_OK)
		return false;
	return attribute.value != 0.0f;
}


status_t
BSimpleGameSound::Init(const entry_ref* inFile)
{
	if (inFile == NULL)
		return B_BAD_VALUE;

	BEntry entry(inFile);
	BPath path;
	if (entry.GetPath(&path) != B_OK)
		return B_ENTRY_NOT_FOUND;

	std::vector<float> samples;
	int64 frames = 0;
	uint32 channels = 0;
	float sampleRate = 0;
	status_t err = BPrivate::GameAudio::DecodeFileToFloat32(path.Path(), samples,
		frames, channels, sampleRate);
	if (err != B_OK)
		return err;

	gs_audio_format format;
	format.frame_rate = sampleRate;
	format.channel_count = channels;
	format.format = gs_audio_format::B_GS_F;
	format.byte_order = B_MEDIA_HOST_ENDIAN;
	format.buffer_size = channels * sizeof(float) * 1024;
	return Init(samples.data(), frames, &format);
}


status_t
BSimpleGameSound::Init(const void* inData, int64 inFrameCount,
	const gs_audio_format* format)
{
	if (inData == NULL || format == NULL || inFrameCount <= 0)
		return B_BAD_VALUE;

	gs_id sound;

	status_t error
		= Device()->CreateBuffer(&sound, format, inData, inFrameCount);
	if (error != B_OK)
		return error;

	return BGameSound::Init(sound);
}


/* unimplemented for protection of the user:
 *
 * BSimpleGameSound::BSimpleGameSound()
 * BSimpleGameSound &BSimpleGameSound::operator=(const BSimpleGameSound &)
 */


status_t
BSimpleGameSound::_Reserved_BSimpleGameSound_0(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BSimpleGameSound::_Reserved_BSimpleGameSound_1(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BSimpleGameSound::_Reserved_BSimpleGameSound_2(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BSimpleGameSound::_Reserved_BSimpleGameSound_3(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BSimpleGameSound::_Reserved_BSimpleGameSound_4(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BSimpleGameSound::_Reserved_BSimpleGameSound_5(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BSimpleGameSound::_Reserved_BSimpleGameSound_6(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BSimpleGameSound::_Reserved_BSimpleGameSound_7(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BSimpleGameSound::_Reserved_BSimpleGameSound_8(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BSimpleGameSound::_Reserved_BSimpleGameSound_9(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BSimpleGameSound::_Reserved_BSimpleGameSound_10(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BSimpleGameSound::_Reserved_BSimpleGameSound_11(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BSimpleGameSound::_Reserved_BSimpleGameSound_12(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BSimpleGameSound::_Reserved_BSimpleGameSound_13(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BSimpleGameSound::_Reserved_BSimpleGameSound_14(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BSimpleGameSound::_Reserved_BSimpleGameSound_15(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BSimpleGameSound::_Reserved_BSimpleGameSound_16(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BSimpleGameSound::_Reserved_BSimpleGameSound_17(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BSimpleGameSound::_Reserved_BSimpleGameSound_18(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BSimpleGameSound::_Reserved_BSimpleGameSound_19(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BSimpleGameSound::_Reserved_BSimpleGameSound_20(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BSimpleGameSound::_Reserved_BSimpleGameSound_21(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BSimpleGameSound::_Reserved_BSimpleGameSound_22(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BSimpleGameSound::_Reserved_BSimpleGameSound_23(int32 arg, ...)
{
	return B_ERROR;
}
