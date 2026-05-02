/*
 * Copyright 2026, Bill Hayden
 * Distributed under the terms of the MIT License.
 */

#define MINIAUDIO_IMPLEMENTATION
#include <third_party/miniaudio/miniaudio.h>

#include <SoundPlayer.h>

#include <Autolock.h>
#include <MediaNode.h>
#include <OS.h>
#include <Sound.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>


namespace {

enum {
	kFlagStarted = 0x01,
	kFlagHasData = 0x02,
};


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
frame_size(const media_raw_audio_format& format)
{
	return bytes_per_sample(format.format) * format.channel_count;
}


static float
clamp_unit(float value)
{
	if (value < 0.0f)
		return 0.0f;
	if (value > 1.0f)
		return 1.0f;
	return value;
}


static float
linear_from_db(float db)
{
	if (db <= -96.0f)
		return 0.0f;
	return std::pow(10.0f, db / 20.0f);
}


static float
db_from_linear(float linear)
{
	linear = clamp_unit(linear);
	if (linear <= 0.00001f)
		return -96.0f;
	return 20.0f * std::log10(linear);
}


static media_raw_audio_format
normalize_format(const media_raw_audio_format* requested)
{
	media_raw_audio_format format = media_raw_audio_format::wildcard;
	format.frame_rate = 48000.0f;
	format.channel_count = 2;
	format.format = media_raw_audio_format::B_AUDIO_FLOAT;
	format.byte_order = B_MEDIA_HOST_ENDIAN;
	format.buffer_size = 4096 * sizeof(float) * format.channel_count;

	if (requested == NULL)
		return format;

	if (requested->frame_rate > 0.0f)
		format.frame_rate = requested->frame_rate;
	if (requested->channel_count > 0)
		format.channel_count = requested->channel_count;

	switch (requested->format) {
		case media_raw_audio_format::B_AUDIO_CHAR:
		case media_raw_audio_format::B_AUDIO_UCHAR:
		case media_raw_audio_format::B_AUDIO_SHORT:
		case media_raw_audio_format::B_AUDIO_INT:
		case media_raw_audio_format::B_AUDIO_FLOAT:
		case media_raw_audio_format::B_AUDIO_DOUBLE:
			format.format = requested->format;
			break;
		default:
			break;
	}

	format.byte_order = requested->byte_order != 0
		? requested->byte_order : B_MEDIA_HOST_ENDIAN;

	size_t defaultBuffer = 4096 * frame_size(format);
	format.buffer_size = requested->buffer_size > 0
		? requested->buffer_size : defaultBuffer;
	if (format.buffer_size == 0)
		format.buffer_size = defaultBuffer;

	return format;
}


static float
read_sample_as_float(const uint8* data, uint32 format)
{
	switch (format) {
		case media_raw_audio_format::B_AUDIO_CHAR:
			return (float)(*(const int8*)data) / 127.0f;
		case media_raw_audio_format::B_AUDIO_UCHAR:
			return ((float)(*(const uint8*)data) - 128.0f) / 127.0f;
		case media_raw_audio_format::B_AUDIO_SHORT:
			return (float)(*(const int16*)data) / 32767.0f;
		case media_raw_audio_format::B_AUDIO_INT:
			return (float)(*(const int32*)data) / 2147483647.0f;
		case media_raw_audio_format::B_AUDIO_FLOAT:
			return *(const float*)data;
		case media_raw_audio_format::B_AUDIO_DOUBLE:
			return (float)(*(const double*)data);
		default:
			return 0.0f;
	}
}


static void
mix_buffer_into_float(const void* rawBuffer, size_t frames,
	const media_raw_audio_format& sourceFormat, float gain, float* output,
	uint32 outputChannels)
{
	const uint8* input = (const uint8*)rawBuffer;
	size_t sampleSize = bytes_per_sample(sourceFormat.format);
	if (input == NULL || sampleSize == 0 || sourceFormat.channel_count == 0)
		return;

	for (size_t frame = 0; frame < frames; frame++) {
		const uint8* sourceFrame = input + frame * sampleSize * sourceFormat.channel_count;
		for (uint32 channel = 0; channel < outputChannels; channel++) {
			float sample;
			if (outputChannels == 1 && sourceFormat.channel_count > 1) {
				sample = 0.0f;
				for (uint32 sourceChannel = 0; sourceChannel < sourceFormat.channel_count;
					sourceChannel++) {
					sample += read_sample_as_float(sourceFrame + sourceChannel * sampleSize,
						sourceFormat.format);
				}
				sample /= sourceFormat.channel_count;
			} else {
				uint32 sourceChannel = sourceFormat.channel_count == 1
					? 0 : std::min(channel, sourceFormat.channel_count - 1);
				sample = read_sample_as_float(sourceFrame + sourceChannel * sampleSize,
					sourceFormat.format);
			}

			output[frame * outputChannels + channel] += sample * gain;
		}
	}
}


static std::atomic<int32> sNextPlayId(1);

} // namespace


namespace BPrivate {

class SoundPlayNode {
public:
	SoundPlayNode(BSoundPlayer* owner, const media_raw_audio_format& format);
	~SoundPlayNode();

	const media_raw_audio_format& Format() const;
	status_t Start();
	void Stop();
	bigtime_t CurrentTime() const;
	bigtime_t Latency() const;
	void Render(float* output, ma_uint32 frames,
		const media_raw_audio_format& mixFormat);

private:
	BSoundPlayer* fOwner;
	media_raw_audio_format fFormat;
	int64 fFramesRendered;
	bool fRegistered;
	std::vector<uint8> fScratch;
};

}


namespace {

struct BackendState {
	std::mutex lock;
	ma_device device;
	bool initialized;
	bool running;
	media_raw_audio_format mixFormat;
	std::vector<BPrivate::SoundPlayNode*> players;

	BackendState()
		:	initialized(false),
			running(false),
			mixFormat(normalize_format(NULL))
	{
		std::memset(&device, 0, sizeof(device));
		mixFormat.format = media_raw_audio_format::B_AUDIO_FLOAT;
		mixFormat.channel_count = 2;
		mixFormat.frame_rate = 48000.0f;
		mixFormat.byte_order = B_MEDIA_HOST_ENDIAN;
		mixFormat.buffer_size = 4096 * sizeof(float) * 2;
	}
};


static BackendState sBackend;


static void
backend_callback(ma_device* device, void* output, const void* input,
	ma_uint32 frameCount)
{
	(void)input;
	float* out = (float*)output;
	std::fill(out, out + frameCount * device->playback.channels, 0.0f);

	std::lock_guard<std::mutex> guard(sBackend.lock);
	for (BPrivate::SoundPlayNode* node : sBackend.players) {
		if (node != NULL)
			node->Render(out, frameCount, sBackend.mixFormat);
	}
}


static status_t
ensure_backend()
{
	std::lock_guard<std::mutex> guard(sBackend.lock);
	if (sBackend.initialized)
		return B_OK;

	ma_device_config config = ma_device_config_init(ma_device_type_playback);
	config.playback.format = ma_format_f32;
	config.playback.channels = sBackend.mixFormat.channel_count;
	config.sampleRate = (ma_uint32)sBackend.mixFormat.frame_rate;
	config.periodSizeInFrames = 1024;
	config.dataCallback = backend_callback;
	config.pUserData = NULL;

	if (ma_device_init(NULL, &config, &sBackend.device) != MA_SUCCESS)
		return B_ERROR;

	sBackend.mixFormat.buffer_size = config.periodSizeInFrames
		* sizeof(float) * sBackend.mixFormat.channel_count;
	sBackend.initialized = true;
	return B_OK;
}


static bigtime_t
backend_latency()
{
	size_t frames = frame_size(sBackend.mixFormat) > 0
		? sBackend.mixFormat.buffer_size / frame_size(sBackend.mixFormat)
		: 0;
	if (sBackend.mixFormat.frame_rate <= 0.0f)
		return 0;
	return (bigtime_t)((frames * 1000000.0) / sBackend.mixFormat.frame_rate);
}

} // namespace


namespace BPrivate {

SoundPlayNode::SoundPlayNode(BSoundPlayer* owner,
	const media_raw_audio_format& format)
		:	fOwner(owner),
			fFormat(format),
			fFramesRendered(0),
			fRegistered(false)
	{
	}


SoundPlayNode::~SoundPlayNode()
	{
		Stop();
	}


const media_raw_audio_format&
SoundPlayNode::Format() const
	{
		return fFormat;
	}


status_t
SoundPlayNode::Start()
	{
		status_t status = ensure_backend();
		if (status != B_OK)
			return status;

		bool shouldStart = false;
		{
			std::lock_guard<std::mutex> guard(sBackend.lock);
			if (!fRegistered) {
				sBackend.players.push_back(this);
				fRegistered = true;
			}

			if (!sBackend.running) {
				sBackend.running = true;
				shouldStart = true;
			}
		}

		if (shouldStart && ma_device_start(&sBackend.device) != MA_SUCCESS) {
			std::lock_guard<std::mutex> guard(sBackend.lock);
			auto it = std::find(sBackend.players.begin(), sBackend.players.end(), this);
			if (it != sBackend.players.end())
				sBackend.players.erase(it);
			fRegistered = false;
			sBackend.running = false;
			return B_ERROR;
		}

		return B_OK;
	}


void
SoundPlayNode::Stop()
	{
		bool shouldStop = false;
		{
			std::lock_guard<std::mutex> guard(sBackend.lock);
			if (!fRegistered)
				return;

			auto it = std::find(sBackend.players.begin(), sBackend.players.end(), this);
			if (it != sBackend.players.end())
				sBackend.players.erase(it);

			fRegistered = false;
			if (sBackend.players.empty() && sBackend.running) {
				sBackend.running = false;
				shouldStop = true;
			}
		}

		if (shouldStop)
			ma_device_stop(&sBackend.device);
	}


bigtime_t
SoundPlayNode::CurrentTime() const
	{
		if (fFormat.frame_rate <= 0.0f)
			return 0;
		return (bigtime_t)((fFramesRendered * 1000000.0) / fFormat.frame_rate);
	}


bigtime_t
SoundPlayNode::Latency() const
	{
		return backend_latency();
	}


void
SoundPlayNode::Render(float* output, ma_uint32 frames,
		const media_raw_audio_format& mixFormat)
	{
		BAutolock lock(fOwner->fLocker);
		if (!lock.IsLocked() || (fOwner->fFlags & kFlagStarted) == 0)
			return;

		float masterGain = linear_from_db(fOwner->fVolumeDB);
		if ((fOwner->fFlags & kFlagHasData) != 0
			&& (fOwner->fPlayBufferFunc != NULL || true)) {
			size_t bytes = frames * frame_size(fFormat);
			if (bytes > 0) {
				fScratch.resize(bytes);
				std::memset(fScratch.data(), 0, bytes);
				fOwner->PlayBuffer(fScratch.data(), bytes, fFormat);
				mix_buffer_into_float(fScratch.data(), frames, fFormat, masterGain,
					output, mixFormat.channel_count);
			}
		}

		bigtime_t now = CurrentTime();
		BSoundPlayer::waiting_sound** waiting = &fOwner->fWaitingSounds;
		while (*waiting != NULL) {
			if ((*waiting)->start_time > now) {
				waiting = &(*waiting)->next;
				continue;
			}

			BSoundPlayer::waiting_sound* queued = *waiting;
			*waiting = queued->next;

			BSoundPlayer::playing_sound* playing
				= new(std::nothrow) BSoundPlayer::playing_sound;
			if (playing != NULL) {
				playing->next = fOwner->fPlayingSounds;
				playing->current_offset = 0;
				playing->sound = queued->sound;
				playing->id = queued->id;
				playing->delta = 0;
				float ratio = queued->sound->Format().frame_rate > 0.0f
					&& fFormat.frame_rate > 0.0f
					? queued->sound->Format().frame_rate / fFormat.frame_rate
					: 1.0f;
				playing->rate = std::max(1, (int32)(ratio * 65536.0f));
				playing->wait_sem = create_sem(0, "sound player wait");
				playing->volume = queued->volume;
				fOwner->fPlayingSounds = playing;
			} else {
				queued->sound->ReleaseRef();
			}

			delete queued;
		}

		BSoundPlayer::playing_sound** sound = &fOwner->fPlayingSounds;
		while (*sound != NULL) {
			BSoundPlayer::playing_sound* current = *sound;
			if (current->sound == NULL) {
				sound = &current->next;
				continue;
			}

			const media_raw_audio_format& soundFormat = current->sound->Format();
			size_t soundFrameSize = frame_size(soundFormat);
			size_t sampleSize = bytes_per_sample(soundFormat.format);
			const uint8* soundData = (const uint8*)current->sound->Data();
			off_t soundSize = current->sound->Size();
			int64 totalFrames = soundFrameSize > 0 ? soundSize / soundFrameSize : 0;

			if (soundData == NULL || soundFrameSize == 0 || totalFrames <= 0) {
				fOwner->_NotifySoundDone(current->id, false);
				if (current->wait_sem >= 0)
					release_sem(current->wait_sem);
				current->sound->ReleaseRef();
				current->sound = NULL;
				sound = &current->next;
				continue;
			}

			for (ma_uint32 frame = 0; frame < frames; frame++) {
				if (current->current_offset >= totalFrames)
					break;

				const uint8* sourceFrame = soundData
					+ current->current_offset * soundFrameSize;
				for (uint32 channel = 0; channel < mixFormat.channel_count; channel++) {
					float sample;
					if (mixFormat.channel_count == 1 && soundFormat.channel_count > 1) {
						sample = 0.0f;
						for (uint32 sourceChannel = 0;
							sourceChannel < soundFormat.channel_count; sourceChannel++) {
							sample += read_sample_as_float(
								sourceFrame + sourceChannel * sampleSize,
								soundFormat.format);
						}
						sample /= soundFormat.channel_count;
					} else {
						uint32 sourceChannel = soundFormat.channel_count == 1
							? 0 : std::min(channel, soundFormat.channel_count - 1);
						sample = read_sample_as_float(sourceFrame
							+ sourceChannel * sampleSize, soundFormat.format);
					}

					output[frame * mixFormat.channel_count + channel]
						+= sample * current->volume * masterGain;
				}

				current->delta += current->rate;
				current->current_offset += current->delta >> 16;
				current->delta &= 0xffff;
			}

			if (current->current_offset >= totalFrames) {
				fOwner->_NotifySoundDone(current->id, true);
				if (current->wait_sem >= 0)
					release_sem(current->wait_sem);
				current->sound->ReleaseRef();
				current->sound = NULL;
			}

			sound = &current->next;
		}

		fFramesRendered += frames;
	}

} // namespace BPrivate


sound_error::sound_error(const char* string)
	:	m_str_const(string)
{
}


const char*
sound_error::what() const throw()
{
	return m_str_const != NULL ? m_str_const : "sound_error";
}


BSoundPlayer::BSoundPlayer(const char* name, BufferPlayerFunc playerFunction,
	EventNotifierFunc eventNotifierFunction, void* cookie)
	:	fPlayerNode(NULL),
		fPlayingSounds(NULL),
		fWaitingSounds(NULL),
		fPlayBufferFunc(NULL),
		fNotifierFunc(NULL),
		fLocker(name != NULL ? name : "sound player"),
		fVolumeDB(0.0f),
		fMediaInput(),
		fMediaOutput(),
		fCookie(NULL),
		fFlags(kFlagHasData),
		fInitStatus(B_NO_INIT),
		fVolumeSlider(NULL),
		fLastVolumeUpdate(0),
		fParameterWeb(NULL),
		_reserved{0}
{
	_Init(NULL, NULL, name, NULL, playerFunction, eventNotifierFunction, cookie);
}


BSoundPlayer::BSoundPlayer(const media_raw_audio_format* format,
	const char* name, BufferPlayerFunc playerFunction,
	EventNotifierFunc eventNotifierFunction, void* cookie)
	:	fPlayerNode(NULL),
		fPlayingSounds(NULL),
		fWaitingSounds(NULL),
		fPlayBufferFunc(NULL),
		fNotifierFunc(NULL),
		fLocker(name != NULL ? name : "sound player"),
		fVolumeDB(0.0f),
		fMediaInput(),
		fMediaOutput(),
		fCookie(NULL),
		fFlags(kFlagHasData),
		fInitStatus(B_NO_INIT),
		fVolumeSlider(NULL),
		fLastVolumeUpdate(0),
		fParameterWeb(NULL),
		_reserved{0}
{
	media_multi_audio_format multi = media_multi_audio_format::wildcard;
	if (format != NULL)
		static_cast<media_raw_audio_format&>(multi) = *format;
	_Init(NULL, &multi, name, NULL, playerFunction, eventNotifierFunction, cookie);
}


BSoundPlayer::BSoundPlayer(const media_node& toNode,
	const media_multi_audio_format* format, const char* name,
	const media_input* input, BufferPlayerFunc playerFunction,
	EventNotifierFunc eventNotifierFunction, void* cookie)
	:	fPlayerNode(NULL),
		fPlayingSounds(NULL),
		fWaitingSounds(NULL),
		fPlayBufferFunc(NULL),
		fNotifierFunc(NULL),
		fLocker(name != NULL ? name : "sound player"),
		fVolumeDB(0.0f),
		fMediaInput(),
		fMediaOutput(),
		fCookie(NULL),
		fFlags(kFlagHasData),
		fInitStatus(B_NO_INIT),
		fVolumeSlider(NULL),
		fLastVolumeUpdate(0),
		fParameterWeb(NULL),
		_reserved{0}
{
	_Init(&toNode, format, name, input, playerFunction, eventNotifierFunction, cookie);
}


BSoundPlayer::~BSoundPlayer()
{
	Stop(true, true);

	while (fPlayingSounds != NULL) {
		playing_sound* sound = fPlayingSounds;
		fPlayingSounds = sound->next;
		if (sound->sound != NULL)
			sound->sound->ReleaseRef();
		if (sound->wait_sem >= 0)
			delete_sem(sound->wait_sem);
		delete sound;
	}

	while (fWaitingSounds != NULL) {
		waiting_sound* sound = fWaitingSounds;
		fWaitingSounds = sound->next;
		if (sound->sound != NULL)
			sound->sound->ReleaseRef();
		delete sound;
	}

	delete fPlayerNode;
}


void
BSoundPlayer::_Init(const media_node* node,
	const media_multi_audio_format* format, const char* name,
	const media_input* input, BufferPlayerFunc playerFunction,
	EventNotifierFunc eventNotifierFunction, void* cookie)
{
	(void)name;
	SetCallbacks(playerFunction, eventNotifierFunction, cookie);

	if (node != NULL)
		fMediaOutput.node = *node;
	if (input != NULL)
		fMediaInput = *input;

	media_raw_audio_format rawFormat = normalize_format(
		format != NULL ? static_cast<const media_raw_audio_format*>(format) : NULL);
	fPlayerNode = new(std::nothrow) BPrivate::SoundPlayNode(this, rawFormat);
	if (fPlayerNode == NULL) {
		SetInitError(B_NO_MEMORY);
		return;
	}

	fMediaInput.format.type = B_MEDIA_RAW_AUDIO;
	fMediaInput.format.u.raw_audio = media_multi_audio_format::wildcard;
	fMediaInput.format.u.raw_audio.frame_rate = rawFormat.frame_rate;
	fMediaInput.format.u.raw_audio.channel_count = rawFormat.channel_count;
	fMediaInput.format.u.raw_audio.format = rawFormat.format;
	fMediaInput.format.u.raw_audio.byte_order = rawFormat.byte_order;
	fMediaInput.format.u.raw_audio.buffer_size = rawFormat.buffer_size;

	fMediaOutput.format = fMediaInput.format;
	SetInitError(B_OK);
}


status_t
BSoundPlayer::InitCheck()
{
	return fInitStatus;
}


media_raw_audio_format
BSoundPlayer::Format() const
{
	if (fPlayerNode == NULL)
		return media_raw_audio_format::wildcard;
	return fPlayerNode->Format();
}


status_t
BSoundPlayer::Start()
{
	BPrivate::SoundPlayNode* playerNode = NULL;
	{
		BAutolock lock(fLocker);
		if (!lock.IsLocked())
			return B_ERROR;
		if (fInitStatus != B_OK || fPlayerNode == NULL)
			return fInitStatus;
		if ((fFlags & kFlagStarted) != 0)
			return B_OK;

		playerNode = fPlayerNode;
	}

	status_t status = playerNode->Start();
	if (status != B_OK)
		return status;

	BAutolock lock(fLocker);
	if (!lock.IsLocked()) {
		playerNode->Stop();
		return B_ERROR;
	}
	if ((fFlags & kFlagStarted) == 0) {
		fFlags |= kFlagStarted;
		Notify(B_STARTED);
	}
	return B_OK;
}


void
BSoundPlayer::Stop(bool block, bool flush)
{
	(void)block;
	BPrivate::SoundPlayNode* playerNode = NULL;
	{
		BAutolock lock(fLocker);
		if (!lock.IsLocked() || (fFlags & kFlagStarted) == 0)
			return;

		fFlags &= ~kFlagStarted;
		playerNode = fPlayerNode;
	}

	if (playerNode != NULL)
		playerNode->Stop();

	BAutolock lock(fLocker);
	if (!lock.IsLocked())
		return;

	if (flush) {
		while (fWaitingSounds != NULL) {
			waiting_sound* sound = fWaitingSounds;
			fWaitingSounds = sound->next;
			_NotifySoundDone(sound->id, false);
			if (sound->sound != NULL)
				sound->sound->ReleaseRef();
			delete sound;
		}

		playing_sound* sound = fPlayingSounds;
		while (sound != NULL) {
			if (sound->sound != NULL) {
				_NotifySoundDone(sound->id, false);
				sound->sound->ReleaseRef();
				sound->sound = NULL;
			}
			if (sound->wait_sem >= 0)
				release_sem(sound->wait_sem);
			sound = sound->next;
		}
	}

	Notify(B_STOPPED);
}


BSoundPlayer::BufferPlayerFunc
BSoundPlayer::BufferPlayer() const
{
	return fPlayBufferFunc;
}


void
BSoundPlayer::SetBufferPlayer(BufferPlayerFunc playBuffer)
{
	BAutolock lock(fLocker);
	if (lock.IsLocked())
		fPlayBufferFunc = playBuffer;
}


BSoundPlayer::EventNotifierFunc
BSoundPlayer::EventNotifier() const
{
	return fNotifierFunc;
}


void
BSoundPlayer::SetNotifier(EventNotifierFunc eventNotifierFunction)
{
	BAutolock lock(fLocker);
	if (lock.IsLocked())
		fNotifierFunc = eventNotifierFunction;
}


void*
BSoundPlayer::Cookie() const
{
	return fCookie;
}


void
BSoundPlayer::SetCookie(void* cookie)
{
	BAutolock lock(fLocker);
	if (lock.IsLocked())
		fCookie = cookie;
}


void
BSoundPlayer::SetCallbacks(BufferPlayerFunc playerFunction,
	EventNotifierFunc eventNotifierFunction, void* cookie)
{
	BAutolock lock(fLocker);
	if (!lock.IsLocked())
		return;

	fPlayBufferFunc = playerFunction;
	fNotifierFunc = eventNotifierFunction;
	fCookie = cookie;
}


bigtime_t
BSoundPlayer::CurrentTime()
{
	return fPlayerNode != NULL ? fPlayerNode->CurrentTime() : 0;
}


bigtime_t
BSoundPlayer::PerformanceTime()
{
	return CurrentTime();
}


status_t
BSoundPlayer::Preroll()
{
	return fInitStatus;
}


BSoundPlayer::play_id
BSoundPlayer::StartPlaying(BSound* sound, bigtime_t atTime)
{
	return StartPlaying(sound, atTime, 1.0f);
}


BSoundPlayer::play_id
BSoundPlayer::StartPlaying(BSound* sound, bigtime_t atTime, float withVolume)
{
	BAutolock lock(fLocker);
	if (!lock.IsLocked() || sound == NULL || sound->InitCheck() != B_OK)
		return -1;

	play_id id = sNextPlayId.fetch_add(1);
	withVolume = std::max(0.0f, withVolume);

	if (atTime > CurrentTime()) {
		waiting_sound* queued = new(std::nothrow) waiting_sound;
		if (queued == NULL)
			return -1;

		queued->next = fWaitingSounds;
		queued->start_time = atTime;
		queued->sound = sound->AcquireRef();
		queued->id = id;
		queued->rate = 0;
		queued->volume = withVolume;
		fWaitingSounds = queued;
		return id;
	}

	playing_sound* playing = new(std::nothrow) playing_sound;
	if (playing == NULL)
		return -1;

	playing->next = fPlayingSounds;
	playing->current_offset = 0;
	playing->sound = sound->AcquireRef();
	playing->id = id;
	playing->delta = 0;
	float ratio = sound->Format().frame_rate > 0.0f && Format().frame_rate > 0.0f
		? sound->Format().frame_rate / Format().frame_rate : 1.0f;
	playing->rate = std::max(1, (int32)(ratio * 65536.0f));
	playing->wait_sem = create_sem(0, "sound player wait");
	playing->volume = withVolume;
	fPlayingSounds = playing;
	return id;
}


status_t
BSoundPlayer::SetSoundVolume(play_id sound, float newVolume)
{
	BAutolock lock(fLocker);
	if (!lock.IsLocked())
		return B_ERROR;

	for (playing_sound* current = fPlayingSounds; current != NULL; current = current->next) {
		if (current->id == sound && current->sound != NULL) {
			current->volume = std::max(0.0f, newVolume);
			return B_OK;
		}
	}

	for (waiting_sound* current = fWaitingSounds; current != NULL; current = current->next) {
		if (current->id == sound) {
			current->volume = std::max(0.0f, newVolume);
			return B_OK;
		}
	}

	return B_BAD_VALUE;
}


bool
BSoundPlayer::IsPlaying(play_id id)
{
	BAutolock lock(fLocker);
	if (!lock.IsLocked())
		return false;

	for (playing_sound* current = fPlayingSounds; current != NULL; current = current->next) {
		if (current->id == id && current->sound != NULL)
			return true;
	}

	for (waiting_sound* current = fWaitingSounds; current != NULL; current = current->next) {
		if (current->id == id)
			return true;
	}

	return false;
}


status_t
BSoundPlayer::StopPlaying(play_id id)
{
	BAutolock lock(fLocker);
	if (!lock.IsLocked())
		return B_ERROR;

	waiting_sound** waiting = &fWaitingSounds;
	while (*waiting != NULL) {
		if ((*waiting)->id != id) {
			waiting = &(*waiting)->next;
			continue;
		}

		waiting_sound* current = *waiting;
		*waiting = current->next;
		_NotifySoundDone(current->id, false);
		if (current->sound != NULL)
			current->sound->ReleaseRef();
		delete current;
		return B_OK;
	}

	for (playing_sound* current = fPlayingSounds; current != NULL; current = current->next) {
		if (current->id != id || current->sound == NULL)
			continue;

		_NotifySoundDone(current->id, false);
		current->sound->ReleaseRef();
		current->sound = NULL;
		if (current->wait_sem >= 0)
			release_sem(current->wait_sem);
		return B_OK;
	}

	return B_BAD_VALUE;
}


status_t
BSoundPlayer::WaitForSound(play_id id)
{
	sem_id waitSem = B_BAD_SEM_ID;
	{
		BAutolock lock(fLocker);
		if (!lock.IsLocked())
			return B_ERROR;

		playing_sound** current = &fPlayingSounds;
		while (*current != NULL) {
			if ((*current)->id != id) {
				current = &(*current)->next;
				continue;
			}

			if ((*current)->sound == NULL) {
				playing_sound* finished = *current;
				*current = finished->next;
				if (finished->wait_sem >= 0)
					delete_sem(finished->wait_sem);
				delete finished;
				return B_OK;
			}

			waitSem = (*current)->wait_sem;
			break;
		}
	}

	if (waitSem < 0)
		return B_BAD_VALUE;

	status_t status = acquire_sem(waitSem);

	BAutolock lock(fLocker);
	if (lock.IsLocked()) {
		playing_sound** current = &fPlayingSounds;
		while (*current != NULL) {
			if ((*current)->id != id) {
				current = &(*current)->next;
				continue;
			}

			playing_sound* finished = *current;
			*current = finished->next;
			if (finished->wait_sem >= 0)
				delete_sem(finished->wait_sem);
			delete finished;
			break;
		}
	}

	return status;
}


float
BSoundPlayer::Volume()
{
	BAutolock lock(fLocker);
	if (!lock.IsLocked())
		return 0.0f;
	return linear_from_db(fVolumeDB);
}


void
BSoundPlayer::SetVolume(float volume)
{
	BAutolock lock(fLocker);
	if (lock.IsLocked())
		fVolumeDB = db_from_linear(volume);
}


float
BSoundPlayer::VolumeDB(bool forcePoll)
{
	(void)forcePoll;
	BAutolock lock(fLocker);
	if (!lock.IsLocked())
		return -96.0f;
	return fVolumeDB;
}


void
BSoundPlayer::SetVolumeDB(float dB)
{
	BAutolock lock(fLocker);
	if (!lock.IsLocked())
		return;
	fVolumeDB = dB;
	fLastVolumeUpdate = system_time();
}


status_t
BSoundPlayer::GetVolumeInfo(media_node* _node, int32* _parameterID,
	float* _minDB, float* _maxDB)
{
	if (_node != NULL) {
		_node->node = -1;
		_node->port = -1;
		_node->kind = 0;
	}
	if (_parameterID != NULL)
		*_parameterID = 0;
	if (_minDB != NULL)
		*_minDB = -96.0f;
	if (_maxDB != NULL)
		*_maxDB = 0.0f;
	return B_OK;
}


bigtime_t
BSoundPlayer::Latency()
{
	return fPlayerNode != NULL ? fPlayerNode->Latency() : 0;
}


bool
BSoundPlayer::HasData()
{
	return (fFlags & kFlagHasData) != 0;
}


void
BSoundPlayer::SetHasData(bool hasData)
{
	BAutolock lock(fLocker);
	if (!lock.IsLocked())
		return;
	if (hasData)
		fFlags |= kFlagHasData;
	else
		fFlags &= ~kFlagHasData;
}


void
BSoundPlayer::SetInitError(status_t error)
{
	fInitStatus = error;
}


void
BSoundPlayer::_SoundPlayBufferFunc(void* cookie, void* buffer, size_t size,
	const media_raw_audio_format& format)
{
	BSoundPlayer* player = static_cast<BSoundPlayer*>(cookie);
	if (player != NULL)
		player->PlayBuffer(buffer, size, format);
}


status_t
BSoundPlayer::_Reserved_SoundPlayer_0(void*, ...)
{
	return B_ERROR;
}


status_t
BSoundPlayer::_Reserved_SoundPlayer_1(void*, ...)
{
	return B_ERROR;
}


status_t
BSoundPlayer::_Reserved_SoundPlayer_2(void*, ...)
{
	return B_ERROR;
}


status_t
BSoundPlayer::_Reserved_SoundPlayer_3(void*, ...)
{
	return B_ERROR;
}


status_t
BSoundPlayer::_Reserved_SoundPlayer_4(void*, ...)
{
	return B_ERROR;
}


status_t
BSoundPlayer::_Reserved_SoundPlayer_5(void*, ...)
{
	return B_ERROR;
}


status_t
BSoundPlayer::_Reserved_SoundPlayer_6(void*, ...)
{
	return B_ERROR;
}


status_t
BSoundPlayer::_Reserved_SoundPlayer_7(void*, ...)
{
	return B_ERROR;
}


void
BSoundPlayer::_GetVolumeSlider()
{
}


void
BSoundPlayer::_NotifySoundDone(play_id sound, bool gotToPlay)
{
	Notify(B_SOUND_DONE, sound, gotToPlay);
}


void
BSoundPlayer::Notify(sound_player_notification what, ...)
{
	if (fNotifierFunc == NULL)
		return;

	va_list args;
	va_start(args, what);
	switch (what) {
		case B_SOUND_DONE:
		{
			play_id sound = va_arg(args, play_id);
			int gotToPlay = va_arg(args, int);
			fNotifierFunc(fCookie, what, sound, gotToPlay);
			break;
		}
		case B_STARTED:
		case B_STOPPED:
		default:
			fNotifierFunc(fCookie, what);
			break;
	}
	va_end(args);
}


void
BSoundPlayer::PlayBuffer(void* buffer, size_t size,
	const media_raw_audio_format& format)
{
	if (fPlayBufferFunc != NULL)
		fPlayBufferFunc(fCookie, buffer, size, format);
	else if (buffer != NULL)
		std::memset(buffer, 0, size);
}