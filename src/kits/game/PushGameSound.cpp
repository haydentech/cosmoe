#include <PushGameSound.h>

#include <Errors.h>

namespace {

#define PUSH_RESERVED(MethodName) \
	status_t BPushGameSound::MethodName(int32 arg, ...) \
	{ \
		(void)arg; \
		return B_ERROR; \
	}

} // namespace

BPushGameSound::BPushGameSound(size_t inBufferFrameCount,
	const gs_audio_format* format, size_t inBufferCount, BGameSoundDevice* device)
	:	BStreamingGameSound(device),
		fLock(B_BAD_SEM_ID),
		fPageLocked(NULL),
		fLockPos(0),
		fPlayPos(0),
		fBuffer(NULL),
		fPageSize(0),
		fPageCount(0),
		fBufferSize(0)
{
	SetInitError(SetParameters(inBufferFrameCount, format, inBufferCount));
}

BPushGameSound::BPushGameSound(BGameSoundDevice* device)
	:	BStreamingGameSound(device),
		fLock(B_BAD_SEM_ID),
		fPageLocked(NULL),
		fLockPos(0),
		fPlayPos(0),
		fBuffer(NULL),
		fPageSize(0),
		fPageCount(0),
		fBufferSize(0)
{
}

BPushGameSound::~BPushGameSound()
{
}

BPushGameSound::lock_status
BPushGameSound::LockNextPage(void** pagePtr, size_t* pageSize)
{
	(void)pagePtr;
	(void)pageSize;
	return lock_failed;
}

status_t
BPushGameSound::UnlockPage(void* pagePtr)
{
	(void)pagePtr;
	return B_UNSUPPORTED;
}

BPushGameSound::lock_status
BPushGameSound::LockForCyclic(void** basePtr, size_t* size)
{
	(void)basePtr;
	(void)size;
	return lock_failed;
}

status_t
BPushGameSound::UnlockCyclic()
{
	return B_UNSUPPORTED;
}

size_t
BPushGameSound::CurrentPosition()
{
	return fPlayPos;
}

BGameSound*
BPushGameSound::Clone() const
{
	return NULL;
}

status_t
BPushGameSound::Perform(int32 selector, void* data)
{
	(void)selector;
	(void)data;
	return B_ERROR;
}

status_t
BPushGameSound::SetParameters(size_t bufferFrameCount,
	const gs_audio_format* format, size_t bufferCount)
{
	return BStreamingGameSound::SetParameters(bufferFrameCount, format, bufferCount);
}

status_t
BPushGameSound::SetStreamHook(void (*hook)(void* inCookie, void* buffer,
	size_t byteCount, BStreamingGameSound* me), void* cookie)
{
	return BStreamingGameSound::SetStreamHook(hook, cookie);
}

void
BPushGameSound::FillBuffer(void* buffer, size_t byteCount)
{
	BStreamingGameSound::FillBuffer(buffer, byteCount);
}

bool
BPushGameSound::BytesReady(size_t* bytes)
{
	if (bytes != NULL)
		*bytes = 0;
	return false;
}

PUSH_RESERVED(_Reserved_BPushGameSound_0)
PUSH_RESERVED(_Reserved_BPushGameSound_1)
PUSH_RESERVED(_Reserved_BPushGameSound_2)
PUSH_RESERVED(_Reserved_BPushGameSound_3)
PUSH_RESERVED(_Reserved_BPushGameSound_4)
PUSH_RESERVED(_Reserved_BPushGameSound_5)
PUSH_RESERVED(_Reserved_BPushGameSound_6)
PUSH_RESERVED(_Reserved_BPushGameSound_7)
PUSH_RESERVED(_Reserved_BPushGameSound_8)
PUSH_RESERVED(_Reserved_BPushGameSound_9)
PUSH_RESERVED(_Reserved_BPushGameSound_10)
PUSH_RESERVED(_Reserved_BPushGameSound_11)
PUSH_RESERVED(_Reserved_BPushGameSound_12)
PUSH_RESERVED(_Reserved_BPushGameSound_13)
PUSH_RESERVED(_Reserved_BPushGameSound_14)
PUSH_RESERVED(_Reserved_BPushGameSound_15)
PUSH_RESERVED(_Reserved_BPushGameSound_16)
PUSH_RESERVED(_Reserved_BPushGameSound_17)
PUSH_RESERVED(_Reserved_BPushGameSound_18)
PUSH_RESERVED(_Reserved_BPushGameSound_19)
PUSH_RESERVED(_Reserved_BPushGameSound_20)
PUSH_RESERVED(_Reserved_BPushGameSound_21)
PUSH_RESERVED(_Reserved_BPushGameSound_22)
PUSH_RESERVED(_Reserved_BPushGameSound_23)
