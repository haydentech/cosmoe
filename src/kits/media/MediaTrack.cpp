#include <MediaTrack.h>

#include "MediaFilePrivate.h"

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


BMediaTrack::~BMediaTrack()
{
}


status_t
BMediaTrack::InitCheck() const
{
	return fInitStatus;
}


status_t
BMediaTrack::GetCodecInfo(media_codec_info* codecInfo) const
{
	if (codecInfo == NULL)
		return B_BAD_VALUE;
	if (fInitStatus != B_OK)
		return fInitStatus;

	*codecInfo = fCodecInfo;
	return B_OK;
}


status_t
BMediaTrack::EncodedFormat(media_format* format) const
{
	if (format == NULL)
		return B_BAD_VALUE;
	if (fInitStatus != B_OK)
		return fInitStatus;

	*format = fFormat;
	return B_OK;
}


status_t
BMediaTrack::DecodedFormat(media_format* format, uint32 flags)
{
	(void)flags;
	if (format == NULL)
		return B_BAD_VALUE;
	if (fInitStatus != B_OK)
		return fInitStatus;

	if (format->type != B_MEDIA_NO_TYPE && format->type != B_MEDIA_RAW_AUDIO)
		return B_MEDIA_BAD_FORMAT;

	*format = fFormat;
	return B_OK;
}


int64
BMediaTrack::CountFrames() const
{
	if (fInitStatus != B_OK || fExtractor == NULL)
		return 0;

	const BPrivate::media::DecodedTrackInfo* info = fExtractor->TrackInfoAt(fStream);
	return info != NULL ? info->frameCount : 0;
}


bigtime_t
BMediaTrack::Duration() const
{
	if (fInitStatus != B_OK || fExtractor == NULL)
		return 0;

	const BPrivate::media::DecodedTrackInfo* info = fExtractor->TrackInfoAt(fStream);
	return info != NULL ? info->duration : 0;
}


status_t
BMediaTrack::GetMetaData(BMessage* data) const
{
	(void)data;
	return fInitStatus == B_OK ? B_UNSUPPORTED : fInitStatus;
}


int64
BMediaTrack::CurrentFrame() const
{
	return fCurrentFrame;
}


bigtime_t
BMediaTrack::CurrentTime() const
{
	return fCurrentTime;
}


status_t
BMediaTrack::ReadFrames(void* buffer, int64* frameCount, media_header* header)
{
	return ReadFrames(buffer, frameCount, header, NULL);
}


status_t
BMediaTrack::ReadFrames(void* buffer, int64* frameCount, media_header* header,
	media_decode_info* info)
{
	if (buffer == NULL || frameCount == NULL)
		return B_BAD_VALUE;
	if (fInitStatus != B_OK)
		return fInitStatus;

	const BPrivate::media::DecodedTrackInfo* track = fExtractor->TrackInfoAt(fStream);
	if (track == NULL)
		return B_BAD_INDEX;

	int64 requested = *frameCount;
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
	*frameCount = toRead;
	clear_header(header, fFormat, fCurrentFrame, toRead);
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
BMediaTrack::ReplaceFrames(const void* buffer, int64* frameCount,
	const media_header* header)
{
	(void)buffer;
	(void)frameCount;
	(void)header;
	return B_UNSUPPORTED;
}


status_t
BMediaTrack::SeekToTime(bigtime_t* time, int32 flags)
{
	(void)flags;
	if (time == NULL)
		return B_BAD_VALUE;
	if (fInitStatus != B_OK)
		return fInitStatus;

	bigtime_t requested = std::max<bigtime_t>(0, *time);
	bigtime_t duration = Duration();
	if (requested > duration)
		requested = duration;

	fCurrentFrame = (int64)((requested * _FrameRate()) / 1000000.0);
	if (fCurrentFrame > CountFrames())
		fCurrentFrame = CountFrames();
	fCurrentTime = (bigtime_t)((fCurrentFrame * 1000000.0) / _FrameRate());
	*time = fCurrentTime;
	return B_OK;
}


status_t
BMediaTrack::SeekToFrame(int64* frame, int32 flags)
{
	(void)flags;
	if (frame == NULL)
		return B_BAD_VALUE;
	if (fInitStatus != B_OK)
		return fInitStatus;

	int64 requested = std::max<int64>(0, *frame);
	if (requested > CountFrames())
		requested = CountFrames();

	fCurrentFrame = requested;
	fCurrentTime = (bigtime_t)((fCurrentFrame * 1000000.0) / _FrameRate());
	*frame = fCurrentFrame;
	return B_OK;
}


status_t
BMediaTrack::FindKeyFrameForTime(bigtime_t* time, int32 flags) const
{
	(void)flags;
	if (time == NULL)
		return B_BAD_VALUE;
	if (fInitStatus != B_OK)
		return fInitStatus;

	if (*time < 0)
		*time = 0;
	if (*time > Duration())
		*time = Duration();
	return B_OK;
}


status_t
BMediaTrack::FindKeyFrameForFrame(int64* frame, int32 flags) const
{
	(void)flags;
	if (frame == NULL)
		return B_BAD_VALUE;
	if (fInitStatus != B_OK)
		return fInitStatus;

	if (*frame < 0)
		*frame = 0;
	if (*frame > CountFrames())
		*frame = CountFrames();
	return B_OK;
}


status_t
BMediaTrack::ReadChunk(char** buffer, int32* size, media_header* header)
{
	(void)buffer;
	(void)size;
	(void)header;
	return B_UNSUPPORTED;
}


status_t
BMediaTrack::AddCopyright(const char* copyright)
{
	(void)copyright;
	return B_UNSUPPORTED;
}


status_t
BMediaTrack::AddTrackInfo(uint32 code, const void* data, size_t size,
	uint32 flags)
{
	(void)code;
	(void)data;
	(void)size;
	(void)flags;
	return B_UNSUPPORTED;
}


status_t
BMediaTrack::WriteFrames(const void* data, int32 frameCount, int32 flags)
{
	(void)data;
	(void)frameCount;
	(void)flags;
	return B_UNSUPPORTED;
}


status_t
BMediaTrack::WriteFrames(const void* data, int64 frameCount,
	media_encode_info* info)
{
	(void)data;
	(void)frameCount;
	(void)info;
	return B_UNSUPPORTED;
}


status_t
BMediaTrack::WriteChunk(const void* data, size_t size, uint32 flags)
{
	(void)data;
	(void)size;
	(void)flags;
	return B_UNSUPPORTED;
}


status_t
BMediaTrack::WriteChunk(const void* data, size_t size,
	media_encode_info* info)
{
	(void)data;
	(void)size;
	(void)info;
	return B_UNSUPPORTED;
}


status_t
BMediaTrack::Flush()
{
	return B_UNSUPPORTED;
}


status_t
BMediaTrack::GetParameterWeb(BParameterWeb** web)
{
	if (web != NULL)
		*web = NULL;
	return B_UNSUPPORTED;
}


status_t
BMediaTrack::GetParameterValue(int32 id, void* value, size_t* size)
{
	(void)id;
	(void)value;
	(void)size;
	return B_UNSUPPORTED;
}


status_t
BMediaTrack::SetParameterValue(int32 id, const void* value, size_t size)
{
	(void)id;
	(void)value;
	(void)size;
	return B_UNSUPPORTED;
}


BView*
BMediaTrack::GetParameterView()
{
	return NULL;
}


status_t
BMediaTrack::GetQuality(float* quality)
{
	if (quality != NULL)
		*quality = 0.0f;
	return B_UNSUPPORTED;
}


status_t
BMediaTrack::SetQuality(float quality)
{
	(void)quality;
	return B_UNSUPPORTED;
}


status_t
BMediaTrack::GetEncodeParameters(encode_parameters* parameters) const
{
	if (parameters != NULL)
		std::memset(parameters, 0, sizeof(*parameters));
	return B_UNSUPPORTED;
}


status_t
BMediaTrack::SetEncodeParameters(encode_parameters* parameters)
{
	(void)parameters;
	return B_UNSUPPORTED;
}


status_t
BMediaTrack::Perform(int32 code, void* data)
{
	(void)code;
	(void)data;
	return B_ERROR;
}


BParameterWeb*
BMediaTrack::Web()
{
	return NULL;
}


status_t
BMediaTrack::ControlCodec(int32 selector, void* inOutData, size_t size)
{
	(void)selector;
	(void)inOutData;
	(void)size;
	return B_ERROR;
}


void
BMediaTrack::SetupWorkaround()
{
}


bool
BMediaTrack::SetupFormatTranslation(const media_format& from, media_format* to)
{
	if (to == NULL)
		return false;
	*to = from;
	return true;
}


double
BMediaTrack::_FrameRate() const
{
	if (fFormat.type != B_MEDIA_RAW_AUDIO || fFormat.u.raw_audio.frame_rate <= 0)
		return 1.0;
	return fFormat.u.raw_audio.frame_rate;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_0(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_1(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_2(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_3(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_4(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_5(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_6(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_7(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_8(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_9(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_10(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_11(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_12(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_13(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_14(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_15(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_16(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_17(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_18(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_19(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_20(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_21(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_22(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_23(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_24(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_25(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_26(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_27(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_28(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_29(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_30(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_31(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_32(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_33(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_34(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_35(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_36(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_37(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_38(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_39(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_40(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_41(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_42(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_43(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_44(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_45(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_46(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}


status_t
BMediaTrack::_Reserved_BMediaTrack_47(int32 arg, ...)
{
	(void)arg;
	return B_ERROR;
}