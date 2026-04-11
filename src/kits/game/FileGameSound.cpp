/*
 * Copyright 2001-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Christopher ML Zumwalt May (zummy@users.sf.net)
 *		Jérôme Duval
 */


#include <FileGameSound.h>

#include <Entry.h>
#include <MediaDefs.h>
#include <Path.h>

#include "GameAudioBackendMiniaudio.h"
#include "GameSoundDevice.h"

#include <Errors.h>

#include <vector>


// Local utility functions -----------------------------------------------
static status_t
init_sound_from_samples(BFileGameSound* owner, const std::vector<float>& samples,
	int64 frames, uint32 channels, float sampleRate, gs_id* outSound)
{
	if (owner == NULL || samples.empty() || frames <= 0 || channels == 0
		|| sampleRate <= 0.0f || outSound == NULL)
		return B_BAD_VALUE;

	gs_audio_format format;
	format.frame_rate = sampleRate;
	format.channel_count = channels;
	format.format = gs_audio_format::B_GS_F;
	format.byte_order = B_MEDIA_HOST_ENDIAN;
	format.buffer_size = channels * sizeof(float) * 1024;

	gs_id sound;
	status_t err = owner->Device()->CreateBuffer(&sound, &format, samples.data(),
		frames);
	if (err != B_OK)
		return err;

	*outSound = sound;
	return B_OK;
}


// BFileGameSound -------------------------------------------------------
BFileGameSound::BFileGameSound(const entry_ref* file, bool looping,
	BGameSoundDevice* device)
	:
	BStreamingGameSound(device),
	fAudioStream(NULL),
	fStopping(false),
	fLooping(looping),
	fReserved(0),
	fBuffer(NULL),
	fFrameSize(0),
	fBufferSize(0),
	fPlayPosition(0),
	fPausing(NULL),
	fPaused(false),
	fPauseGain(1.0f),
	fDataSource(NULL)
{
	if (InitCheck() != B_OK)
		return;

	if (file == NULL) {
		SetInitError(B_BAD_VALUE);
		return;
	}

	BEntry entry(file);
	BPath path;
	if (entry.GetPath(&path) != B_OK) {
		SetInitError(B_ENTRY_NOT_FOUND);
		return;
	}

	std::vector<float> samples;
	int64 frames = 0;
	uint32 channels = 0;
	float sampleRate = 0;
	status_t err = BPrivate::GameAudio::DecodeFileToFloat32(path.Path(), samples,
		frames, channels, sampleRate);
	if (err != B_OK) {
		SetInitError(err);
		return;
	}

	gs_id sound;
	err = init_sound_from_samples(this, samples, frames, channels, sampleRate,
		&sound);
	if (err == B_OK)
		err = BGameSound::Init(sound);
	if (err == B_OK) {
		gs_attribute attribute;
		attribute.attribute = B_GS_LOOPING;
		attribute.duration = 0;
		attribute.value = looping ? 1.0f : 0.0f;
		attribute.flags = 0;
		err = SetAttributes(&attribute, 1);
	}
	SetInitError(err);
}


BFileGameSound::BFileGameSound(const char* file, bool looping,
	BGameSoundDevice* device)
	:
	BStreamingGameSound(device),
	fAudioStream(NULL),
	fStopping(false),
	fLooping(looping),
	fReserved(0),
	fBuffer(NULL),
	fFrameSize(0),
	fBufferSize(0),
	fPlayPosition(0),
	fPausing(NULL),
	fPaused(false),
	fPauseGain(1.0f),
	fDataSource(NULL)
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

	gs_id sound;
	err = init_sound_from_samples(this, samples, frames, channels, sampleRate,
		&sound);
	if (err == B_OK)
		err = BGameSound::Init(sound);
	if (err == B_OK) {
		gs_attribute attribute;
		attribute.attribute = B_GS_LOOPING;
		attribute.duration = 0;
		attribute.value = looping ? 1.0f : 0.0f;
		attribute.flags = 0;
		err = SetAttributes(&attribute, 1);
	}
	SetInitError(err);
}


BFileGameSound::BFileGameSound(BDataIO* data, bool looping,
	BGameSoundDevice* device)
	:
	BStreamingGameSound(device),
	fAudioStream(NULL),
	fStopping(false),
	fLooping(looping),
	fReserved(0),
	fBuffer(NULL),
	fFrameSize(0),
	fBufferSize(0),
	fPlayPosition(0),
	fPausing(NULL),
	fPaused(false),
	fPauseGain(1.0f),
	fDataSource(data)
{
	if (InitCheck() == B_OK)
		SetInitError(Init(data));
}


BFileGameSound::~BFileGameSound()
{
}


BGameSound*
BFileGameSound::Clone() const
{
	return NULL;
}


status_t
BFileGameSound::StartPlaying()
{
	return BGameSound::StartPlaying();
}


status_t
BFileGameSound::StopPlaying()
{
	status_t error = BGameSound::StopPlaying();

	return error;
}


status_t
BFileGameSound::Preload()
{
	return B_OK;
}


void
BFileGameSound::FillBuffer(void* inBuffer, size_t inByteCount)
{
	(void)inBuffer;
	(void)inByteCount;
}


status_t
BFileGameSound::Perform(int32 selector, void* data)
{
	(void)selector;
	(void)data;
	return B_ERROR;
}


status_t
BFileGameSound::SetPaused(bool isPaused, bigtime_t rampTime)
{
	(void)rampTime;
	if (fPaused == isPaused)
		return EALREADY;
	fPaused = isPaused;
	return B_OK;
}


int32
BFileGameSound::IsPaused()
{
	if (fPaused)
		return B_PAUSED;

	return B_NOT_PAUSED;
}


status_t
BFileGameSound::Init(BDataIO* data)
{
	if (data == NULL)
		return B_NO_MEMORY;

	std::vector<uint8> encoded;
	const size_t kChunkSize = 8192;
	encoded.reserve(kChunkSize);

	while (true) {
		size_t oldSize = encoded.size();
		encoded.resize(oldSize + kChunkSize);
		ssize_t bytesRead = data->Read(encoded.data() + oldSize, kChunkSize);
		if (bytesRead < 0)
			return (status_t)bytesRead;
		if (bytesRead == 0) {
			encoded.resize(oldSize);
			break;
		}
		encoded.resize(oldSize + (size_t)bytesRead);
	}

	if (encoded.empty())
		return B_BAD_DATA;

	std::vector<float> samples;
	int64 frames = 0;
	uint32 channels = 0;
	float sampleRate = 0;
	status_t err = BPrivate::GameAudio::DecodeMemoryToFloat32(encoded.data(),
		encoded.size(), samples, frames, channels, sampleRate);
	if (err != B_OK)
		return err;

	gs_id sound;
	err = init_sound_from_samples(this, samples, frames, channels, sampleRate,
		&sound);
	if (err != B_OK)
		return err;

	err = BGameSound::Init(sound);
	if (err != B_OK)
		return err;

	gs_attribute attribute;
	attribute.attribute = B_GS_LOOPING;
	attribute.duration = 0;
	attribute.value = fLooping ? 1.0f : 0.0f;
	attribute.flags = 0;
	return SetAttributes(&attribute, 1);
}


bool
BFileGameSound::Load()
{
	return false;
}


bool
BFileGameSound::Read(void* buffer, size_t bytes)
{
	return false;
}


/* unimplemented for protection of the user:
 *
 * BFileGameSound::BFileGameSound()
 * BFileGameSound::BFileGameSound(const BFileGameSound &)
 * BFileGameSound &BFileGameSound::operator=(const BFileGameSound &)
 */


status_t
BFileGameSound::_Reserved_BFileGameSound_0(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BFileGameSound::_Reserved_BFileGameSound_1(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BFileGameSound::_Reserved_BFileGameSound_2(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BFileGameSound::_Reserved_BFileGameSound_3(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BFileGameSound::_Reserved_BFileGameSound_4(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BFileGameSound::_Reserved_BFileGameSound_5(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BFileGameSound::_Reserved_BFileGameSound_6(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BFileGameSound::_Reserved_BFileGameSound_7(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BFileGameSound::_Reserved_BFileGameSound_8(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BFileGameSound::_Reserved_BFileGameSound_9(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BFileGameSound::_Reserved_BFileGameSound_10(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BFileGameSound::_Reserved_BFileGameSound_11(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BFileGameSound::_Reserved_BFileGameSound_12(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BFileGameSound::_Reserved_BFileGameSound_13(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BFileGameSound::_Reserved_BFileGameSound_14(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BFileGameSound::_Reserved_BFileGameSound_15(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BFileGameSound::_Reserved_BFileGameSound_16(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BFileGameSound::_Reserved_BFileGameSound_17(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BFileGameSound::_Reserved_BFileGameSound_18(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BFileGameSound::_Reserved_BFileGameSound_19(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BFileGameSound::_Reserved_BFileGameSound_20(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BFileGameSound::_Reserved_BFileGameSound_21(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BFileGameSound::_Reserved_BFileGameSound_22(int32 arg, ...)
{
	return B_ERROR;
}


status_t
BFileGameSound::_Reserved_BFileGameSound_23(int32 arg, ...)
{
	return B_ERROR;
}
