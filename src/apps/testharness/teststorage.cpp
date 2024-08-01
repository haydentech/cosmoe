#include <VolumeRoster.h>
#include <stdio.h>
#include <string.h>

#include <TestSuite.h>

#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace std;

#include <errno.h>
#include <list>
#include <map>
#include <set>

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <cppunit/TestCaller.h>
#include <cppunit/TestSuite.h>
#include <cppunit/TestFailure.h>

#include <Entry.h>
#include <Directory.h>
#include <Path.h>

#include "EntryTest.h"
#include "PathTest.h"
#include "FileTest.h"
#include "NodeTest.h"
#include "DirectoryTest.h"

	typedef std::map<std::string, CppUnit::Test*> TestMap;
	typedef std::map<std::string, BTestSuite*> SuiteMap;

int main(void)
{
	BTestSuite *bsuite = new BTestSuite("Storage");

	//bsuite->addTest("BAppFileInfo", AppFileInfoTest::Suite());
	//bsuite->addTest("BDirectory", DirectoryTest::Suite());
	bsuite->addTest("BEntry", EntryTest::Suite());
	//bsuite->addTest("BFile", FileTest::Suite());
	//bsuite->addTest("BMimeType", MimeTypeTest::Suite());
	//bsuite->addTest("BNode", NodeTest::Suite());
	//bsuite->addTest("BNodeInfo", NodeInfoTest::Suite());
	//bsuite->addTest("BPath", PathTest::Suite());
	//bsuite->addTest("BQuery", QueryTest::Suite());
	//bsuite->addTest("BResources", ResourcesTest::Suite());
	//bsuite->addTest("BResourceStrings", ResourceStringsTest::Suite());
	//bsuite->addTest("BSymLink", SymLinkTest::Suite());
	//bsuite->addTest("BVolume", VolumeTest::Suite());
	//bsuite->addTest("FindDirectory", FindDirectoryTest::Suite());
	//bsuite->addTest("MimeSniffer", MimeSnifferTest::Suite());
	
	CppUnit::TestSuite suite;

	// Add its tests
	const TestMap &map = bsuite->getTests();
	for (TestMap::const_iterator i = map.begin(); i != map.end(); i++) {
		suite.addTest( i->second );
		if (i->second)
			cout << "  " << i->first << endl;
	}

	CppUnit::TestResult fTestResults;
	CppUnit::TestResultCollector fResultsCollector;
	fTestResults.addListener(&fResultsCollector);
	suite.run(&fTestResults);


	if (true) {
		// Print out detailed results for verbosity levels > 0
		cout << "------------------------------------------------------------------------------" << endl;
		cout << "Results " << endl;
		cout << "------------------------------------------------------------------------------" << endl;

		// Print failures and errors if there are any, otherwise just say "PASSED"
		::CppUnit::TestResultCollector::TestFailures::const_iterator iFailure;
		if (fResultsCollector.testFailuresTotal() > 0) {
			if (fResultsCollector.testFailures() > 0) {
				cout << "- FAILURES: " << fResultsCollector.testFailures() << endl;
				for (iFailure = fResultsCollector.failures().begin();
				     iFailure != fResultsCollector.failures().end();
				     ++iFailure)
				{
					if (!(*iFailure)->isError())
						cout << "    " << (*iFailure)->failedTestName() << endl;
				}
			}
			if (fResultsCollector.testErrors() > 0) {
				cout << "- ERRORS: " << fResultsCollector.testErrors() << endl;
				for (iFailure = fResultsCollector.failures().begin();
				     iFailure != fResultsCollector.failures().end();
				     ++iFailure)
				{
					if ((*iFailure)->isError())
						cout << "    " << (*iFailure)->failedTestName() << endl;
				}
			}

		}
		else
			cout << "+ PASSED" << endl;

		cout << endl;

	}
	else {
		// Print out concise results for verbosity level == 0
		if (fResultsCollector.testFailuresTotal() > 0)
			cout << "- FAILED" << endl;
		else
			cout << "+ PASSED" << endl;
	}

}

