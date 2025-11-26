#include <TestSuite.h>
#include <TestSuiteAddon.h>

// ##### Include headers for your tests here #####
// #include "AppFileInfoTest.h"  // TODO: BMimeType::IsInstalled not implemented
#include "DirectoryTest.h"
// #include "DataIOTest.h"  // TODO: BBufferedDataIO not implemented
#include "EntryTest.h"
#include "FileTest.h"
#include "FindDirectoryTest.h"
// #include "MimeSnifferTest.h"
// #include "MimeTypeTest.h"
// #include "NodeInfoTest.h"  // TODO: BNodeInfo::GetPreferredApp/SetPreferredApp not implemented
#include "NodeTest.h"
#include "PathTest.h"
// #include "QueryTest.h"
// #include "ResourcesTest.h"
// #include "ResourceStringsTest.h"
#include "SymLinkTest.h"
// #include "VolumeTest.h"

BTestSuite* getTestSuite() {
	BTestSuite *suite = new BTestSuite("Storage");

	// ##### Add test suites here #####
	// suite->addTest("BAppFileInfo", AppFileInfoTest::Suite());
	suite->addTest("BDirectory", DirectoryTest::Suite());
	// suite->addTest("BDataIO", DataIOTest::Suite());  // TODO: BBufferedDataIO not implemented
	suite->addTest("BEntry", EntryTest::Suite());
	suite->addTest("BFile", FileTest::Suite());
// #if 0
// 	suite->addTest("BMimeType", MimeTypeTest::Suite());
// #endif
	suite->addTest("BNode", NodeTest::Suite());
	// suite->addTest("BNodeInfo", NodeInfoTest::Suite());  // TODO: BNodeInfo::GetPreferredApp not implemented
	suite->addTest("BPath", PathTest::Suite());
	// TODO: calls Lock on destruction hangs
	//suite->addTest("BQuery", QueryTest::Suite());
	// suite->addTest("BResources", ResourcesTest::Suite());
	// suite->addTest("BResourceStrings", ResourceStringsTest::Suite());
	suite->addTest("BSymLink", SymLinkTest::Suite());
	// TODO: mkbfs missing
	//suite->addTest("BVolume", VolumeTest::Suite());
	suite->addTest("FindDirectory", FindDirectoryTest::Suite());
	// suite->addTest("MimeSniffer", MimeSnifferTest::Suite());
	
	return suite;
}
