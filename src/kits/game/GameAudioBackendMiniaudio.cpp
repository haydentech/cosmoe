#define MINIAUDIO_IMPLEMENTATION
#include <third_party/miniaudio/miniaudio.h>

#include "GameAudioBackendMiniaudio.h"

#include <Errors.h>
#include <MediaDefs.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace BPrivate {
namespace GameAudio {

namespace {

struct SoundEntry {
	gs_audio_format format;
	ma_audio_buffer buffer;
	ma_sound sound;
	ma_uint64 frames;
	float gain;
	float pan;
	bool looping;
	bool initializedBuffer;
	bool initializedSound;
};

struct BackendState {
	ma_engine engine;
	bool initialized;
	gs_audio_format deviceFormat;
	gs_id nextSoundId;
	std::mutex lock;
	std::unordered_map<gs_id, std::unique_ptr<SoundEntry>> sounds;

	BackendState()
		:	initialized(false),
			nextSoundId(1)
	{
		std::memset(&deviceFormat, 0, sizeof(deviceFormat));
		deviceFormat.frame_rate = 48000.0f;
		deviceFormat.channel_count = 2;
		deviceFormat.format = gs_audio_format::B_GS_F;
		deviceFormat.byte_order = B_MEDIA_HOST_ENDIAN;
		deviceFormat.buffer_size = 4096;
	}
};

BackendState sBackend;
gs_audio_format sInvalidFormat = {0.0f, 0, 0, 0, 0};

static ma_format
to_ma_format(uint32 format)
{
	switch (format) {
		case gs_audio_format::B_GS_U8:
			return ma_format_u8;
		case gs_audio_format::B_GS_S16:
			return ma_format_s16;
		case gs_audio_format::B_GS_S32:
			return ma_format_s32;
		case gs_audio_format::B_GS_F:
			return ma_format_f32;
		default:
			return ma_format_unknown;
	}
}

static size_t
bytes_per_sample(uint32 format)
{
	switch (format) {
		case gs_audio_format::B_GS_U8:
			return sizeof(uint8);
		case gs_audio_format::B_GS_S16:
			return sizeof(int16);
		case gs_audio_format::B_GS_S32:
			return sizeof(int32);
		case gs_audio_format::B_GS_F:
			return sizeof(float);
		default:
			return 0;
	}
}

static SoundEntry*
find_sound_locked(gs_id sound)
{
	auto it = sBackend.sounds.find(sound);
	if (it == sBackend.sounds.end())
		return nullptr;
	return it->second.get();
}

static void
uninit_sound(SoundEntry& entry)
{
	if (entry.initializedSound)
		ma_sound_uninit(&entry.sound);
	if (entry.initializedBuffer)
		ma_audio_buffer_uninit(&entry.buffer);
	entry.initializedSound = false;
	entry.initializedBuffer = false;
}

static bool
diagnostics_enabled()
{
	static int sEnabled = -1;
	if (sEnabled >= 0)
		return sEnabled == 1;

	const char* value = std::getenv("COSMOE_GAME_AUDIO_DIAGNOSTICS");
	sEnabled = (value != NULL && value[0] != '\0' && std::strcmp(value, "0") != 0)
		? 1 : 0;
	return sEnabled == 1;
}

static float
scale_down_if_clipping(std::vector<float>& samples)
{
	float appliedScale = 1.0f;
	float peak = 0.0f;
	for (size_t i = 0; i < samples.size(); i++) {
		float value = std::fabs(samples[i]);
		if (value > peak)
			peak = value;
	}

	if (peak <= 1.0f)
		return appliedScale;

	appliedScale = 1.0f / peak;
	for (size_t i = 0; i < samples.size(); i++)
		samples[i] *= appliedScale;

	return appliedScale;
}

static void
log_decoded_audio(const char* sourceTag, ma_uint32 channels,
	ma_uint32 sampleRate, ma_uint64 totalFrames, const std::vector<float>& samples,
	float appliedScale)
{
	if (!diagnostics_enabled())
		return;

	if (sourceTag == NULL)
		sourceTag = "<unknown>";

	double sum = 0.0;
	double sumSquares = 0.0;
	float peak = 0.0f;
	for (size_t i = 0; i < samples.size(); i++) {
		float sample = samples[i];
		sum += sample;
		sumSquares += sample * sample;
		float absSample = std::fabs(sample);
		if (absSample > peak)
			peak = absSample;
	}

	double mean = samples.empty() ? 0.0 : (sum / (double)samples.size());
	double rms = samples.empty() ? 0.0 : std::sqrt(sumSquares / (double)samples.size());

	std::fprintf(stderr,
		"[GameAudio] decode source=%s frames=%llu channels=%u rate=%u samples=%zu peak=%.6f rms=%.6f dc=%.6f scale=%.6f\n",
		sourceTag, (unsigned long long)totalFrames, (unsigned int)channels,
		(unsigned int)sampleRate, samples.size(), peak, rms, mean, appliedScale);

	size_t dumpCount = samples.size() < 32 ? samples.size() : 32;
	std::fprintf(stderr, "[GameAudio] first %zu samples:", dumpCount);
	for (size_t i = 0; i < dumpCount; i++)
		std::fprintf(stderr, " %.6f", samples[i]);
	std::fprintf(stderr, "\n");
}

} // namespace

status_t
InitBackend()
{
	std::lock_guard<std::mutex> guard(sBackend.lock);
	if (sBackend.initialized)
		return B_OK;

	ma_result result = ma_engine_init(NULL, &sBackend.engine);
	if (result != MA_SUCCESS)
		return B_ERROR;

	sBackend.initialized = true;
	return B_OK;
}

void
ShutdownBackend()
{
	std::lock_guard<std::mutex> guard(sBackend.lock);
	for (auto& pair : sBackend.sounds)
		uninit_sound(*pair.second);
	sBackend.sounds.clear();

	if (sBackend.initialized) {
		ma_engine_uninit(&sBackend.engine);
		sBackend.initialized = false;
	}
}

status_t
CreateBufferFromPcm(const gs_audio_format* format, const void* data,
	int64 frames, gs_id* sound)
{
	if (format == nullptr || data == nullptr || sound == nullptr || frames <= 0)
		return B_BAD_VALUE;
	if (format->channel_count == 0 || format->frame_rate <= 0.0f)
		return B_BAD_VALUE;

	ma_format maFormat = to_ma_format(format->format);
	if (maFormat == ma_format_unknown)
		return B_MEDIA_BAD_FORMAT;

	status_t initError = InitBackend();
	if (initError != B_OK)
		return initError;

	auto entry = std::make_unique<SoundEntry>();
	std::memset(entry.get(), 0, sizeof(SoundEntry));
	entry->format = *format;
	entry->frames = (ma_uint64)frames;
	entry->format.buffer_size = (size_t)frames * format->channel_count
		* bytes_per_sample(format->format);
	entry->gain = 1.0f;
	entry->pan = 0.0f;
	entry->looping = false;

	ma_audio_buffer_config bufferConfig = ma_audio_buffer_config_init(
		maFormat, format->channel_count, (ma_uint64)frames, data, NULL);
	ma_result result = ma_audio_buffer_init_copy(&bufferConfig, &entry->buffer);
	if (result != MA_SUCCESS)
		return B_NO_MEMORY;
	entry->initializedBuffer = true;

	result = ma_sound_init_from_data_source(&sBackend.engine,
		(ma_data_source*)&entry->buffer, MA_SOUND_FLAG_NO_SPATIALIZATION, NULL,
		&entry->sound);
	if (result != MA_SUCCESS) {
		uninit_sound(*entry);
		return B_ERROR;
	}
	entry->initializedSound = true;

	ma_sound_set_volume(&entry->sound, 1.0f);
	ma_sound_set_pan(&entry->sound, 0.0f);
	ma_sound_set_looping(&entry->sound, MA_FALSE);

	std::lock_guard<std::mutex> guard(sBackend.lock);
	gs_id id = sBackend.nextSoundId++;
	while (id <= 0 || sBackend.sounds.find(id) != sBackend.sounds.end())
		id = sBackend.nextSoundId++;
	sBackend.sounds[id] = std::move(entry);
	*sound = id;

	return B_OK;
}

status_t
CreateStreamingBuffer(const void* object, const gs_audio_format* format,
	size_t inBufferFrameCount, size_t inBufferCount, gs_id* sound)
{
	(void)object;
	(void)format;
	(void)inBufferFrameCount;
	(void)inBufferCount;
	(void)sound;
	return B_UNSUPPORTED;
}

void
ReleaseBuffer(gs_id sound)
{
	std::lock_guard<std::mutex> guard(sBackend.lock);
	auto it = sBackend.sounds.find(sound);
	if (it == sBackend.sounds.end())
		return;
	uninit_sound(*it->second);
	sBackend.sounds.erase(it);
}

bool
IsPlaying(gs_id sound)
{
	std::lock_guard<std::mutex> guard(sBackend.lock);
	SoundEntry* entry = find_sound_locked(sound);
	if (entry == nullptr)
		return false;
	return ma_sound_is_playing(&entry->sound) == MA_TRUE;
}

status_t
StartPlaying(gs_id sound)
{
	std::lock_guard<std::mutex> guard(sBackend.lock);
	SoundEntry* entry = find_sound_locked(sound);
	if (entry == nullptr)
		return B_BAD_VALUE;
	return ma_sound_start(&entry->sound) == MA_SUCCESS ? B_OK : B_ERROR;
}

status_t
StopPlaying(gs_id sound)
{
	std::lock_guard<std::mutex> guard(sBackend.lock);
	SoundEntry* entry = find_sound_locked(sound);
	if (entry == nullptr)
		return B_BAD_VALUE;
	ma_result stopResult = ma_sound_stop(&entry->sound);
	ma_result seekResult = ma_sound_seek_to_pcm_frame(&entry->sound, 0);
	if (stopResult != MA_SUCCESS)
		return B_ERROR;
	return seekResult == MA_SUCCESS ? B_OK : B_ERROR;
}

status_t
Buffer(gs_id sound, gs_audio_format* format, void*& data)
{
	if (format == nullptr)
		return B_BAD_VALUE;

	std::lock_guard<std::mutex> guard(sBackend.lock);
	SoundEntry* entry = find_sound_locked(sound);
	if (entry == nullptr)
		return B_BAD_VALUE;

	size_t sampleSize = bytes_per_sample(entry->format.format);
	if (sampleSize == 0)
		return B_MEDIA_BAD_FORMAT;

	size_t byteCount = (size_t)entry->frames * entry->format.channel_count
		* sampleSize;
	void* copied = std::malloc(byteCount);
	if (copied == nullptr)
		return B_NO_MEMORY;

	std::memcpy(copied, entry->buffer.ref.pData, byteCount);
	*format = entry->format;
	data = copied;
	return B_OK;
}

status_t
GetAttributes(gs_id sound, gs_attribute* attributes, size_t attributeCount)
{
	if (attributes == nullptr)
		return B_BAD_VALUE;

	std::lock_guard<std::mutex> guard(sBackend.lock);
	SoundEntry* entry = find_sound_locked(sound);
	if (entry == nullptr)
		return B_BAD_VALUE;

	for (size_t i = 0; i < attributeCount; i++) {
		switch (attributes[i].attribute) {
			case B_GS_GAIN:
				attributes[i].value = entry->gain;
				attributes[i].duration = 0;
				break;
			case B_GS_PAN:
				attributes[i].value = entry->pan;
				attributes[i].duration = 0;
				break;
			case B_GS_LOOPING:
				attributes[i].value = entry->looping ? 1.0f : 0.0f;
				attributes[i].duration = 0;
				break;
			default:
				attributes[i].value = 0.0f;
				attributes[i].duration = 0;
				break;
		}
	}

	return B_OK;
}

status_t
SetAttributes(gs_id sound, gs_attribute* attributes, size_t attributeCount)
{
	if (attributes == nullptr)
		return B_BAD_VALUE;

	std::lock_guard<std::mutex> guard(sBackend.lock);
	SoundEntry* entry = find_sound_locked(sound);
	if (entry == nullptr)
		return B_BAD_VALUE;

	for (size_t i = 0; i < attributeCount; i++) {
		switch (attributes[i].attribute) {
			case B_GS_GAIN:
				entry->gain = attributes[i].value;
				ma_sound_set_volume(&entry->sound, entry->gain);
				break;
			case B_GS_PAN:
				entry->pan = attributes[i].value;
				ma_sound_set_pan(&entry->sound, entry->pan);
				break;
			case B_GS_LOOPING:
				entry->looping = attributes[i].value != 0.0f;
				ma_sound_set_looping(&entry->sound, entry->looping ? MA_TRUE : MA_FALSE);
				break;
			default:
				break;
		}
	}

	return B_OK;
}

const gs_audio_format&
DeviceFormat()
{
	return sBackend.deviceFormat;
}

const gs_audio_format&
SoundFormat(gs_id sound)
{
	thread_local gs_audio_format copied = {0.0f, 0, 0, 0, 0};
	std::lock_guard<std::mutex> guard(sBackend.lock);
	SoundEntry* entry = find_sound_locked(sound);
	if (entry == nullptr)
		return sInvalidFormat;
	copied = entry->format;
	return copied;
}

status_t
DecodeFileToFloat32(const std::string& path,
	std::vector<float>& samples, int64& frames, uint32& channelCount,
	float& sampleRate)
{
	ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
	ma_decoder decoder;
	ma_result result = ma_decoder_init_file(path.c_str(), &config, &decoder);
	if (result != MA_SUCCESS)
		return B_ENTRY_NOT_FOUND;

	ma_format outputFormat = ma_format_unknown;
	ma_uint32 outputChannels = 0;
	ma_uint32 outputSampleRate = 0;
	result = ma_decoder_get_data_format(&decoder, &outputFormat,
		&outputChannels, &outputSampleRate, NULL, 0);
	if (result != MA_SUCCESS || outputFormat != ma_format_f32
		|| outputChannels == 0 || outputSampleRate == 0) {
		ma_decoder_uninit(&decoder);
		return B_MEDIA_BAD_FORMAT;
	}

	ma_uint64 totalFrames = 0;
	result = ma_decoder_get_length_in_pcm_frames(&decoder, &totalFrames);
	if (result == MA_SUCCESS && totalFrames > 0) {
		samples.resize((size_t)totalFrames * outputChannels);
		ma_uint64 readFrames = 0;
		ma_uint64 offset = 0;
		while (offset < totalFrames) {
			ma_uint64 justRead = 0;
			result = ma_decoder_read_pcm_frames(&decoder,
				samples.data() + offset * outputChannels,
				totalFrames - offset, &justRead);
			if (result != MA_SUCCESS)
				break;
			if (justRead == 0)
				break;
			offset += justRead;
			readFrames += justRead;
		}
		totalFrames = readFrames;
		samples.resize((size_t)totalFrames * outputChannels);
	} else {
		const ma_uint64 kChunkFrames = 4096;
		std::vector<float> chunk((size_t)kChunkFrames * outputChannels);
		samples.clear();
		while (true) {
			ma_uint64 justRead = 0;
			result = ma_decoder_read_pcm_frames(&decoder, chunk.data(), kChunkFrames,
				&justRead);
			if (result != MA_SUCCESS || justRead == 0)
				break;
			samples.insert(samples.end(), chunk.begin(),
				chunk.begin() + (ptrdiff_t)(justRead * outputChannels));
			totalFrames += justRead;
		}
	}

	ma_decoder_uninit(&decoder);
	if (samples.empty() || totalFrames == 0)
		return B_ERROR;

	float appliedScale = scale_down_if_clipping(samples);
	log_decoded_audio(path.c_str(), outputChannels, outputSampleRate, totalFrames,
		samples, appliedScale);

	frames = (int64)totalFrames;
	channelCount = outputChannels;
	sampleRate = (float)outputSampleRate;
	return B_OK;
}

status_t
DecodeMemoryToFloat32(const void* data, size_t size,
	std::vector<float>& samples, int64& frames, uint32& channelCount,
	float& sampleRate)
{
	if (data == NULL || size == 0)
		return B_BAD_VALUE;

	ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
	ma_decoder decoder;
	ma_result result = ma_decoder_init_memory(data, size, &config, &decoder);
	if (result != MA_SUCCESS)
		return B_BAD_DATA;

	ma_format outputFormat = ma_format_unknown;
	ma_uint32 outputChannels = 0;
	ma_uint32 outputSampleRate = 0;
	result = ma_decoder_get_data_format(&decoder, &outputFormat,
		&outputChannels, &outputSampleRate, NULL, 0);
	if (result != MA_SUCCESS || outputFormat != ma_format_f32
		|| outputChannels == 0 || outputSampleRate == 0) {
		ma_decoder_uninit(&decoder);
		return B_MEDIA_BAD_FORMAT;
	}

	ma_uint64 totalFrames = 0;
	result = ma_decoder_get_length_in_pcm_frames(&decoder, &totalFrames);
	if (result == MA_SUCCESS && totalFrames > 0) {
		samples.resize((size_t)totalFrames * outputChannels);
		ma_uint64 readFrames = 0;
		ma_uint64 offset = 0;
		while (offset < totalFrames) {
			ma_uint64 justRead = 0;
			result = ma_decoder_read_pcm_frames(&decoder,
				samples.data() + offset * outputChannels,
				totalFrames - offset, &justRead);
			if (result != MA_SUCCESS)
				break;
			if (justRead == 0)
				break;
			offset += justRead;
			readFrames += justRead;
		}
		totalFrames = readFrames;
		samples.resize((size_t)totalFrames * outputChannels);
	} else {
		const ma_uint64 kChunkFrames = 4096;
		std::vector<float> chunk((size_t)kChunkFrames * outputChannels);
		samples.clear();
		while (true) {
			ma_uint64 justRead = 0;
			result = ma_decoder_read_pcm_frames(&decoder, chunk.data(), kChunkFrames,
				&justRead);
			if (result != MA_SUCCESS || justRead == 0)
				break;
			samples.insert(samples.end(), chunk.begin(),
				chunk.begin() + (ptrdiff_t)(justRead * outputChannels));
			totalFrames += justRead;
		}
	}

	ma_decoder_uninit(&decoder);
	if (samples.empty() || totalFrames == 0)
		return B_BAD_DATA;

	float appliedScale = scale_down_if_clipping(samples);
	log_decoded_audio("<memory>", outputChannels, outputSampleRate, totalFrames,
		samples, appliedScale);

	frames = (int64)totalFrames;
	channelCount = outputChannels;
	sampleRate = (float)outputSampleRate;
	return B_OK;
}

} // namespace GameAudio
} // namespace BPrivate
