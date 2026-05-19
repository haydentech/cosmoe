// MimeUpdateTest.h

#ifndef __sk_mime_update_test_h__
#define __sk_mime_update_test_h__

#include <cppunit/TestCaller.h>
#include <cppunit/TestSuite.h>

#include "BasicTest.h"


class MimeUpdateTest : public BasicTest {
public:
	static CppUnit::Test* Suite();

	void setUp();
	void tearDown();

	void UpdateMimeInfoTest();
	void CreateAppMetaMimeTest();

private:
	const char* TestDir() const;
	status_t MimeDatabaseDir(BPath& path) const;
};


#endif	// __sk_mime_update_test_h__