/*
	$Id: BlockCacheTest.cpp 4522 2003-09-07 11:53:03Z bonefish $
*/
	
	
#include "cppunit/Test.h"
#include "cppunit/TestSuite.h"
// #include "BlockCacheExerciseTest.h"  // TODO: Missing cassert include
// #include "BlockCacheConcurrencyTest.h"  // TODO: Requires BThreadedTestCaller


CppUnit::Test* BlockCacheTestSuite()
{
	CppUnit::TestSuite *testSuite = new CppUnit::TestSuite();
	
	// testSuite->addTest(BlockCacheExerciseTest::suite());
	// testSuite->addTest(BlockCacheConcurrencyTest::suite());
	
	return testSuite;
}

