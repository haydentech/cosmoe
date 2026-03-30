#ifndef COSMOE_GAME_AUDIO_BACKEND_MINIAUDIO_H
#define COSMOE_GAME_AUDIO_BACKEND_MINIAUDIO_H

#include <GameSoundDefs.h>
#include <SupportDefs.h>

#include <string>
#include <vector>

namespace BPrivate {
namespace GameAudio {

status_t InitBackend();
void ShutdownBackend();

status_t CreateBufferFromPcm(const gs_audio_format* format, const void* data,
	int64 frames, gs_id* sound);
status_t CreateStreamingBuffer(const void* object, const gs_audio_format* format,
	size_t inBufferFrameCount, size_t inBufferCount, gs_id* sound);
void ReleaseBuffer(gs_id sound);

bool IsPlaying(gs_id sound);
status_t StartPlaying(gs_id sound);
status_t StopPlaying(gs_id sound);

status_t Buffer(gs_id sound, gs_audio_format* format, void*& data);
status_t GetAttributes(gs_id sound, gs_attribute* attributes, size_t attributeCount);
status_t SetAttributes(gs_id sound, gs_attribute* attributes, size_t attributeCount);

const gs_audio_format& DeviceFormat();
const gs_audio_format& SoundFormat(gs_id sound);

status_t DecodeFileToFloat32(const std::string& path,
	std::vector<float>& samples, int64& frames, uint32& channelCount,
	float& sampleRate);
status_t DecodeMemoryToFloat32(const void* data, size_t size,
	std::vector<float>& samples, int64& frames, uint32& channelCount,
	float& sampleRate);

} // namespace GameAudio
} // namespace BPrivate

#endif
