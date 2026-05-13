//------------------------------------------------------------------------------
//	MessageCStringItemTest.h
//
//------------------------------------------------------------------------------

#ifndef MESSAGECSTRINGITEMTEST_H
#define MESSAGECSTRINGITEMTEST_H

// Standard Includes -----------------------------------------------------------

// System Includes -------------------------------------------------------------
#include <Debug.h>

// Project Includes ------------------------------------------------------------

// Local Includes --------------------------------------------------------------
#include "MessageItemTest.h"

// Local Defines ---------------------------------------------------------------

// Globals ---------------------------------------------------------------------

struct TCStringFuncPolicy
{
	static status_t Add(BMessage& msg, const char* name, const char*& val)
	{
		return msg.AddString(name, val);
	}

	static status_t AddData(BMessage& msg, const char* name, type_code type,
		const char** val, ssize_t size, bool fixedSize = true)
	{
		return msg.AddData(name, type, *val, size, fixedSize, 1);
	}

	static status_t Find(BMessage& msg, const char* name, int32 index,
		const char** val)
	{
		return msg.FindString(name, index, val);
	}

	static status_t ShortFind(BMessage& msg, const char* name,
		const char** val)
	{
		return msg.FindString(name, val);
	}

	static const char* QuickFind(BMessage& msg, const char* name, int32 index)
	{
		return msg.FindString(name, index);
	}

	static bool Has(BMessage& msg, const char* name, int32 index)
	{
		return msg.HasString(name, index);
	}

	static status_t Replace(BMessage& msg, const char* name, int32 index,
		const char*& val)
	{
		return msg.ReplaceString(name, index, val);
	}

	static status_t FindData(BMessage& msg, const char* name, type_code type,
		int32 index, const void** data, ssize_t* size)
	{
		return msg.FindData(name, type, index, data, size);
	}
};

struct TCStringInitPolicy : public ArrayTypeBase<const char*>
{
	typedef const char*	TypePtr;

	inline static const char* Zero()	{ return sStr1; }
	inline static const char* Test1()	{ return sStr2; }
	inline static const char* Test2()	{ return sStr3; }
	inline static size_t SizeOf(const char*& data)	{ return strlen(data) + 1; }
	inline static ArrayType Array()
	{
		ArrayType array;
		array.push_back(Zero());
		array.push_back(Test1());
		array.push_back(Test2());
		return array;
	}

	private:
		static const char* sStr1;
		static const char* sStr2;
		static const char* sStr3;
};
const char* TCStringInitPolicy::sStr1 = "";
const char* TCStringInitPolicy::sStr2 = "cstring one";
const char* TCStringInitPolicy::sStr3 = "Bibbity-bobbity-boo!";
//------------------------------------------------------------------------------
struct TCStringAssertPolicy
{
	inline static const char*	Zero()				{ return ""; }
	inline static const char*	Invalid()			{ return NULL; }
	static bool					Size(size_t size, const char* data)
		;//{ return size == msg.FlattenedSize(); }
};
bool TCStringAssertPolicy::Size(size_t size, const char* data)
{
	return size == strlen(data) + 1;
}
//------------------------------------------------------------------------------
struct TCStringComparePolicy
{
	static bool Compare(const char* lhs, const char* rhs);
};
bool TCStringComparePolicy::Compare(const char* lhs, const char* rhs)
{
	if (!lhs)
		return !rhs;
	if (!rhs)
		return false;
	return strcmp(lhs, rhs) == 0;
}
//------------------------------------------------------------------------------
template<>
struct TypePolicy<const char*>
{
	typedef const char** TypePtr;
	enum { FixedSize = false };
	// For strings, FindData returns the const char* directly, not a pointer to it
	// So we just cast the pointer, not dereference it
	inline const char* Dereference(TypePtr p) { return (const char*)p; }
	inline TypePtr AddressOf(const char*& t) { return &t; }
};
//------------------------------------------------------------------------------
typedef TMessageItemTest
<
	const char*,
	B_STRING_TYPE,
	TCStringFuncPolicy,
	TCStringInitPolicy,
	TCStringAssertPolicy,
	TCStringComparePolicy
>
TMessageCStringItemTest;


#endif	// MESSAGECSTRINGITEMTEST_H

/*
 * $Log $
 *
 * $Id  $
 *
 */

