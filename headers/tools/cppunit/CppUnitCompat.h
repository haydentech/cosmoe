// Compatibility layer for differing CppUnit API versions
#ifndef _beos_cppunit_compat_h_
#define _beos_cppunit_compat_h_

#include <cppunit/Message.h>
#include <cppunit/Exception.h>

namespace CppUnit {

// Some CppUnit versions provide these helpers; for portability, provide
// minimal shims here when they are absent.

// Marker type meaning "no exception expected" in template defaults.
struct NoExceptionExpected {};

// Default trait: when an exception was expected but not thrown, raise
// a failure exception.
template <class ExpectedException>
struct ExpectedExceptionTraits {
    static void expectedException() {
        throw Exception(Message("Expected exception not thrown"));
    }
};

// Specialization for "no exception expected" -- do nothing.
template <>
struct ExpectedExceptionTraits<NoExceptionExpected> {
    static void expectedException() {}
};

} // namespace CppUnit

#endif // _beos_cppunit_compat_h_
