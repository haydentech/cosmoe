#include <TestSuite.h>
#include <TestSuiteAddon.h>

// ##### Include headers for your tests here #####
// #include "barchivable/ArchivableTest.h"  // TODO: posix/string.h issue
// #include "bautolock/AutolockTest.h"  // TODO: BThreadedTestCaller
// #include "blocker/LockerTest.h"  // TODO: BThreadedTestCaller
#include "bmemoryio/MemoryIOTest.h"
#include "bmemoryio/MallocIOTest.h"
#include "bstring/StringTest.h"
#include "bblockcache/BlockCacheTest.h"
// #include "ByteOrderTest.h"  // TODO: undefined __swap_double


BTestSuite *
getTestSuite()
{
	BTestSuite *suite = new BTestSuite("Support");

	// ##### Add test suites here #####
	// suite->addTest("BArchivable", ArchivableTestSuite());
	// suite->addTest("BAutolock", AutolockTestSuite());
	// suite->addTest("BLocker", LockerTestSuite());
	suite->addTest("BMemoryIO", MemoryIOTestSuite());
	suite->addTest("BMallocIO", MallocIOTestSuite());
	suite->addTest("BString", StringTestSuite());
	suite->addTest("BBlockCache", BlockCacheTestSuite());
	// suite->addTest("ByteOrder", ByteOrderTestSuite());

	return suite;
}
