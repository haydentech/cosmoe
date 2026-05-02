/*
 * Copyright 2002-2007, Marcus Overhagen <marcus@overhagen.de>
 * Copyright 2009-2010, Stephan Aßmus <superstippi@gmx.de>
 * Copyright 2013, Haiku, Inc. All Rights Reserved.
 * Copyright 2026, Bill Hayden <hayden@haydentech.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * Authors:
 *		Stephan Aßmus, superstippi@gmx.de
 *		Marcus Overhagen, marcus@overhagen.de
 */


#include <MediaTrack.h>

#include "MediaFilePrivate.h"
#include <MediaDebug.h>

#include <Message.h>

#include <algorithm>
#include <cstring>


namespace {

static size_t
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


static size_t
frame_size(const media_format& format)
{
	if (format.type != B_MEDIA_RAW_AUDIO)
		return 0;
	return bytes_per_sample(format.u.raw_audio.format)
		* format.u.raw_audio.channel_count;
}


static void
clear_header(media_header* header, const media_format& format,
	int64 currentFrame, int64 frameCount)
{
	if (header == NULL)
		return;

	std::memset(header, 0, sizeof(*header));
	header->type = format.type;
	header->size_used = frameCount * frame_size(format);
	header->start_time = (bigtime_t)((currentFrame * 1000000.0)
		/ format.u.raw_audio.frame_rate);
	header->file_pos = currentFrame * frame_size(format);
	header->u.raw_audio.frame_rate = format.u.raw_audio.frame_rate;
	header->u.raw_audio.channel_count = format.u.raw_audio.channel_count;
}

} // namespace


BMediaTrack::BMediaTrack(BPrivate::media::MediaExtractor* extractor,
	int32 streamIndex)
	:	fInitStatus(B_NO_INIT),
		fDecoder(NULL),
		fRawDecoder(NULL),
		fExtractor(extractor),
		fStream(streamIndex),
		fCurrentFrame(0),
		fCurrentTime(0),
		fCodecInfo{},
		fEncoder(NULL),
		fEncoderID(-1),
		fWriter(NULL),
		fFormat(),
		fWorkaroundFlags(0)
{
	std::memset(_reserved_BMediaTrack_, 0, sizeof(_reserved_BMediaTrack_));

	if (extractor == NULL) {
		fInitStatus = B_BAD_VALUE;
		return;
	}

	const BPrivate::media::DecodedTrackInfo* info = extractor->TrackInfoAt(streamIndex);
	if (info == NULL) {
		fInitStatus = B_BAD_INDEX;
		return;
	}

	fFormat = info->format;
	fCodecInfo = info->codecInfo;
	fInitStatus = B_OK;
}


BMediaTrack::BMediaTrack(BPrivate::media::MediaWriter* writer,
	int32 streamIndex, media_format* format, const media_codec_info* codecInfo)
	:	fInitStatus(B_UNSUPPORTED),
		fDecoder(NULL),
		fRawDecoder(NULL),
		fExtractor(NULL),
		fStream(streamIndex),
		fCurrentFrame(0),
		fCurrentTime(0),
		fCodecInfo{},
		fEncoder(NULL),
		fEncoderID(-1),
		fWriter(writer),
		fFormat(),
		fWorkaroundFlags(0)
{
	if (format != NULL)
		fFormat = *format;
	if (codecInfo != NULL)
		fCodecInfo = *codecInfo;
	std::memset(_reserved_BMediaTrack_, 0, sizeof(_reserved_BMediaTrack_));
}


BMediaTrack::BMediaTrack()
	:	fInitStatus(B_NO_INIT),
		fDecoder(NULL),
		fRawDecoder(NULL),
		fExtractor(NULL),
		fStream(-1),
		fCurrentFrame(0),
		fCurrentTime(0),
		fCodecInfo{},
		fEncoder(NULL),
		fEncoderID(-1),
		fWriter(NULL),
		fFormat(),
		fWorkaroundFlags(0)
{
	std::memset(_reserved_BMediaTrack_, 0, sizeof(_reserved_BMediaTrack_));
}

/*************************************************************
 * protected BMediaTrack
 *************************************************************/

BMediaTrack::~BMediaTrack()
{
	CALLED();
}

/*************************************************************
 * public BMediaTrack
 *************************************************************/

status_t
BMediaTrack::InitCheck() const
{
	CALLED();

	return fInitStatus;
}


status_t
BMediaTrack::GetCodecInfo(media_codec_info* _codecInfo) const
{
	CALLED();
	if (_codecInfo == NULL)
		return B_BAD_VALUE;
	if (fExtractor == NULL)
		return B_NO_INIT;

	*_codecInfo = fCodecInfo;

	return B_OK;
}


status_t
BMediaTrack::EncodedFormat(media_format* _format) const
{
	CALLED();

	if (_format == NULL)
		return B_BAD_VALUE;

	if (fExtractor == NULL)
		return B_NO_INIT;

	const media_format* format = fExtractor->EncodedFormat(fStream);
	if (format == NULL)
		return B_BAD_INDEX;

	*_format = *format;
	return B_OK;
}


status_t
BMediaTrack::DecodedFormat(media_format* _format, uint32 flags)
{
	CALLED();
	if (_format == NULL)
		return B_BAD_VALUE;
	if (fExtractor == NULL)
		return B_NO_INIT;

	if (_format->type != B_MEDIA_NO_TYPE && _format->type != B_MEDIA_RAW_AUDIO)
		return B_MEDIA_BAD_FORMAT;

	media_format requested = *_format;
	*_format = fFormat;
	if ((flags & 0x4000) == 0 && requested.type == B_MEDIA_RAW_AUDIO
		&& requested.u.raw_audio.format != 0
		&& requested.u.raw_audio.format != _format->u.raw_audio.format) {
		if (SetupFormatTranslation(requested, &fFormat))
			*_format = fFormat;
	}
	fFormat = *_format;
	return B_OK;
}


status_t
BMediaTrack::GetMetaData(BMessage* _data) const
{
	CALLED();

	if (fExtractor == NULL)
		return B_NO_INIT;

	if (_data == NULL)
		return B_BAD_VALUE;

	_data->MakeEmpty();

	return fExtractor->GetStreamMetaData(fStream, _data);
}


int64
BMediaTrack::CountFrames() const
{
	CALLED();

	int64 frames = fExtractor != NULL ? fExtractor->CountFrames(fStream) : 0;
	return frames;
}


bigtime_t
BMediaTrack::Duration() const
{
	CALLED();

	bigtime_t duration = fExtractor != NULL ? fExtractor->Duration(fStream) : 0;
	return duration;
}


int64
BMediaTrack::CurrentFrame() const
{
	CALLED();
	return fCurrentFrame;
}


bigtime_t
BMediaTrack::CurrentTime() const
{
	return fCurrentTime;
}


status_t
BMediaTrack::ReadFrames(void* buffer, int64* _frameCount, media_header* header)
{
	return ReadFrames(buffer, _frameCount, header, NULL);
}


status_t
BMediaTrack::ReadFrames(void* buffer, int64* _frameCount,
	media_header* _header, media_decode_info* info)
{
	CALLED();
	if (buffer == NULL || _frameCount == NULL)
		return B_BAD_VALUE;
	if (fInitStatus != B_OK)
		return fInitStatus;

	const BPrivate::media::DecodedTrackInfo* track = fExtractor->TrackInfoAt(fStream);
	if (track == NULL)
		return B_BAD_INDEX;

	int64 requested = *_frameCount;
	if (requested < 0)
		return B_BAD_VALUE;
	if (requested == 0) {
		size_t bytesPerFrame = frame_size(fFormat);
		if (bytesPerFrame == 0)
			return B_MEDIA_BAD_FORMAT;

		requested = fFormat.u.raw_audio.buffer_size / bytesPerFrame;
		if (requested <= 0)
			requested = 1;
	}

	int64 available = std::max<int64>(0, track->frameCount - fCurrentFrame);
	int64 toRead = std::min(requested, available);
	*_frameCount = toRead;
	clear_header(_header, fFormat, fCurrentFrame, toRead);
	if (info != NULL)
		*info = media_decode_info();

	if (toRead == 0)
		return B_LAST_BUFFER_ERROR;

	size_t bytesPerFrame = frame_size(fFormat);
	std::memcpy(buffer, track->decodedData.data() + fCurrentFrame * bytesPerFrame,
		toRead * bytesPerFrame);
	fCurrentFrame += toRead;
	fCurrentTime = (bigtime_t)((fCurrentFrame * 1000000.0) / _FrameRate());
	return B_OK;
}


status_t
BMediaTrack::ReplaceFrames(const void* inBuffer, int64* _frameCount,
	const media_header* header)
{
	CALLED();
	(void)inBuffer;
	(void)_frameCount;
	(void)header;
	return B_UNSUPPORTED;
}


status_t
BMediaTrack::SeekToTime(bigtime_t* _time, int32 flags)
{
	CALLED();
	(void)flags;

	if (_time == NULL)
		return B_BAD_VALUE;
	if (fInitStatus != B_OK)
		return fInitStatus;

	bigtime_t requested = std::max<bigtime_t>(0, *_time);
	bigtime_t duration = Duration();
	if (requested > duration)
		requested = duration;

	fCurrentFrame = (int64)((requested * _FrameRate()) / 1000000.0);
	if (fCurrentFrame > CountFrames())
		fCurrentFrame = CountFrames();
	fCurrentTime = (bigtime_t)((fCurrentFrame * 1000000.0) / _FrameRate());
	*_time = fCurrentTime;
	return B_OK;
}


status_t
BMediaTrack::SeekToFrame(int64* _frame, int32 flags)
{
	CALLED();
	(void)flags;
	if (_frame == NULL)
		return B_BAD_VALUE;
	if (fInitStatus != B_OK)
		return fInitStatus;

	int64 requested = std::max<int64>(0, *_frame);
	if (requested > CountFrames())
		requested = CountFrames();

	fCurrentFrame = requested;
	fCurrentTime = (bigtime_t)((fCurrentFrame * 1000000.0) / _FrameRate());
	*_frame = fCurrentFrame;
	return B_OK;
}


status_t
BMediaTrack::FindKeyFrameForTime(bigtime_t* _time, int32 flags) const
{
	CALLED();

	(void)flags;

	if (_time == NULL)
		return B_BAD_VALUE;
	if (fInitStatus != B_OK)
		return fInitStatus;

	if (*_time < 0)
		*_time = 0;
	if (*_time > Duration())
		*_time = Duration();
	return B_OK;
}


status_t
BMediaTrack::FindKeyFrameForFrame(int64* _frame, int32 flags) const
{
	CALLED();

	(void)flags;

	if (_frame == NULL)
		return B_BAD_VALUE;
	if (fInitStatus != B_OK)
		return fInitStatus;

	if (*_frame < 0)
		*_frame = 0;
	if (*_frame > CountFrames())
		*_frame = CountFrames();
	return B_OK;
}


status_t
BMediaTrack::ReadChunk(char** _buffer, int32* _size, media_header* _header)
{
	CALLED();

	if (_buffer == NULL || _size == NULL)
		return B_BAD_VALUE;

	(void)_buffer;
	(void)_size;
	(void)_header;
	return B_UNSUPPORTED;
}


status_t
BMediaTrack::AddCopyright(const char* copyright)
{
	CALLED();
	(void)copyright;
	return B_UNSUPPORTED;
}


status_t
BMediaTrack::AddTrackInfo(uint32 code, const void* data, size_t size,
	uint32 flags)
{
	CALLED();
	(void)code;
	(void)data;
	(void)size;
	(void)flags;
	return B_UNSUPPORTED;
}


status_t
BMediaTrack::WriteFrames(const void* data, int32 frameCount, int32 flags)
{
	CALLED();
	(void)data;
	(void)frameCount;
	(void)flags;
	return B_UNSUPPORTED;
}


status_t
BMediaTrack::WriteFrames(const void* data, int64 frameCount,
	media_encode_info* info)
{
	CALLED();
	(void)data;
	(void)frameCount;
	(void)info;
	return B_UNSUPPORTED;
}


status_t
BMediaTrack::WriteChunk(const void* data, size_t size, uint32 flags)
{
	CALLED();
	(void)data;
	(void)size;
	(void)flags;
	return B_UNSUPPORTED;
}


status_t
BMediaTrack::WriteChunk(const void* data, size_t size,
	media_encode_info* info)
{
	CALLED();
	(void)data;
	(void)size;
	(void)info;
	return B_UNSUPPORTED;
}


status_t
BMediaTrack::Flush()
{
	CALLED();
	return B_UNSUPPORTED;
}


status_t
BMediaTrack::GetParameterWeb(BParameterWeb** outWeb)
{
	if (outWeb == NULL)
		return B_BAD_VALUE;

	if (outWeb!= NULL)
		*outWeb = NULL;
	return B_UNSUPPORTED;
}


status_t
BMediaTrack::GetParameterValue(int32 id, void* value, size_t* size)
{
	if (value == NULL || size == NULL)
		return B_BAD_VALUE;
	(void)id;

	return B_UNSUPPORTED;
}


status_t
BMediaTrack::SetParameterValue(int32 id, const void* value, size_t size)
{
	if (value == NULL || size == 0)
		return B_BAD_VALUE;

	(void)id;

	return B_UNSUPPORTED;
}


BView*
BMediaTrack::GetParameterView()
{
	CALLED();
	return NULL;
}


status_t
BMediaTrack::GetQuality(float* quality)
{
	if (quality == NULL)
		return B_BAD_VALUE;

	if (quality != NULL)
		*quality = 0.0f;
	return B_UNSUPPORTED;
}


status_t
BMediaTrack::SetQuality(float quality)
{
	CALLED();
	(void)quality;
	return B_UNSUPPORTED;
}


status_t
BMediaTrack::GetEncodeParameters(encode_parameters* parameters) const
{
	if (parameters == NULL)
		return B_BAD_VALUE;

	std::memset(parameters, 0, sizeof(*parameters));
	return B_UNSUPPORTED;
}


status_t
BMediaTrack::SetEncodeParameters(encode_parameters* parameters)
{
	if (parameters == NULL)
		return B_BAD_VALUE;

	return B_UNSUPPORTED;
}


status_t
BMediaTrack::Perform(int32 selector, void* data)
{
	(void)selector;
	(void)data;
	return B_ERROR;
}

// #pragma mark - private


BParameterWeb*
BMediaTrack::Web()
{
	CALLED();
	return NULL;
}


// Does nothing, returns B_ERROR, for Zeta compatiblity only
status_t
BMediaTrack::ControlCodec(int32 selector, void* io_data, size_t size)
{
	(void)selector;
	(void)io_data;
	(void)size;
	return B_ERROR;
}


void
BMediaTrack::SetupWorkaround()
{
	CALLED();
}


bool
BMediaTrack::SetupFormatTranslation(const media_format &from, media_format* to)
{
	CALLED();
	if (to == NULL)
		return false;
	*to = from;
	return true;
}


double
BMediaTrack::_FrameRate() const
{
	switch (fFormat.type) {
		case B_MEDIA_RAW_VIDEO:
			return fFormat.u.raw_video.field_rate;
		case B_MEDIA_ENCODED_VIDEO:
			return fFormat.u.encoded_video.output.field_rate;
		case B_MEDIA_RAW_AUDIO:
			return fFormat.u.raw_audio.frame_rate;
		case B_MEDIA_ENCODED_AUDIO:
			return fFormat.u.encoded_audio.output.frame_rate;
		default:
			return 1.0;
	}
}

#if 0
// unimplemented
BMediaTrack::BMediaTrack()
BMediaTrack::BMediaTrack(const BMediaTrack &)
BMediaTrack &BMediaTrack::operator=(const BMediaTrack &)
#endif

status_t BMediaTrack::_Reserved_BMediaTrack_0(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_1(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_2(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_3(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_4(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_5(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_6(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_7(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_8(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_9(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_10(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_11(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_12(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_13(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_14(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_15(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_16(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_17(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_18(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_19(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_20(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_21(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_22(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_23(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_24(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_25(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_26(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_27(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_28(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_29(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_30(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_31(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_32(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_33(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_34(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_35(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_36(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_37(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_38(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_39(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_40(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_41(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_42(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_43(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_44(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_45(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_46(int32 arg, ...) { return B_ERROR; }
status_t BMediaTrack::_Reserved_BMediaTrack_47(int32 arg, ...) { return B_ERROR; }
