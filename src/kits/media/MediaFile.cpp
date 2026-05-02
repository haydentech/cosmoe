/*
 * Copyright 2009, Stephan Aßmus <superstippi@gmx.de>
 * Copyright 2002-2004, Marcus Overhagen <marcus@overhagen.de>
 * Copyright 2026, Bill Hayden <hayden@haydentech.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include <MediaFile.h>

#include <new>

#include <stdlib.h>
#include <string.h>

#include "MediaFilePrivate.h"

#include <DataIO.h>
#include <Entry.h>
#include <File.h>
#include <MediaTrack.h>
#include <Message.h>
#include <Path.h>
#include <String.h>
#include <Url.h>

#include <MediaDebug.h>

#include <third_party/miniaudio/miniaudio.h>

#include <algorithm>
#include <cctype>
#include <cstring>

#include <vector>


namespace {

static status_t
read_all(BDataIO* source, std::vector<uint8>& data)
{
	if (source == NULL)
		return B_BAD_VALUE;

	uint8 buffer[16384];
	for (;;) {
		ssize_t bytesRead = source->Read(buffer, sizeof(buffer));
		if (bytesRead == 0)
			return data.empty() ? B_ERROR : B_OK;
		if (bytesRead < 0)
			return (status_t)bytesRead;

		data.insert(data.end(), buffer, buffer + bytesRead);
	}
}


static void
set_string_field(char* field, size_t size, const char* value)
{
	if (field == NULL || size == 0)
		return;

	field[0] = '\0';
	if (value == NULL)
		return;

	std::strncpy(field, value, size - 1);
	field[size - 1] = '\0';
}


static const char*
file_extension(const char* sourceName)
{
	if (sourceName == NULL)
		return NULL;

	const char* dot = std::strrchr(sourceName, '.');
	if (dot == NULL || dot[1] == '\0')
		return NULL;
	return dot + 1;
}


static bool
extension_equals(const char* extension, const char* expected)
{
	if (extension == NULL || expected == NULL)
		return false;

	for (;;) {
		char a = std::tolower((unsigned char)*extension);
		char b = std::tolower((unsigned char)*expected);
		if (a != b)
			return false;
		if (a == '\0')
			return true;
		extension++;
		expected++;
	}
}


static void
fill_audio_format(media_format& format, ma_uint32 channels,
	ma_uint32 sampleRate, size_t frameCount)
{
	const size_t kPreferredBufferFrames = 1024;
	size_t bufferFrames = std::min(frameCount, kPreferredBufferFrames);
	if (bufferFrames == 0)
		bufferFrames = 1;

	format.Clear();
	format.type = B_MEDIA_RAW_AUDIO;
	format.u.raw_audio = media_multi_audio_format::wildcard;
	format.u.raw_audio.frame_rate = sampleRate;
	format.u.raw_audio.channel_count = channels;
	format.u.raw_audio.format = media_raw_audio_format::B_AUDIO_FLOAT;
	format.u.raw_audio.byte_order = B_MEDIA_HOST_ENDIAN;
	format.u.raw_audio.buffer_size = bufferFrames * channels * sizeof(float);
	format.require_flags = 0;
	format.deny_flags = 0;
}


static void
fill_encoded_audio_format(media_format& format, const media_format& decoded,
	const char* sourceName)
{
	format.Clear();

	const char* extension = file_extension(sourceName);
	if (extension_equals(extension, "wav") || extension_equals(extension, "aif")
		|| extension_equals(extension, "aiff")) {
		format = decoded;
		return;
	}

	format.type = B_MEDIA_ENCODED_AUDIO;
	std::memset(&format.u.encoded_audio, 0, sizeof(format.u.encoded_audio));
	format.u.encoded_audio.output = decoded.u.raw_audio;
	format.u.encoded_audio.encoding = media_encoded_audio_format::B_ANY;
	format.require_flags = 0;
	format.deny_flags = 0;
}

} // namespace


namespace BPrivate { namespace media {

MediaExtractor::MediaExtractor()
	:	fInitStatus(B_NO_INIT),
		fFileFormat{}
{
}


status_t
MediaExtractor::InitFromDataIO(BDataIO* source, const char* sourceName)
{
	std::vector<uint8> encoded;
	status_t status = read_all(source, encoded);
	if (status != B_OK)
		return fInitStatus = status;

	return fInitStatus = _DecodeMemory(encoded.data(), encoded.size(), sourceName);
}


status_t
MediaExtractor::InitCheck() const
{
	return fInitStatus;
}


int32
MediaExtractor::CountTracks() const
{
	return (int32)fTracks.size();
}


void
MediaExtractor::GetFileFormatInfo(media_file_format* fileFormat) const
{
	if (fileFormat != NULL)
		*fileFormat = fFileFormat;
}


status_t
MediaExtractor::GetMetaData(BMessage* data) const
{
	if (data == NULL)
		return B_BAD_VALUE;

	*data = fMetaData;
	return B_OK;
}


status_t
MediaExtractor::GetStreamMetaData(int32 index, BMessage* data) const
{
	if (data == NULL)
		return B_BAD_VALUE;

	const DecodedTrackInfo* track = TrackInfoAt(index);
	if (track == NULL)
		return B_BAD_INDEX;

	*data = track->metaData;
	return B_OK;
}


const media_format*
MediaExtractor::EncodedFormat(int32 index) const
{
	const DecodedTrackInfo* track = TrackInfoAt(index);
	return track != NULL ? &track->encodedFormat : NULL;
}


int64
MediaExtractor::CountFrames(int32 index) const
{
	const DecodedTrackInfo* track = TrackInfoAt(index);
	return track != NULL ? track->frameCount : 0;
}


bigtime_t
MediaExtractor::Duration(int32 index) const
{
	const DecodedTrackInfo* track = TrackInfoAt(index);
	return track != NULL ? track->duration : 0;
}


const DecodedTrackInfo*
MediaExtractor::TrackInfoAt(int32 index) const
{
	if (index < 0 || index >= (int32)fTracks.size())
		return NULL;
	return &fTracks[index];
}


const media_file_format&
MediaExtractor::FileFormat() const
{
	return fFileFormat;
}


const char*
MediaExtractor::Copyright() const
{
	return NULL;
}


status_t
MediaExtractor::_DecodeMemory(const void* data, size_t size,
	const char* sourceName)
{
	if (data == NULL || size == 0)
		return B_BAD_VALUE;

	ma_decoder decoder;
	if (ma_decoder_init_memory(data, size, NULL, &decoder) != MA_SUCCESS)
		return B_ERROR;

	ma_uint64 frameCount = 0;
	ma_result lengthResult = ma_decoder_get_length_in_pcm_frames(&decoder,
		&frameCount);
	if (lengthResult != MA_SUCCESS || frameCount == 0
		|| decoder.outputChannels == 0 || decoder.outputSampleRate == 0) {
		ma_decoder_uninit(&decoder);
		return B_ERROR;
	}

	DecodedTrackInfo track{};
	track.decodedData.resize(frameCount * decoder.outputChannels * sizeof(float));
	ma_uint64 framesRead = 0;
	ma_result readResult = ma_decoder_read_pcm_frames(&decoder,
		track.decodedData.data(), frameCount, &framesRead);
	ma_decoder_uninit(&decoder);
	if (readResult != MA_SUCCESS || framesRead == 0)
		return B_ERROR;

	track.decodedData.resize(framesRead * decoder.outputChannels * sizeof(float));
	track.frameCount = (int64)framesRead;
	track.duration = (bigtime_t)((framesRead * 1000000.0)
		/ decoder.outputSampleRate);
	fill_audio_format(track.format, decoder.outputChannels,
		decoder.outputSampleRate, framesRead);
	fill_encoded_audio_format(track.encodedFormat, track.format, sourceName);
	std::memset(&track.codecInfo, 0, sizeof(track.codecInfo));
	set_string_field(track.codecInfo.pretty_name,
		sizeof(track.codecInfo.pretty_name), "miniaudio decoder");
	set_string_field(track.codecInfo.short_name,
		sizeof(track.codecInfo.short_name), "miniaudio");

	fTracks.clear();
	fTracks.push_back(track);
	_InitFileFormat(sourceName);
	return B_OK;
}


void
MediaExtractor::_InitFileFormat(const char* sourceName)
{
	std::memset(&fFileFormat, 0, sizeof(fFileFormat));
	fFileFormat.capabilities = media_file_format::B_READABLE
		| media_file_format::B_PERFECTLY_SEEKABLE
		| media_file_format::B_KNOWS_RAW_AUDIO;
	fFileFormat.family = B_MISC_FORMAT_FAMILY;
	fFileFormat.version = 100;
	set_string_field(fFileFormat.mime_type, sizeof(fFileFormat.mime_type),
		"audio/*");
	set_string_field(fFileFormat.pretty_name,
		sizeof(fFileFormat.pretty_name), "Decoded Audio File");
	set_string_field(fFileFormat.short_name, sizeof(fFileFormat.short_name),
		"audio");
	set_string_field(fFileFormat.file_extension,
		sizeof(fFileFormat.file_extension), "");

	const char* extension = file_extension(sourceName);
	if (extension_equals(extension, "wav")) {
		fFileFormat.family = B_WAV_FORMAT_FAMILY;
		set_string_field(fFileFormat.mime_type, sizeof(fFileFormat.mime_type),
			"audio/wav");
		set_string_field(fFileFormat.pretty_name,
			sizeof(fFileFormat.pretty_name), "WAV Audio File");
		set_string_field(fFileFormat.short_name,
			sizeof(fFileFormat.short_name), "wav");
		set_string_field(fFileFormat.file_extension,
			sizeof(fFileFormat.file_extension), "wav");
	} else if (extension_equals(extension, "aif")
		|| extension_equals(extension, "aiff")) {
		fFileFormat.family = B_AIFF_FORMAT_FAMILY;
		set_string_field(fFileFormat.mime_type, sizeof(fFileFormat.mime_type),
			"audio/aiff");
		set_string_field(fFileFormat.pretty_name,
			sizeof(fFileFormat.pretty_name), "AIFF Audio File");
		set_string_field(fFileFormat.short_name,
			sizeof(fFileFormat.short_name), "aiff");
		set_string_field(fFileFormat.file_extension,
			sizeof(fFileFormat.file_extension), "aiff");
	} else if (extension_equals(extension, "mp3")) {
		fFileFormat.family = B_MPEG_FORMAT_FAMILY;
		set_string_field(fFileFormat.mime_type, sizeof(fFileFormat.mime_type),
			"audio/mpeg");
		set_string_field(fFileFormat.pretty_name,
			sizeof(fFileFormat.pretty_name), "MPEG Audio File");
		set_string_field(fFileFormat.short_name,
			sizeof(fFileFormat.short_name), "mpeg");
		set_string_field(fFileFormat.file_extension,
			sizeof(fFileFormat.file_extension), "mp3");
	}
}

} }


BMediaFile::BMediaFile()
{
	CALLED();
	_Init();
}


BMediaFile::BMediaFile(const entry_ref* ref)
{
	CALLED();
	_Init();
	SetTo(ref);
}


BMediaFile::BMediaFile(BDataIO* source)
{
	CALLED();
	_Init();
	SetTo(source);
}


BMediaFile::BMediaFile(const entry_ref* ref, int32 flags)
{
	CALLED();
	_Init();
	fDeleteSource = true;
	_InitReader(new(std::nothrow) BFile(ref, O_RDONLY), NULL, flags);
}


BMediaFile::BMediaFile(BDataIO* source, int32 flags)
{
	CALLED();
	_Init();
	_InitReader(source, NULL, flags);
}


BMediaFile::BMediaFile(const entry_ref* ref, const media_file_format* mfi,
	int32 flags)
{
	CALLED();
	_Init();
	(void)ref;
	(void)flags;
	if (mfi != NULL)
		fMFI = *mfi;
	fErr = B_UNSUPPORTED;
}


BMediaFile::BMediaFile(BDataIO* destination, const media_file_format* mfi,
	int32 flags)
{
	CALLED();
	_Init();
	(void)destination;
	(void)flags;
	if (mfi != NULL)
		fMFI = *mfi;
	fErr = B_UNSUPPORTED;
}


// File will be set later by SetTo()
BMediaFile::BMediaFile(const media_file_format* mfi, int32 flags)
{
	CALLED();
	_Init();
	(void)flags;
	if (mfi != NULL)
		fMFI = *mfi;
	fErr = B_UNSUPPORTED;
}


BMediaFile::BMediaFile(const BUrl& url)
{
	CALLED();
	_Init();
	SetTo(url);
}


BMediaFile::BMediaFile(const BUrl& url, int32 flags)
{
	CALLED();
	_Init();
	_InitReader(NULL, &url, flags);
}


BMediaFile::BMediaFile(const BUrl& destination,
	const media_file_format* mfi, int32 flags)
{
	CALLED();
	_Init();
	(void)destination;
	(void)flags;
	if (mfi != NULL)
		fMFI = *mfi;
	fErr = B_UNSUPPORTED;
}


BMediaFile::~BMediaFile()
{
	CALLED();
	_UnInit();
}


status_t
BMediaFile::SetTo(const entry_ref* ref)
{
	CALLED();

	if (ref == NULL)
		return fErr = B_BAD_VALUE;

	_UnInit();
	_Init();

	BFile* file = new(std::nothrow) BFile(ref, B_READ_ONLY);
	if (file == NULL)
		return fErr = B_NO_MEMORY;
	if (file->InitCheck() != B_OK) {
		status_t status = file->InitCheck();
		delete file;
		return fErr = status;
	}

	fSource = file;
	fDeleteSource = true;
	_InitReader(file);
	return fErr;
}


status_t
BMediaFile::SetTo(BDataIO* destination)
{
	CALLED();
	_UnInit();
	_Init();
	_InitReader(destination);

	return fErr;
}


status_t
BMediaFile::SetTo(const BUrl& url)
{
	CALLED();
	_UnInit();
	_Init();
	_InitReader(NULL, &url);

	return fErr;
}


status_t
BMediaFile::InitCheck() const
{
	CALLED();
	return fErr;
}


status_t
BMediaFile::GetFileFormatInfo(media_file_format* mfi) const
{
	CALLED();
	if (mfi == NULL)
		return B_BAD_VALUE;
	if (fErr)
		return fErr;
	*mfi = fMFI;
	return B_OK;
}


status_t
BMediaFile::GetMetaData(BMessage* _data) const
{
	if (fExtractor == NULL)
		return B_NO_INIT;
	if (_data == NULL)
		return B_BAD_VALUE;

	_data->MakeEmpty();

	return fExtractor->GetMetaData(_data);
}


const char*
BMediaFile::Copyright() const
{
	return fExtractor != NULL ? fExtractor->Copyright() : NULL;
}


int32
BMediaFile::CountTracks() const
{
	return fTrackNum;
}


// Can be called multiple times with the same index.  You must call
// ReleaseTrack() when you're done with a track.
BMediaTrack*
BMediaFile::TrackAt(int32 index)
{
	CALLED();
	if (fExtractor == NULL
		|| index < 0 || index >= fTrackNum) {
		return NULL;
	}
	if (fTrackList[index] == NULL) {
		TRACE("BMediaFile::TrackAt, creating new track for index %"
			B_PRId32 "\n", index);
		fTrackList[index] = new(std::nothrow) BMediaTrack(fExtractor, index);
		TRACE("BMediaFile::TrackAt, new track is %p\n", fTrackList[index]);
	}
	return fTrackList[index];
}


// Release the resource used by a given BMediaTrack object, to reduce
// the memory usage of your application. The specific 'track' object
// can no longer be used, but you can create another one by calling
// TrackAt() with the same track index.
status_t
BMediaFile::ReleaseTrack(BMediaTrack* track)
{
	CALLED();
	if (!fTrackList || !track)
		return B_ERROR;
	for (int32 i = 0; i < fTrackNum; i++) {
		if (fTrackList[i] == track) {
			TRACE("BMediaFile::ReleaseTrack, releasing track %p with index "
				"%" B_PRId32 "\n", track, i);
			delete track;
			fTrackList[i] = NULL;
			return B_OK;
		}
	}
	fprintf(stderr, "BMediaFile::ReleaseTrack track %p not found\n", track);
	return B_ERROR;
}


status_t
BMediaFile::ReleaseAllTracks()
{
	CALLED();
	if (!fTrackList)
		return B_ERROR;
	for (int32 i = 0; i < fTrackNum; i++) {
		if (fTrackList[i]) {
			TRACE("BMediaFile::ReleaseAllTracks, releasing track %p with "
				"index %" B_PRId32 "\n", fTrackList[i], i);
			delete fTrackList[i];
			fTrackList[i] = NULL;
		}
	}
	return B_OK;
}


// Create and add a track to the media file
BMediaTrack*
BMediaFile::CreateTrack(media_format* mediaFormat,
	const media_codec_info* codecInfo, uint32 flags)
{
	if (mediaFormat == NULL)
		return NULL;

	// NOTE: It is allowed to pass NULL for codecInfo. In that case, the
	// track won't have an Encoder and you can only use WriteChunk() with
	// already encoded data.

	(void)codecInfo;
	(void)flags;
	return NULL;
}


// Create and add a raw track to the media file (it has no encoder)
BMediaTrack*
BMediaFile::CreateTrack(media_format* mf, uint32 flags)
{
	return CreateTrack(mf, NULL, flags);
}


// Lets you set the copyright info for the entire file
status_t
BMediaFile::AddCopyright(const char* copyright)
{
	(void)copyright;
	return B_UNSUPPORTED;
}


// Call this to add user-defined chunks to a file (if they're supported)
status_t
BMediaFile::AddChunk(int32 type, const void* data, size_t size)
{
	CALLED();
	(void)type;
	(void)data;
	(void)size;
	return B_UNSUPPORTED;
}


// After you have added all the tracks you want, call this
status_t
BMediaFile::CommitHeader()
{
	CALLED();
	return B_UNSUPPORTED;
}


// After you have written all the data to the track objects, call this
status_t
BMediaFile::CloseFile()
{
	CALLED();
	fFileClosed = true;
	return fErr == B_OK ? B_OK : fErr;
}

// This is for controlling file format parameters

// returns a copy of the parameter web
status_t
BMediaFile::GetParameterWeb(BParameterWeb** outWeb)
{
	CALLED();
	if (outWeb != NULL)
		*outWeb = NULL;
	return B_UNSUPPORTED;
}


// deprecated BeOS R5 API
BParameterWeb*
BMediaFile::Web()
{
	CALLED();
	return NULL;
}


status_t
BMediaFile::GetParameterValue(int32 id, void* value, size_t* size)
{
	CALLED();
	(void)id;
	(void)value;
	(void)size;
	return B_UNSUPPORTED;
}


status_t
BMediaFile::SetParameterValue(int32 id, const void* value, size_t size)
{
	CALLED();
	(void)id;
	(void)value;
	(void)size;
	return B_UNSUPPORTED;
}


BView*
BMediaFile::GetParameterView()
{
	CALLED();
	return NULL;
}


status_t
BMediaFile::Perform(int32 selector, void* data)
{
	CALLED();
	(void)selector;
	(void)data;
	return B_ERROR;
}


status_t
BMediaFile::ControlFile(int32 selector, void* ioData, size_t size)
{
	CALLED();
	(void)selector;
	(void)ioData;
	(void)size;
	return B_ERROR;
}


// #pragma mark - private


void
BMediaFile::_Init()
{
	CALLED();

	fSource = NULL;
	fTrackNum = 0;
	fTrackList = NULL;
	fExtractor = NULL;
	fStreamer = NULL;
	fWriter = NULL;
	fWriterID = 0;
	fErr = B_NO_INIT;
	std::memset(&fMFI, 0, sizeof(fMFI));
	fDeleteSource = false;
	std::memset(_reserved_BMediaFile_, 0, sizeof(_reserved_BMediaFile_));

	// not used so far:
	fEncoderMgr = NULL;
	fWriterMgr = NULL;
	fFileClosed = false;
}


void
BMediaFile::_UnInit()
{
	ReleaseAllTracks();
	free(fTrackList);
	fTrackList = NULL;

	delete fExtractor;
	fExtractor = NULL;

	if (fDeleteSource) {
		delete fSource;
		fDeleteSource = false;
	}
	fSource = NULL;

	fTrackNum = 0;
	fErr = B_NO_INIT;
}


void
BMediaFile::_InitReader(BDataIO* source, const BUrl* url, int32 flags)
{
	CALLED();
	(void)flags;

	if (source == NULL && url != NULL) {
		if (url->HasProtocol() && url->Protocol() != "file") {
			fErr = B_UNSUPPORTED;
			return;
		}

		BString path = url->Path();
		if (path.IsEmpty()) {
			fErr = B_BAD_VALUE;
			return;
		}

		BFile* file = new(std::nothrow) BFile(path.String(), B_READ_ONLY);
		if (file == NULL) {
			fErr = B_NO_MEMORY;
			return;
		}
		if (file->InitCheck() != B_OK) {
			fErr = file->InitCheck();
			delete file;
			return;
		}

		fSource = file;
		fDeleteSource = true;
		source = file;
	}

	if (source == NULL) {
		fErr = B_BAD_VALUE;
		return;
	}

	if (fSource == NULL)
		fSource = source;

	fExtractor = new(std::nothrow) BPrivate::media::MediaExtractor();
	if (fExtractor == NULL) {
		fErr = B_NO_MEMORY;
		return;
	}

	const char* sourceName = NULL;
	if (sourceName == NULL && url != NULL)
		sourceName = url->Path().String();

	fErr = fExtractor->InitFromDataIO(source, sourceName);
	if (fErr != B_OK)
		return;

	fTrackNum = fExtractor->CountTracks();
	fExtractor->GetFileFormatInfo(&fMFI);
	fTrackList = (BMediaTrack**)malloc(fTrackNum * sizeof(BMediaTrack*));
	if (fTrackList == NULL && fTrackNum > 0) {
		fErr = B_NO_MEMORY;
		return;
	}
	if (fTrackList != NULL)
		std::memset(fTrackList, 0, fTrackNum * sizeof(BMediaTrack*));
}


void
BMediaFile::_InitWriter(BDataIO* target, const BUrl* url,
	const media_file_format* fileFormat, int32 flags)
{
	CALLED();
	(void)target;
	(void)url;
	(void)flags;

	if (fileFormat == NULL) {
		fErr = B_BAD_VALUE;
		return;
	}

	if (target == NULL && url == NULL) {
		fErr = B_NO_MEMORY;
		return;
	}

	fErr = B_UNSUPPORTED;
}


void
BMediaFile::_InitStreamer(const BUrl& url, BDataIO** adapter)
{
	CALLED();
	(void)url;
	if (adapter != NULL)
		*adapter = NULL;
}

/*
//unimplemented
BMediaFile::BMediaFile();
BMediaFile::BMediaFile(const BMediaFile&);
 BMediaFile::BMediaFile& operator=(const BMediaFile&);
*/

status_t BMediaFile::_Reserved_BMediaFile_0(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_1(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_2(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_3(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_4(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_5(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_6(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_7(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_8(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_9(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_10(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_11(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_12(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_13(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_14(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_15(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_16(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_17(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_18(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_19(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_20(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_21(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_22(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_23(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_24(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_25(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_26(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_27(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_28(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_29(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_30(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_31(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_32(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_33(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_34(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_35(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_36(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_37(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_38(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_39(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_40(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_41(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_42(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_43(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_44(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_45(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_46(int32 arg, ...) { return B_ERROR; }
status_t BMediaFile::_Reserved_BMediaFile_47(int32 arg, ...) { return B_ERROR; }
