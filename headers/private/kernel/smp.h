/*
 * Copyright 2002-2005, Axel Dörfler, axeld@pinc-software.de.
 * Distributed under the terms of the MIT License.
 *
 * Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
 * Distributed under the terms of the NewOS License.
 */
#ifndef KERNEL_SMP_H
#define KERNEL_SMP_H


#include <string.h>


struct kernel_args;


// intercpu messages
enum {
	SMP_MSG_INVALIDATE_PAGE_RANGE = 0,
	SMP_MSG_INVALIDATE_PAGE_LIST,
	SMP_MSG_USER_INVALIDATE_PAGES,
	SMP_MSG_GLOBAL_INVALIDATE_PAGES,
	SMP_MSG_CPU_HALT,
	SMP_MSG_CALL_FUNCTION,
	SMP_MSG_RESCHEDULE
};

enum {
	SMP_MSG_FLAG_ASYNC		= 0x0,
	SMP_MSG_FLAG_SYNC		= 0x1,
	SMP_MSG_FLAG_FREE_ARG	= 0x2,
};

typedef void (*smp_call_func)(addr_t data1, int32 currentCPU, addr_t data2, addr_t data3);

class CPUSet {
public:
	inline				CPUSet();

	inline	void		ClearAll();
	inline	void		SetAll();

	inline	void		SetBit(int32 cpu);
	inline	void		ClearBit(int32 cpu);

	inline	void		SetBitAtomic(int32 cpu);
	inline	void		ClearBitAtomic(int32 cpu);

	inline	bool		GetBit(int32 cpu) const;

	inline	bool		Matches(const CPUSet& mask) const;
	inline	CPUSet		And(const CPUSet& mask) const;

	inline	bool		IsEmpty() const;

	inline uint32		Bits(uint32 index) const { return fBitmap[index];}
private:
	static	const int	kArrayBits = 32;
	static	const int	kArraySize = ROUNDUP(SMP_MAX_CPUS, kArrayBits) / kArrayBits;

			uint32		fBitmap[kArraySize];
};

inline
CPUSet::CPUSet()
{
	memset(fBitmap, 0, sizeof(fBitmap));
}


inline void
CPUSet::ClearAll()
{
	memset(fBitmap, 0, sizeof(fBitmap));
}


inline void
CPUSet::SetAll()
{
	memset(fBitmap, ~uint8(0), sizeof(fBitmap));
}


inline void
CPUSet::SetBit(int32 cpu)
{
	int32* element = (int32*)&fBitmap[cpu / kArrayBits];
	*element |= 1u << (cpu % kArrayBits);
}


inline void
CPUSet::ClearBit(int32 cpu)
{
	int32* element = (int32*)&fBitmap[cpu / kArrayBits];
	*element &= ~uint32(1u << (cpu % kArrayBits));
}


inline void
CPUSet::SetBitAtomic(int32 cpu)
{
	int32* element = (int32*)&fBitmap[cpu / kArrayBits];
	atomic_or(element, 1u << (cpu % kArrayBits));
}


inline void
CPUSet::ClearBitAtomic(int32 cpu)
{
	int32* element = (int32*)&fBitmap[cpu / kArrayBits];
	atomic_and(element, ~uint32(1u << (cpu % kArrayBits)));
}


inline bool
CPUSet::GetBit(int32 cpu) const
{
	int32* element = (int32*)&fBitmap[cpu / kArrayBits];
	return ((uint32)atomic_get(element) & (1u << (cpu % kArrayBits))) != 0;
}


inline CPUSet
CPUSet::And(const CPUSet& mask) const
{
	CPUSet andSet;
	for (int i = 0; i < kArraySize; i++)
		andSet.fBitmap[i] = fBitmap[i] & mask.fBitmap[i];
	return andSet;
}


inline bool
CPUSet::Matches(const CPUSet& mask) const
{
	for (int i = 0; i < kArraySize; i++) {
		if ((fBitmap[i] & mask.fBitmap[i]) != 0)
			return true;
	}

	return false;
}


inline bool
CPUSet::IsEmpty() const
{
	for (int i = 0; i < kArraySize; i++) {
		if (fBitmap[i] != 0)
			return false;
	}

	return true;
}


#endif	/* KERNEL_SMP_H */
