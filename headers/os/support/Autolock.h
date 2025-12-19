/*
 * Copyright 2001-2009, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef	_AUTOLOCK_H
#define	_AUTOLOCK_H


#include <Locker.h>
#include <Looper.h>


class BAutolock {
public:
	inline						BAutolock(BLooper* looper);
	inline						BAutolock(BLocker* locker);
	inline						BAutolock(BLocker& locker);
	inline						~BAutolock();

	inline	bool				IsLocked();

	inline	bool				Lock();
	inline	void				Unlock();

private:
			BLocker*			fLocker;
			BLooper*			fLooper;
			bool				fIsLocked;
};


inline
BAutolock::BAutolock(BLooper *looper)
	:
	fLocker(NULL),
	fLooper(looper),
	fIsLocked(looper->Lock())
{
}


inline
BAutolock::BAutolock(BLocker *locker)
	:
	fLocker(locker),
	fLooper(NULL),
	fIsLocked(locker->Lock())
{
}


inline
BAutolock::BAutolock(BLocker &locker)
	:
	fLocker(&locker),
	fLooper(NULL),
	fIsLocked(locker.Lock())
{
}


inline
BAutolock::~BAutolock()
{
	Unlock();
}


inline bool
BAutolock::IsLocked()
{
	return fIsLocked;
}


inline bool
BAutolock::Lock()
{
	if (fIsLocked)
		return true;

	if (fLooper != NULL)
		fIsLocked = fLooper->Lock();
	else
		fIsLocked = fLocker->Lock();

	return fIsLocked;
}


inline void
BAutolock::Unlock()
{
	if (!fIsLocked)
		return;

	fIsLocked = false;
	if (fLooper != NULL)
		fLooper->Unlock();
	else
		fLocker->Unlock();
}


// Template-based AutoLock from Tracker (originally in private/tracker/AutoLock.h)
// Exception-safe locking mechanism, allocate on stack and have
// destructor unlock for you whenever the lock goes out of scope
template<class T>
class AutoLock {
public:
	AutoLock(T* lock, bool lockNow = true)
		:	fLock(lock),
			fHasLock(false)
	{
		if (lockNow)
			fHasLock = fLock->Lock();
	}

	AutoLock(T& lock, bool lockNow = true)
		:	fLock(&lock),
			fHasLock(false)
	{
		if (lockNow)
			fHasLock = fLock->Lock();
	}

	~AutoLock()
	{
		if (fHasLock)
			fLock->Unlock();
	}

	bool operator!() const
	{
		return !fHasLock;
	}

	bool IsLocked() const
	{
		return fHasLock;
	}

	// Explicit Lock/Unlock calls are only used in special cases
	// for unlocking before lock goes out of scope and successive re-locking
	void Unlock()
	{
		if (fHasLock) {
			fLock->Unlock();
			fHasLock = false;
		}
	}

	bool Lock()
	{
		if (!fHasLock)
			fHasLock = fLock->Lock();
		return fHasLock;
	}

	// Convenience call used when passing the AutoLock and the locked object around
	T* LockedItem() const
	{
		return fLock;
	}

private:
	T*		fLock;
	bool	fHasLock;
};


#endif	// _AUTOLOCK_H
