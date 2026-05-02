/*
 * Copyright 2026, Bill Hayden
 * Distributed under the terms of the MIT License.
 */

#include <Sound.h>

#include <Entry.h>
#include <Path.h>

#include <third_party/miniaudio/miniaudio.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <vector>


namespace {

size_t
bytes_per_sample(uint32 format)
{
	switch (format) {
		case media_raw_audio_format::B_AUDIO_CHAR:
		case media_raw_audio_format::B_AUDIO_UCHAR:
			return sizeof(int8);
		case media_raw_audio_format::B_AUDIO_SHORT:
			return sizeof(int16);
		case media_raw_audio_format::B_AUDIO_INT:
			return sizeof(int32);
		case media_raw_audio_format::B_AUDIO_FLOAT:
			return sizeof(float);
		case media_raw_audio_format::B_AUDIO_DOUBLE:
			return sizeof(double);
		default:
			return 0;
	}
}


} // namespace


BSound::BSound(void* data, size_t size, const media_raw_audio_format& format,
	bool freeWhenDone)
	:	fData(data),
		fDataSize(size),
		fFile(NULL),
		fRefCount(1),
		fStatus(B_OK),
		fFormat(format),
		fFreeWhenDone(freeWhenDone),
		fReserved{false, false, false},
		fTrackReader(NULL),
		fReserved2{0}
{
	if (data == NULL || size == 0)
		fStatus = B_BAD_VALUE;
}


BSound::BSound(const entry_ref* soundFile, bool loadIntoMemory)
	:	fData(NULL),
		fDataSize(0),
		fFile(NULL),
		fRefCount(1),
		fStatus(B_OK),
		fFormat(media_raw_audio_format::wildcard),
		fFreeWhenDone(true),
		fReserved{false, false, false},
		fTrackReader(NULL),
		fReserved2{0}
{
	(void)loadIntoMemory;

	if (soundFile == NULL || soundFile->name == NULL) {
		fStatus = B_BAD_VALUE;
		return;
	}

	BEntry entry(soundFile);
	BPath path;
	if (entry.InitCheck() != B_OK || entry.GetPath(&path) != B_OK) {
		fStatus = B_ENTRY_NOT_FOUND;
		return;
	}

	ma_decoder decoder;
	if (ma_decoder_init_file(path.Path(), NULL, &decoder) != MA_SUCCESS) {
		fStatus = B_ERROR;
		return;
	}

	ma_uint64 totalFrames = 0;
	ma_decoder_get_length_in_pcm_frames(&decoder, &totalFrames);
	ma_uint32 channels = decoder.outputChannels;
	ma_uint32 sampleRate = decoder.outputSampleRate;

	if (totalFrames == 0 || channels == 0 || sampleRate == 0) {
		ma_decoder_uninit(&decoder);
		fStatus = B_ERROR;
		return;
	}

	std::vector<float> samples(totalFrames * channels);
	ma_uint64 framesRead = 0;
	if (ma_decoder_read_pcm_frames(&decoder, samples.data(), totalFrames,
			&framesRead) != MA_SUCCESS || framesRead == 0) {
		ma_decoder_uninit(&decoder);
		fStatus = B_ERROR;
		return;
	}

	ma_decoder_uninit(&decoder);

	fDataSize = (size_t)(framesRead * channels * sizeof(float));
	fData = std::malloc(fDataSize);
	if (fData == NULL) {
		fStatus = B_NO_MEMORY;
		return;
	}

	std::memcpy(fData, samples.data(), fDataSize);
	fFormat.frame_rate = sampleRate;
	fFormat.channel_count = channels;
	fFormat.format = media_raw_audio_format::B_AUDIO_FLOAT;
	fFormat.byte_order = B_MEDIA_HOST_ENDIAN;
	fFormat.buffer_size = fDataSize;
}


BSound::BSound(const media_raw_audio_format& format)
	:	fData(NULL),
		fDataSize(0),
		fFile(NULL),
		fRefCount(1),
		fStatus(B_OK),
		fFormat(format),
		fFreeWhenDone(false),
		fReserved{false, false, false},
		fTrackReader(NULL),
		fReserved2{0}
{
}


BSound::~BSound()
{
	if (fFreeWhenDone)
		std::free(fData);
}


status_t
BSound::InitCheck()
{
	return fStatus;
}


BSound*
BSound::AcquireRef()
{
	++fRefCount;
	return this;
}


bool
BSound::ReleaseRef()
{
	if (--fRefCount > 0)
		return false;

	delete this;
	return true;
}


int32
BSound::RefCount() const
{
	return fRefCount;
}


bigtime_t
BSound::Duration() const
{
	size_t sampleSize = bytes_per_sample(fFormat.format);
	if (sampleSize == 0 || fFormat.channel_count == 0 || fFormat.frame_rate <= 0)
		return 0;

	size_t frameSize = sampleSize * fFormat.channel_count;
	if (frameSize == 0)
		return 0;

	double frames = (double)fDataSize / (double)frameSize;
	return (bigtime_t)((frames * 1000000.0) / fFormat.frame_rate);
}


const media_raw_audio_format&
BSound::Format() const
{
	return fFormat;
}


const void*
BSound::Data() const
{
	return fData;
}


off_t
BSound::Size() const
{
	return (off_t)fDataSize;
}


bool
BSound::GetDataAt(off_t offset, void* intoBuffer, size_t bufferSize,
	size_t* outUsed)
{
	if (outUsed != NULL)
		*outUsed = 0;

	if (intoBuffer == NULL || fData == NULL || offset < 0
		|| (size_t)offset >= fDataSize)
		return false;

	size_t available = fDataSize - (size_t)offset;
	size_t toCopy = std::min(bufferSize, available);
	std::memcpy(intoBuffer, (const uint8*)fData + offset, toCopy);
	if (outUsed != NULL)
		*outUsed = toCopy;
	return true;
}


status_t
BSound::Perform(int32 code, ...)
{
	(void)code;
	return B_ERROR;
}


status_t
BSound::BindTo(BSoundPlayer* player, const media_raw_audio_format& format)
{
	(void)player;
	(void)format;
	return InitCheck();
}


status_t
BSound::UnbindFrom(BSoundPlayer* player)
{
	(void)player;
	return B_OK;
}


status_t
BSound::_Reserved_Sound_0(void*)
{
	return B_ERROR;
}


status_t
BSound::_Reserved_Sound_1(void*)
{
	return B_ERROR;
}


status_t
BSound::_Reserved_Sound_2(void*)
{
	return B_ERROR;
}


status_t
BSound::_Reserved_Sound_3(void*)
{
	return B_ERROR;
}


status_t
BSound::_Reserved_Sound_4(void*)
{
	return B_ERROR;
}


status_t
BSound::_Reserved_Sound_5(void*)
{
	return B_ERROR;
}