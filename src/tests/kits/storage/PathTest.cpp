// PathTest.cpp
#include "PathTest.h"

#include <cppunit/Test.h>
#include <cppunit/TestCaller.h>
#include <cppunit/TestSuite.h>
#include <Directory.h>
#include <Entry.h>
#include <Path.h>
#include <TypeConstants.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <string>
using std::string;


namespace {

#ifdef _WIN32
const char* kRootPath = "C:/";
const char* kRootMessyPath = "C:/.///.";
const char* kRootTrailingMessyPath = "C:/.///";
const char* kHomeLeaf = "Users";
const char* kHomePath = "C:/Users";
const char* kUsrLeaf = "Windows";
const char* kUsrPath = "C:/Windows";
const char* kUsrTrailingPath = "C:/Windows/";
const char* kUsrLocalLeaf = "System32";
const char* kUsrLocalPath = "C:/Windows/System32";
const char* kUsrLibLeaf = "drivers";
const char* kUsrLibPath = "C:/Windows/System32/drivers";
const char* kUsrLibParentPath = "C:/Windows/System32";
const char* kUsrLibLinuxPath = "C:/Windows/System32/drivers/etc";
const char* kRelativeHomePath = "Users";
const char* kRelativeHomeTrailingPath = "Users/";
const char* kRelativeUsrPath = "Windows";
const char* kRelativeUsrTrailingPath = "Windows/";
const char* kUsrLibFromRootLeaf = "Windows/System32/drivers";
const char* kUsrLibLinuxFromUsrLeaf = "System32/drivers/etc";
const char* kUsrLibMessyFromUsrLeaf = "System32/drivers//";
const char* kUsrParentFromUsrLeaf = "System32/..";
const char* kHomeParentFromRootLeaf = "Users/..";
const char* kHomeNonExistingFromRootLeaf = "Users/non-existing";
const char* kNonExistingAbsPath = "C:/doesn't/exist/but/who/cares";
const char* kNonExistingAbsMessyPath = "C:/doesn't/exist/but///who/cares";
const char* kNonExistingAbsBasePath = "C:/doesn't/exist";
const char* kAbstractEntryPath = "C:/Users/shouldn't exist";
const char* kBadAbsoluteLeaf = "C:/Users";
const char* kAbsoluteAppendPath = "C:/Temp";
const char* kDoubleSlashRootPath = "C://";
const char* kBadMissingDirPath = "C:/this/dir/doesn't/exists";
const char* kBadMissingEntryPath = "C:/this/doesn't/exist";
#else
const char* kRootPath = "/";
const char* kRootMessyPath = "/.///.";
const char* kRootTrailingMessyPath = "/.///";
const char* kHomeLeaf = "home";
const char* kHomePath = "/home";
const char* kUsrLeaf = "usr";
const char* kUsrPath = "/usr";
const char* kUsrTrailingPath = "/usr/";
const char* kUsrLocalLeaf = "local";
const char* kUsrLocalPath = "/usr/local";
const char* kUsrLibLeaf = "lib";
const char* kUsrLibPath = "/usr/lib";
const char* kUsrLibParentPath = "/usr";
const char* kUsrLibLinuxPath = "/usr/lib/linux";
const char* kRelativeHomePath = "home";
const char* kRelativeHomeTrailingPath = "home/";
const char* kRelativeUsrPath = "usr";
const char* kRelativeUsrTrailingPath = "usr/";
const char* kUsrLibFromRootLeaf = "usr/lib";
const char* kUsrLibLinuxFromUsrLeaf = "lib/linux";
const char* kUsrLibMessyFromUsrLeaf = "lib//";
const char* kUsrParentFromUsrLeaf = "lib/..";
const char* kHomeParentFromRootLeaf = "home/..";
const char* kHomeNonExistingFromRootLeaf = "home/non-existing";
const char* kNonExistingAbsPath = "/doesn't/exist/but/who/cares";
const char* kNonExistingAbsMessyPath = "/doesn't/exist/but///who/cares";
const char* kNonExistingAbsBasePath = "/doesn't/exist";
const char* kAbstractEntryPath = "/home/shouldn't exist";
const char* kBadAbsoluteLeaf = "/home";
const char* kAbsoluteAppendPath = "/tmp";
const char* kDoubleSlashRootPath = "//";
const char* kBadMissingDirPath = "/this/dir/doesn't/exists";
const char* kBadMissingEntryPath = "/this/doesn't/exist";
#endif

string
JoinPath(const char* base, const char* leaf)
{
	string path(base != NULL ? base : "");
	if (leaf == NULL || leaf[0] == '\0')
		return path;
	if (!path.empty() && path[path.length() - 1] != '/')
		path += '/';
	path += leaf;
	return path;
}

bool
PathsAreEquivalent(const char* expected, const char* actual)
{
	if (expected == actual)
		return true;
	if (expected == NULL || actual == NULL)
		return false;
	if (strcmp(expected, actual) == 0)
		return true;
	#ifdef _WIN32
	string expectedLower(expected);
	string actualLower(actual);
	for (size_t i = 0; i < expectedLower.length(); i++)
		expectedLower[i] = tolower((unsigned char)expectedLower[i]);
	for (size_t i = 0; i < actualLower.length(); i++)
		actualLower[i] = tolower((unsigned char)actualLower[i]);
	if (expectedLower == actualLower)
		return true;
	#endif

	BEntry expectedEntry(expected);
	BEntry actualEntry(actual);
	return expectedEntry.InitCheck() == B_OK
		&& actualEntry.InitCheck() == B_OK
		&& expectedEntry == actualEntry;
}

} // namespace


// Suite
CppUnit::Test*
PathTest::Suite() {
	CppUnit::TestSuite *suite = new CppUnit::TestSuite();
	typedef CppUnit::TestCaller<PathTest> TC;
		
	suite->addTest( new TC("BPath::Init Test1", &PathTest::InitTest1) );
	suite->addTest( new TC("BPath::Init Test2", &PathTest::InitTest2) );
	suite->addTest( new TC("BPath::Append Test", &PathTest::AppendTest) );
	suite->addTest( new TC("BPath::Leaf Test", &PathTest::LeafTest) );
	suite->addTest( new TC("BPath::Parent Test", &PathTest::ParentTest) );
	suite->addTest( new TC("BPath::Comparison Test",
						   &PathTest::ComparisonTest) );
	suite->addTest( new TC("BPath::Assignment Test",
						   &PathTest::AssignmentTest) );
	suite->addTest( new TC("BPath::Flattenable Test",
						   &PathTest::FlattenableTest) );
		
	return suite;
}		

// setUp
void
PathTest::setUp()
{
	BasicTest::setUp();
}
	
// tearDown
void
PathTest::tearDown()
{
	BasicTest::tearDown();
}

// InitTest1
void
PathTest::InitTest1()
{
	// 1. default constructor
	NextSubTest();
	{
		BPath path;
		CPPUNIT_ASSERT( path.InitCheck() == B_NO_INIT );
		CPPUNIT_ASSERT( path.Path() == NULL );
	}

	// 2. BPath(const char*, const char*, bool)
	// absolute existing path (root dir), no leaf, no normalization
	NextSubTest();
	{
		const char *pathName = kRootPath;
		BPath path(pathName);
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( string(pathName) == path.Path() );
	}
	// absolute existing path, no leaf, no normalization
	NextSubTest();
	{
		const char *pathName = kUsrPath;
		BPath path(pathName);
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( string(pathName) == path.Path() );
	}
	// absolute non-existing path, no leaf, no normalization
	NextSubTest();
	{
		const char *pathName = kNonExistingAbsPath;
		BPath path(pathName);
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( string(pathName) == path.Path() );
	}
	// absolute existing path (root dir), no leaf, auto normalization
	NextSubTest();
	{
		const char *pathName = kRootMessyPath;
		const char *normalizedPathName = kRootPath;
		BPath path(pathName);
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( string(normalizedPathName) == path.Path() );
	}
	// absolute existing path, no leaf, auto normalization
	NextSubTest();
	{
		const char *pathName = kUsrTrailingPath;
		const char *normalizedPathName = kUsrPath;
		BPath path(pathName);
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( PathsAreEquivalent(normalizedPathName, path.Path()) );
	}
	// absolute non-existing path, no leaf, auto normalization
	NextSubTest();
	{
		const char *pathName = kNonExistingAbsMessyPath;
		BPath path(pathName);
		#ifdef _WIN32
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( string(kNonExistingAbsPath) == path.Path() );
		#else
		CPPUNIT_ASSERT( path.InitCheck() == B_ENTRY_NOT_FOUND );
		CPPUNIT_ASSERT( path.Path() == NULL );
		#endif
	}
	// absolute existing path (root dir), no leaf, normalization forced
	NextSubTest();
	{
		const char *pathName = kRootPath;
		BPath path(pathName, NULL, true);
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( string(pathName) == path.Path() );
	}
	// absolute existing path, no leaf, normalization forced
	NextSubTest();
	{
		const char *pathName = kUsrPath;
		BPath path(pathName, NULL, true);
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( string(pathName) == path.Path() );
	}
	// absolute non-existing path, no leaf, normalization forced
	NextSubTest();
	{
		const char *pathName = kNonExistingAbsPath;
		BPath path(pathName, NULL, true);
		#ifdef _WIN32
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( string(kNonExistingAbsPath) == path.Path() );
		#else
		CPPUNIT_ASSERT( path.InitCheck() == B_ENTRY_NOT_FOUND );
		CPPUNIT_ASSERT( path.Path() == NULL );
		#endif
	}
	// relative existing path, no leaf, no normalization needed, but done
	chdir(kRootPath);
	NextSubTest();
	{
		const char *pathName = kRelativeUsrPath;
		const char *absolutePathName = kUsrPath;
		BPath path(pathName);
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( string(absolutePathName) == path.Path() );
	}
	// // relative non-existing path, no leaf, no normalization needed, but done
	chdir(kUsrPath);
	NextSubTest();
	{
		const char *pathName = "doesn't/exist/but/who/cares";
		BPath path(pathName);
		#ifdef _WIN32
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( JoinPath(kUsrPath, pathName) == path.Path() );
		#else
		CPPUNIT_ASSERT( path.InitCheck() == B_ENTRY_NOT_FOUND );
		CPPUNIT_ASSERT( path.Path() == NULL );
		#endif
	}
	// relative existing path, no leaf, auto normalization
	chdir(kRootPath);
	NextSubTest();
	{
		const char *pathName = kRelativeUsrTrailingPath;
		const char *normalizedPathName = kUsrPath;
		BPath path(pathName);
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( PathsAreEquivalent(normalizedPathName, path.Path()) );
	}
	// // relative non-existing path, no leaf, auto normalization
	chdir(kUsrPath);
	NextSubTest();
	{
		const char *pathName = "doesn't/exist/but///who/cares";
		BPath path(pathName);
		#ifdef _WIN32
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( JoinPath(kUsrPath,
			"doesn't/exist/but/who/cares") == path.Path() );
		#else
		CPPUNIT_ASSERT( path.InitCheck() == B_ENTRY_NOT_FOUND );
		CPPUNIT_ASSERT( path.Path() == NULL );
		#endif
	}
	// relative existing path, no leaf, normalization forced
	// chdir("/");
	// NextSubTest();
	// {
	// 	const char *pathName = "boot";
	// 	const char *absolutePathName = "/boot";
	// 	BPath path(pathName, NULL, true);
	// 	CPPUNIT_ASSERT( path.InitCheck() == B_OK );
	// 	CPPUNIT_ASSERT( string(absolutePathName) == path.Path() );
	// }
	// relative non-existing path, no leaf, normalization forced
	// chdir("/boot");
	NextSubTest();
	{
		const char *pathName = "doesn't/exist/but/who/cares";
		BPath path(pathName, NULL, true);
		#ifdef _WIN32
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( JoinPath(kUsrPath, pathName) == path.Path() );
		#else
		CPPUNIT_ASSERT( path.InitCheck() == B_ENTRY_NOT_FOUND );
		CPPUNIT_ASSERT( path.Path() == NULL );
		#endif
	}
	// absolute existing path (root dir), leaf, no normalization
	NextSubTest();
	{
		const char *pathName = kRootPath;
		const char *leafName = kHomeLeaf;
		const char *absolutePathName = kHomePath;
		BPath path(pathName, leafName);
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( string(absolutePathName) == path.Path() );
	}
	// absolute existing path, leaf, no normalization
	NextSubTest();
	{
		const char *pathName = kUsrLocalPath;
		const char *leafName = "bin";
		string absolutePathName = JoinPath(pathName, leafName);
		BPath path(pathName, leafName);
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( absolutePathName == path.Path() );
	}
	// absolute non-existing path, leaf, no normalization
	NextSubTest();
	{
		const char *pathName = kNonExistingAbsBasePath;
		const char *leafName = "but/who/cares";
		string absolutePathName = JoinPath(pathName, leafName);
		BPath path(pathName, leafName);
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( absolutePathName == path.Path() );
	}
	// absolute existing path (root dir), leaf, auto normalization
	NextSubTest();
	{
		const char *pathName = kRootTrailingMessyPath;
		const char *leafName = ".";
		const char *absolutePathName = kRootPath;
		BPath path(pathName, leafName);
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( string(absolutePathName) == path.Path() );
	}
	// absolute existing path, leaf, auto normalization
	NextSubTest();
	{
		const char *pathName = kRootPath;
		const char *leafName = kHomeParentFromRootLeaf;
		const char *absolutePathName = kRootPath;
		BPath path(pathName, leafName);
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( PathsAreEquivalent(absolutePathName, path.Path()) );
	}
	// absolute non-existing path, leaf, auto normalization
	NextSubTest();
	{
		const char *pathName = kNonExistingAbsBasePath;
		const char *leafName = "but//who/cares";
		BPath path(pathName, leafName);
		#ifdef _WIN32
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( string(kNonExistingAbsPath) == path.Path() );
		#else
		CPPUNIT_ASSERT( path.InitCheck() == B_ENTRY_NOT_FOUND );
		CPPUNIT_ASSERT( path.Path() == NULL );
		#endif
	}
	// absolute non-existing path, leaf, normalization forced
	NextSubTest();
	{
		const char *pathName = kNonExistingAbsBasePath;
		const char *leafName = "but/who/cares";
		BPath path(pathName, leafName, true);
		#ifdef _WIN32
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( string(kNonExistingAbsPath) == path.Path() );
		#else
		CPPUNIT_ASSERT( path.InitCheck() == B_ENTRY_NOT_FOUND );
		CPPUNIT_ASSERT( path.Path() == NULL );
		#endif
	}
	// relative existing path, leaf, no normalization needed, but done
	chdir(kRootPath);
	NextSubTest();
	{
		const char *pathName = "";
		const char *leafName = kHomeLeaf;
		const char *absolutePathName = kHomePath;
		BPath path(pathName, leafName);
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( string(absolutePathName) == path.Path() );
	}
	// relative non-existing path, leaf, no normalization needed, but done
	chdir(kRootPath);
	NextSubTest();
	{
		const char *pathName = "doesn't/exist";
		const char *leafName = "but/who/cares";
		BPath path(pathName, leafName);
		#ifdef _WIN32
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( string(kNonExistingAbsPath) == path.Path() );
		#else
		CPPUNIT_ASSERT( path.InitCheck() == B_ENTRY_NOT_FOUND );
		CPPUNIT_ASSERT( path.Path() == NULL );
		#endif
	}
	// relative existing path, leaf, auto normalization
	chdir(kRootPath);
	NextSubTest();
	{
		const char *pathName = kRelativeUsrPath;
		const char *leafName = kUsrLibMessyFromUsrLeaf;
		const char *normalizedPathName = kUsrLibPath;
		BPath path(pathName, leafName);
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( PathsAreEquivalent(normalizedPathName, path.Path()) );
	}
	// bad args (absolute lead)
	NextSubTest();
	{
		const char *pathName = kRootPath;
		const char *leafName = kBadAbsoluteLeaf;
		BPath path(pathName, leafName);
		CPPUNIT_ASSERT( path.InitCheck() == B_BAD_VALUE );
		CPPUNIT_ASSERT( path.Path() == NULL );
	}
	// bad args
	NextSubTest();
	{
		BPath path((const char*)NULL, "test");
		CPPUNIT_ASSERT( path.InitCheck() == B_BAD_VALUE );
		CPPUNIT_ASSERT( path.Path() == NULL );
	}
	// bad args
	NextSubTest();
	{
		BPath path((const char*)NULL);
		CPPUNIT_ASSERT( path.InitCheck() == B_BAD_VALUE );
		CPPUNIT_ASSERT( path.Path() == NULL );
	}

	// 3. BPath(const BDirectory*, const char*, bool)
	// existing dir (root dir), no leaf, no normalization
	NextSubTest();
	{
		const char *pathName = kRootPath;
		BDirectory dir(pathName);
		BPath path(&dir, NULL);
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( string(pathName) == path.Path() );
	}
	// existing dir, no leaf, no normalization
	NextSubTest();
	{
		const char *pathName = kRootPath;
		BDirectory dir(pathName);
		BPath path(&dir, NULL);
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( string(pathName) == path.Path() );
	}
	// existing dir (root dir), no leaf, normalization forced
	NextSubTest();
	{
		const char *pathName = kRootPath;
		BDirectory dir(pathName);
		BPath path(&dir, NULL, true);
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( string(pathName) == path.Path() );
	}
	// existing dir, no leaf, normalization forced
	NextSubTest();
	{
		const char *pathName = kRootPath;
		BDirectory dir(pathName);
		BPath path(&dir, NULL, true);
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( string(pathName) == path.Path() );
	}
	// existing dir (root dir), leaf, no normalization
	NextSubTest();
	{
		const char *pathName = kRootPath;
		const char *leafName = kHomeLeaf;
		const char *absolutePathName = kHomePath;
		BDirectory dir(pathName);
		BPath path(&dir, leafName);
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( string(absolutePathName) == path.Path() );
	}
	// existing dir, leaf, no normalization
	NextSubTest();
	{
		const char *pathName = kRootPath;
		const char *leafName = kUsrLibFromRootLeaf;
		const char *absolutePathName = kUsrLibPath;
		BDirectory dir(pathName);
		BPath path(&dir, leafName);
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( string(absolutePathName) == path.Path() );
	}
	// existing dir, leaf, auto normalization
	NextSubTest();
	{
		const char *pathName = kRootPath;
		const char *leafName = "home/..";
		const char *absolutePathName = kRootPath;
		BDirectory dir(pathName);
		BPath path(&dir, leafName);
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( string(absolutePathName) == path.Path() );
	}
	// bad args (absolute leaf)
	NextSubTest();
	{
		const char *pathName = kRootPath;
		const char *leafName = kBadAbsoluteLeaf;
		BDirectory dir(pathName);
		BPath path(&dir, leafName);
		CPPUNIT_ASSERT( path.InitCheck() == B_BAD_VALUE );
		CPPUNIT_ASSERT( path.Path() == NULL );
	}
	// bad args (uninitialized dir)
	NextSubTest();
	{
		BDirectory dir;
		BPath path(&dir, "test");
		CPPUNIT_ASSERT( path.InitCheck() == B_BAD_VALUE );
		CPPUNIT_ASSERT( path.Path() == NULL );
	}
	// bad args (badly initialized dir)
	NextSubTest();
	{
		BDirectory dir(kBadMissingDirPath);
		BPath path(&dir, "test");
		CPPUNIT_ASSERT( path.InitCheck() == B_BAD_VALUE );
		CPPUNIT_ASSERT( path.Path() == NULL );
	}
	// bad args (NULL dir)
// R5: crashs, when passing a NULL BDirectory
#if !TEST_R5
	NextSubTest();
	{
		BPath path((const BDirectory*)NULL, "test");
		CPPUNIT_ASSERT( path.InitCheck() == B_BAD_VALUE );
		CPPUNIT_ASSERT( path.Path() == NULL );
	}
	// bad args (NULL dir)
	NextSubTest();
	{
		BPath path((const BDirectory*)NULL, NULL);
		CPPUNIT_ASSERT( path.InitCheck() == B_BAD_VALUE );
		CPPUNIT_ASSERT( path.Path() == NULL );
	}
#endif

	// 4. BPath(const BEntry*)
	// existing entry (root dir)
	NextSubTest();
	{
		const char *pathName = kRootPath;
		BEntry entry(pathName);
		BPath path(&entry);
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( PathsAreEquivalent(pathName, path.Path()) );
	}
	// existing entry
	NextSubTest();
	{
		const char *pathName = kRootPath;
		BEntry entry(pathName);
		BPath path(&entry);
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( PathsAreEquivalent(pathName, path.Path()) );
	}
	// abstract entry
	NextSubTest();
	{
		const char *pathName = kAbstractEntryPath;
		BEntry entry(pathName);
		BPath path(&entry);
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( PathsAreEquivalent(pathName, path.Path()) );
	}
	// bad args (uninitialized BEntry)
	NextSubTest();
	{
		BEntry entry;
		BPath path(&entry);
		CPPUNIT_ASSERT( path.InitCheck() == B_NO_INIT );
		CPPUNIT_ASSERT( path.Path() == NULL );
	}
	// bad args (badly initialized BEntry)
	NextSubTest();
	{
		BEntry entry(kBadMissingEntryPath);
		BPath path(&entry);
		CPPUNIT_ASSERT( equals(path.InitCheck(), B_NO_INIT, B_ENTRY_NOT_FOUND) );
		CPPUNIT_ASSERT( path.Path() == NULL );
	}
	// bad args (NULL BEntry)
	NextSubTest();
	{
		BPath path((const BEntry*)NULL);
		CPPUNIT_ASSERT( equals(path.InitCheck(), B_NO_INIT, B_BAD_VALUE) );
		CPPUNIT_ASSERT( path.Path() == NULL );
	}

	// 5. BPath(const entry_ref*)
	// existing entry (root dir)
	NextSubTest();
	{
		const char *pathName = kRootPath;
		BEntry entry(pathName);
		entry_ref ref;
		CPPUNIT_ASSERT( entry.GetRef(&ref) == B_OK );
		BPath path(&ref);
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( PathsAreEquivalent(pathName, path.Path()) );
	}
	// existing entry
	NextSubTest();
	{
		const char *pathName = kRootPath;
		BEntry entry(pathName);
		entry_ref ref;
		CPPUNIT_ASSERT( entry.GetRef(&ref) == B_OK );
		BPath path(&ref);
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( PathsAreEquivalent(pathName, path.Path()) );
	}
	// abstract entry
	NextSubTest();
	{
		const char *pathName = kAbstractEntryPath;
		BEntry entry(pathName);
		entry_ref ref;
		CPPUNIT_ASSERT( entry.GetRef(&ref) == B_OK );
		BPath path(&ref);
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( PathsAreEquivalent(pathName, path.Path()) );
	}
	// bad args (NULL entry_ref)
	NextSubTest();
	{
		BPath path((const entry_ref*)NULL);
		CPPUNIT_ASSERT( equals(path.InitCheck(), B_NO_INIT, B_BAD_VALUE) );
		CPPUNIT_ASSERT( path.Path() == NULL );
	}
}

// InitTest2
void
PathTest::InitTest2()
{
	BPath path;
	const char *pathName;
	const char *leafName;
	const char *absolutePathName;
	const char *normalizedPathName;
	BDirectory dir;
	BEntry entry;
	entry_ref ref;

	// 2. SetTo(const char*, const char*, bool)
	// absolute existing path (root dir), no leaf, no normalization
	NextSubTest();
	pathName = kRootPath;
	CPPUNIT_ASSERT( path.SetTo(pathName) == B_OK );
	CPPUNIT_ASSERT( path.InitCheck() == B_OK );
	CPPUNIT_ASSERT( string(pathName) == path.Path() );
	path.Unset();
	// absolute existing path, no leaf, no normalization
	NextSubTest();
	pathName = kHomePath;
	CPPUNIT_ASSERT( path.SetTo(pathName) == B_OK );
	CPPUNIT_ASSERT( path.InitCheck() == B_OK );
	CPPUNIT_ASSERT( string(pathName) == path.Path() );
	path.Unset();
	// absolute non-existing path, no leaf, no normalization
	NextSubTest();
	pathName = kNonExistingAbsPath;
	CPPUNIT_ASSERT( path.SetTo(pathName) == B_OK );
	CPPUNIT_ASSERT( path.InitCheck() == B_OK );
	CPPUNIT_ASSERT( string(pathName) == path.Path() );
	path.Unset();
	// absolute existing path (root dir), no leaf, auto normalization
	NextSubTest();
	pathName = kRootMessyPath;
	normalizedPathName = kRootPath;
	CPPUNIT_ASSERT( path.SetTo(pathName) == B_OK );
	CPPUNIT_ASSERT( path.InitCheck() == B_OK );
	CPPUNIT_ASSERT( string(normalizedPathName) == path.Path() );
	path.Unset();
	// absolute existing path, no leaf, auto normalization
	NextSubTest();
	pathName = kDoubleSlashRootPath;
	normalizedPathName = kRootPath;
	CPPUNIT_ASSERT( path.SetTo(pathName) == B_OK );
	CPPUNIT_ASSERT( path.InitCheck() == B_OK );
	CPPUNIT_ASSERT( string(normalizedPathName) == path.Path() );
	path.Unset();
	// absolute non-existing path, no leaf, auto normalization
	NextSubTest();
	pathName = kNonExistingAbsMessyPath;
	#ifdef _WIN32
	CPPUNIT_ASSERT( path.SetTo(pathName) == B_OK );
	CPPUNIT_ASSERT( path.InitCheck() == B_OK );
	CPPUNIT_ASSERT( string(kNonExistingAbsPath) == path.Path() );
	#else
	CPPUNIT_ASSERT( path.SetTo(pathName) == B_ENTRY_NOT_FOUND );
	CPPUNIT_ASSERT( path.InitCheck() == B_ENTRY_NOT_FOUND );
	CPPUNIT_ASSERT( path.Path() == NULL );
	#endif
	path.Unset();
	// absolute existing path (root dir), no leaf, normalization forced
	NextSubTest();
	pathName = kRootPath;
	CPPUNIT_ASSERT( path.SetTo(pathName, NULL, true) == B_OK );
	CPPUNIT_ASSERT( path.InitCheck() == B_OK );
	CPPUNIT_ASSERT( string(pathName) == path.Path() );
	path.Unset();
	// absolute existing path, no leaf, normalization forced
	NextSubTest();
	pathName = kRootPath;
	CPPUNIT_ASSERT( path.SetTo(pathName, NULL, true) == B_OK );
	CPPUNIT_ASSERT( path.InitCheck() == B_OK );
	CPPUNIT_ASSERT( string(pathName) == path.Path() );
	path.Unset();
	// absolute non-existing path, no leaf, normalization forced
	NextSubTest();
	pathName = kNonExistingAbsPath;
	#ifdef _WIN32
	CPPUNIT_ASSERT( path.SetTo(pathName, NULL, true) == B_OK );
	CPPUNIT_ASSERT( path.InitCheck() == B_OK );
	CPPUNIT_ASSERT( string(pathName) == path.Path() );
	#else
	CPPUNIT_ASSERT( path.SetTo(pathName, NULL, true) == B_ENTRY_NOT_FOUND );
	CPPUNIT_ASSERT( path.InitCheck() == B_ENTRY_NOT_FOUND );
	CPPUNIT_ASSERT( path.Path() == NULL );
	#endif
	path.Unset();
	// relative existing path, no leaf, no normalization needed, but done
	chdir(kRootPath);
	NextSubTest();
	pathName = ".";
	absolutePathName = kRootPath;
	CPPUNIT_ASSERT( path.SetTo(pathName) == B_OK );
	CPPUNIT_ASSERT( path.InitCheck() == B_OK );
	CPPUNIT_ASSERT( string(absolutePathName) == path.Path() );
	path.Unset();
	// relative non-existing path, no leaf, no normalization needed, but done
	chdir(kHomePath);
	NextSubTest();
	pathName = "doesn't/exist/but/who/cares";
	{
		string expectedPath = JoinPath(kHomePath, pathName);
		#ifdef _WIN32
		CPPUNIT_ASSERT( path.SetTo(pathName) == B_OK );
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( expectedPath == path.Path() );
		#else
		CPPUNIT_ASSERT( path.SetTo(pathName) == B_ENTRY_NOT_FOUND );
		CPPUNIT_ASSERT( path.InitCheck() == B_ENTRY_NOT_FOUND );
		CPPUNIT_ASSERT( path.Path() == NULL );
		#endif
	}
	path.Unset();
	// relative existing path, no leaf, auto normalization
	chdir(kRootPath);
	NextSubTest();
	pathName = kRelativeHomeTrailingPath;
	normalizedPathName = kHomePath;
	CPPUNIT_ASSERT( path.SetTo(pathName) == B_OK );
	CPPUNIT_ASSERT( path.InitCheck() == B_OK );
	CPPUNIT_ASSERT( PathsAreEquivalent(normalizedPathName, path.Path()) );
	path.Unset();
	// // relative non-existing path, no leaf, auto normalization
	chdir(kHomePath);
	NextSubTest();
	pathName = "doesn't/exist/but///who/cares";
	#ifdef _WIN32
	CPPUNIT_ASSERT( path.SetTo(pathName) == B_OK );
	CPPUNIT_ASSERT( path.InitCheck() == B_OK );
	CPPUNIT_ASSERT( JoinPath(kHomePath,
		"doesn't/exist/but/who/cares") == path.Path() );
	#else
	CPPUNIT_ASSERT( path.SetTo(pathName) == B_ENTRY_NOT_FOUND );
	CPPUNIT_ASSERT( path.InitCheck() == B_ENTRY_NOT_FOUND );
	CPPUNIT_ASSERT( path.Path() == NULL );
	#endif
	path.Unset();
	// relative existing path, no leaf, normalization forced
	chdir(kRootPath);
	NextSubTest();
	pathName = kRelativeHomePath;
	absolutePathName = kHomePath;
	CPPUNIT_ASSERT( path.SetTo(pathName, NULL, true) == B_OK );
	CPPUNIT_ASSERT( path.InitCheck() == B_OK );
	CPPUNIT_ASSERT( string(absolutePathName) == path.Path() );
	path.Unset();
	// relative non-existing path, no leaf, normalization forced
	chdir(kHomePath);
	NextSubTest();
	pathName = "doesn't/exist/but/who/cares";
	#ifdef _WIN32
	CPPUNIT_ASSERT( path.SetTo(pathName, NULL, true) == B_OK );
	CPPUNIT_ASSERT( path.InitCheck() == B_OK );
	CPPUNIT_ASSERT( JoinPath(kHomePath, pathName) == path.Path() );
	#else
	CPPUNIT_ASSERT( path.SetTo(pathName, NULL, true) == B_ENTRY_NOT_FOUND );
	CPPUNIT_ASSERT( path.InitCheck() == B_ENTRY_NOT_FOUND );
	CPPUNIT_ASSERT( path.Path() == NULL );
	#endif
	path.Unset();
	// absolute existing path (root dir), leaf, no normalization
	NextSubTest();
	pathName = kRootPath;
	leafName = kHomeLeaf;
	absolutePathName = kHomePath;
	CPPUNIT_ASSERT( path.SetTo(pathName, leafName) == B_OK );
	CPPUNIT_ASSERT( path.InitCheck() == B_OK );
	CPPUNIT_ASSERT( string(absolutePathName) == path.Path() );
	path.Unset();
	// absolute existing path, leaf, no normalization
	NextSubTest();
	pathName = kRootPath;
	leafName = kUsrLibFromRootLeaf;
	absolutePathName = kUsrLibPath;
	CPPUNIT_ASSERT( path.SetTo(pathName, leafName) == B_OK );
	CPPUNIT_ASSERT( path.InitCheck() == B_OK );
	CPPUNIT_ASSERT( string(absolutePathName) == path.Path() );
	path.Unset();
	// absolute non-existing path, leaf, no normalization
	NextSubTest();
	pathName = kNonExistingAbsBasePath;
	leafName = "but/who/cares";
	{
		string absolute = JoinPath(pathName, leafName);
		CPPUNIT_ASSERT( path.SetTo(pathName, leafName) == B_OK );
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( absolute == path.Path() );
	}
	path.Unset();
	// absolute existing path (root dir), leaf, auto normalization
	NextSubTest();
	pathName = kRootTrailingMessyPath;
	leafName = ".";
	absolutePathName = kRootPath;
	CPPUNIT_ASSERT( path.SetTo(pathName, leafName) == B_OK );
	CPPUNIT_ASSERT( path.InitCheck() == B_OK );
	CPPUNIT_ASSERT( string(absolutePathName) == path.Path() );
	path.Unset();
	// absolute existing path, leaf, auto normalization
	NextSubTest();
	pathName = kRootPath;
	leafName = kHomeParentFromRootLeaf;
	absolutePathName = kRootPath;
	CPPUNIT_ASSERT( path.SetTo(pathName, leafName) == B_OK );
	CPPUNIT_ASSERT( path.InitCheck() == B_OK );
	CPPUNIT_ASSERT( PathsAreEquivalent(absolutePathName, path.Path()) );
	path.Unset();
	// absolute existing path, leaf, self assignment
	NextSubTest();
	pathName = kUsrLibPath;
	leafName = kUsrLibLeaf;
	absolutePathName = kUsrLibPath;
	CPPUNIT_ASSERT( path.SetTo(pathName) == B_OK );
	CPPUNIT_ASSERT( path.SetTo(path.Path(), ".///./") == B_OK );
	CPPUNIT_ASSERT( PathsAreEquivalent(pathName, path.Path()) );
	CPPUNIT_ASSERT( path.SetTo(path.Path(), "..") == B_OK );
	CPPUNIT_ASSERT( PathsAreEquivalent(kUsrLibParentPath, path.Path()) );
	CPPUNIT_ASSERT( path.SetTo(path.Path(), leafName) == B_OK );
	CPPUNIT_ASSERT( string(absolutePathName) == path.Path() );
	path.Unset();
	// absolute non-existing path, leaf, auto normalization
	NextSubTest();
	pathName = kNonExistingAbsBasePath;
	leafName = "but//who/cares";
	#ifdef _WIN32
	CPPUNIT_ASSERT( path.SetTo(pathName, leafName) == B_OK );
	CPPUNIT_ASSERT( path.InitCheck() == B_OK );
	CPPUNIT_ASSERT( string(kNonExistingAbsPath) == path.Path() );
	#else
	CPPUNIT_ASSERT( path.SetTo(pathName, leafName) == B_ENTRY_NOT_FOUND );
	CPPUNIT_ASSERT( path.InitCheck() == B_ENTRY_NOT_FOUND );
	CPPUNIT_ASSERT( path.Path() == NULL );
	#endif
	path.Unset();
	// absolute non-existing path, leaf, normalization forced
	NextSubTest();
	pathName = kNonExistingAbsBasePath;
	leafName = "but/who/cares";
	#ifdef _WIN32
	CPPUNIT_ASSERT( path.SetTo(pathName, leafName, true) == B_OK );
	CPPUNIT_ASSERT( path.InitCheck() == B_OK );
	CPPUNIT_ASSERT( string(kNonExistingAbsPath) == path.Path() );
	#else
	CPPUNIT_ASSERT( path.SetTo(pathName, leafName, true) == B_ENTRY_NOT_FOUND );
	CPPUNIT_ASSERT( path.InitCheck() == B_ENTRY_NOT_FOUND );
	CPPUNIT_ASSERT( path.Path() == NULL );
	#endif
	path.Unset();
	// relative existing path, leaf, no normalization needed, but done
	chdir(kRootPath);
	// NextSubTest();
	pathName = kRelativeUsrPath;
	leafName = kHomeLeaf;
	{
		string absolute = JoinPath(kUsrPath, leafName);
		CPPUNIT_ASSERT( path.SetTo(pathName, leafName) == B_OK );
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( absolute == path.Path() );
	}
	path.Unset();
	// // relative non-existing path, leaf, no normalization needed, but done
	chdir(kRootPath);
	NextSubTest();
	pathName = "doesn't/exist";
	leafName = "but/who/cares";
	#ifdef _WIN32
	CPPUNIT_ASSERT( path.SetTo(pathName, leafName) == B_OK );
	CPPUNIT_ASSERT( path.InitCheck() == B_OK );
	CPPUNIT_ASSERT( string(kNonExistingAbsPath) == path.Path() );
	#else
	CPPUNIT_ASSERT( path.SetTo(pathName, leafName) == B_ENTRY_NOT_FOUND );
	CPPUNIT_ASSERT( path.InitCheck() == B_ENTRY_NOT_FOUND );
	CPPUNIT_ASSERT( path.Path() == NULL );
	#endif
	path.Unset();
	// relative existing path, leaf, auto normalization
	chdir(kRootPath);
	NextSubTest();
	pathName = kRelativeUsrPath;
	leafName = kUsrLibMessyFromUsrLeaf;
	normalizedPathName = kUsrLibPath;
	CPPUNIT_ASSERT( path.SetTo(pathName, leafName) == B_OK );
	CPPUNIT_ASSERT( path.InitCheck() == B_OK );
	CPPUNIT_ASSERT( PathsAreEquivalent(normalizedPathName, path.Path()) );
	path.Unset();
	// bad args (absolute leaf)
	NextSubTest();
	CPPUNIT_ASSERT( path.SetTo(kRootPath, kUsrPath) == B_BAD_VALUE );
	CPPUNIT_ASSERT( path.InitCheck() == B_BAD_VALUE );
	CPPUNIT_ASSERT( path.Path() == NULL );
	path.Unset();
	// bad args
	NextSubTest();
	CPPUNIT_ASSERT( path.SetTo((const char*)NULL, "test") == B_BAD_VALUE );
	CPPUNIT_ASSERT( path.InitCheck() == B_BAD_VALUE );
	CPPUNIT_ASSERT( path.Path() == NULL );
	path.Unset();
	// bad args
	NextSubTest();
	CPPUNIT_ASSERT( path.SetTo((const char*)NULL) == B_BAD_VALUE );
	CPPUNIT_ASSERT( path.InitCheck() == B_BAD_VALUE );
	CPPUNIT_ASSERT( path.Path() == NULL );
	path.Unset();

	// 3. SetTo(const BDirectory*, const char*, bool)
	// existing dir (root dir), no leaf, no normalization
	NextSubTest();
	pathName = kRootPath;
	CPPUNIT_ASSERT( dir.SetTo(pathName) == B_OK );
	CPPUNIT_ASSERT( path.SetTo(&dir, NULL) == B_OK );
	CPPUNIT_ASSERT( path.InitCheck() == B_OK );
	CPPUNIT_ASSERT( PathsAreEquivalent(pathName, path.Path()) );
	path.Unset();
	dir.Unset();
	// existing dir, no leaf, no normalization
	NextSubTest();
	pathName = kUsrPath;
	CPPUNIT_ASSERT( dir.SetTo(pathName) == B_OK );
	CPPUNIT_ASSERT( path.SetTo(&dir, NULL) == B_OK );
	CPPUNIT_ASSERT( path.InitCheck() == B_OK );
	CPPUNIT_ASSERT( PathsAreEquivalent(pathName, path.Path()) );
	path.Unset();
	dir.Unset();
	// existing dir (root dir), no leaf, normalization forced
	NextSubTest();
	pathName = kRootPath;
	CPPUNIT_ASSERT( dir.SetTo(pathName) == B_OK );
	CPPUNIT_ASSERT( path.SetTo(&dir, NULL, true) == B_OK );
	CPPUNIT_ASSERT( path.InitCheck() == B_OK );
	CPPUNIT_ASSERT( PathsAreEquivalent(pathName, path.Path()) );
	path.Unset();
	dir.Unset();
	// existing dir, no leaf, normalization forced
	NextSubTest();
	pathName = kUsrPath;
	CPPUNIT_ASSERT( dir.SetTo(pathName) == B_OK );
	CPPUNIT_ASSERT( path.SetTo(&dir, NULL, true) == B_OK );
	CPPUNIT_ASSERT( path.InitCheck() == B_OK );
	CPPUNIT_ASSERT( PathsAreEquivalent(pathName, path.Path()) );
	path.Unset();
	dir.Unset();
	// existing dir (root dir), leaf, no normalization
	NextSubTest();
	pathName = kRootPath;
	leafName = kUsrLeaf;
	absolutePathName = kUsrPath;
	CPPUNIT_ASSERT( dir.SetTo(pathName) == B_OK );
	CPPUNIT_ASSERT( path.SetTo(&dir, leafName) == B_OK );
	CPPUNIT_ASSERT( path.InitCheck() == B_OK );
	CPPUNIT_ASSERT( PathsAreEquivalent(absolutePathName, path.Path()) );
	path.Unset();
	dir.Unset();
	// existing dir, leaf, no normalization
	NextSubTest();
	pathName = kUsrPath;
	leafName = kUsrLibLinuxFromUsrLeaf;
	absolutePathName = kUsrLibLinuxPath;
	CPPUNIT_ASSERT( dir.SetTo(pathName) == B_OK );
	CPPUNIT_ASSERT( path.SetTo(&dir, leafName) == B_OK );
	CPPUNIT_ASSERT( path.InitCheck() == B_OK );
	CPPUNIT_ASSERT( PathsAreEquivalent(absolutePathName, path.Path()) );
	path.Unset();
	dir.Unset();
	// existing dir, leaf, auto normalization
	NextSubTest();
	pathName = kUsrPath;
	leafName = kUsrParentFromUsrLeaf;
	absolutePathName = kUsrPath;
	CPPUNIT_ASSERT( dir.SetTo(pathName) == B_OK );
	CPPUNIT_ASSERT( path.SetTo(&dir, leafName) == B_OK );
	CPPUNIT_ASSERT( path.InitCheck() == B_OK );
	CPPUNIT_ASSERT( PathsAreEquivalent(absolutePathName, path.Path()) );
	path.Unset();
	dir.Unset();
	// // bad args (absolute leaf)
	NextSubTest();
	pathName = kRootPath;
	leafName = kUsrPath;
	CPPUNIT_ASSERT( dir.SetTo(pathName) == B_OK );
	CPPUNIT_ASSERT( path.SetTo(&dir, leafName) == B_BAD_VALUE );
	CPPUNIT_ASSERT( path.InitCheck() == B_BAD_VALUE );
	CPPUNIT_ASSERT( path.Path() == NULL );
	path.Unset();
	dir.Unset();
	// bad args (uninitialized dir)
	NextSubTest();
	CPPUNIT_ASSERT( dir.InitCheck() == B_NO_INIT );
	CPPUNIT_ASSERT( path.SetTo(&dir, "test") == B_BAD_VALUE );
	CPPUNIT_ASSERT( path.InitCheck() == B_BAD_VALUE );
	CPPUNIT_ASSERT( path.Path() == NULL );
	path.Unset();
	dir.Unset();
	// bad args (badly initialized dir)
	NextSubTest();
	CPPUNIT_ASSERT( dir.SetTo(kBadMissingDirPath) == B_ENTRY_NOT_FOUND );
	CPPUNIT_ASSERT( path.SetTo(&dir, "test") == B_BAD_VALUE );
	CPPUNIT_ASSERT( path.InitCheck() == B_BAD_VALUE );
	CPPUNIT_ASSERT( path.Path() == NULL );
	path.Unset();
	dir.Unset();
// R5: crashs, when passing a NULL BDirectory
#if !TEST_R5
	// bad args (NULL dir)
	NextSubTest();
	CPPUNIT_ASSERT( path.SetTo((const BDirectory*)NULL, "test") == B_BAD_VALUE );
	CPPUNIT_ASSERT( path.InitCheck() == B_BAD_VALUE );
	CPPUNIT_ASSERT( path.Path() == NULL );
	path.Unset();
	dir.Unset();
	// bad args (NULL dir)
	NextSubTest();
	CPPUNIT_ASSERT( path.SetTo((const BDirectory*)NULL, NULL) == B_BAD_VALUE );
	CPPUNIT_ASSERT( path.InitCheck() == B_BAD_VALUE );
	CPPUNIT_ASSERT( path.Path() == NULL );
	path.Unset();
	dir.Unset();
#endif

	// 4. SetTo(const BEntry*)
	// existing entry (root dir)
	NextSubTest();
	pathName = kRootPath;
	CPPUNIT_ASSERT( entry.SetTo(pathName) == B_OK );
	CPPUNIT_ASSERT( path.SetTo(&entry) == B_OK );
	CPPUNIT_ASSERT( path.InitCheck() == B_OK );
	CPPUNIT_ASSERT( PathsAreEquivalent(pathName, path.Path()) );
	path.Unset();
	entry.Unset();
	// existing entry
	NextSubTest();
	pathName = kUsrPath;
	CPPUNIT_ASSERT( entry.SetTo(pathName) == B_OK );
	CPPUNIT_ASSERT( path.SetTo(&entry) == B_OK );
	CPPUNIT_ASSERT( path.InitCheck() == B_OK );
	CPPUNIT_ASSERT( PathsAreEquivalent(pathName, path.Path()) );
	path.Unset();
	entry.Unset();
	// abstract entry
	NextSubTest();
	{
		string abstractPath = JoinPath(kUsrPath, "shouldn't exist");
		pathName = abstractPath.c_str();
		CPPUNIT_ASSERT( entry.SetTo(pathName) == B_OK );
		CPPUNIT_ASSERT( path.SetTo(&entry) == B_OK );
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( PathsAreEquivalent(pathName, path.Path()) );
	}
	path.Unset();
	entry.Unset();
	// bad args (uninitialized BEntry)
	NextSubTest();
	CPPUNIT_ASSERT( entry.InitCheck() == B_NO_INIT );
	CPPUNIT_ASSERT( path.SetTo(&entry) == B_NO_INIT );
	CPPUNIT_ASSERT( path.InitCheck() == B_NO_INIT );
	CPPUNIT_ASSERT( path.Path() == NULL );
	path.Unset();
	entry.Unset();
	// bad args (badly initialized BEntry)
	NextSubTest();
	CPPUNIT_ASSERT( entry.SetTo(kBadMissingEntryPath) == B_ENTRY_NOT_FOUND );
	CPPUNIT_ASSERT( equals(path.SetTo(&entry), B_NO_INIT, B_ENTRY_NOT_FOUND) );
	CPPUNIT_ASSERT( equals(path.InitCheck(), B_NO_INIT, B_ENTRY_NOT_FOUND) );
	CPPUNIT_ASSERT( path.Path() == NULL );
	path.Unset();
	entry.Unset();
	// bad args (NULL BEntry)
	NextSubTest();
	CPPUNIT_ASSERT( equals(path.SetTo((const BEntry*)NULL), B_NO_INIT,
									  B_BAD_VALUE) );
	CPPUNIT_ASSERT( equals(path.InitCheck(), B_NO_INIT, B_BAD_VALUE) );
	CPPUNIT_ASSERT( path.Path() == NULL );
	path.Unset();
	entry.Unset();

	// 5. SetTo(const entry_ref*)
	// existing entry (root dir)
	NextSubTest();
	pathName = kRootPath;
	CPPUNIT_ASSERT( entry.SetTo(pathName) == B_OK );
	CPPUNIT_ASSERT( entry.GetRef(&ref) == B_OK );
	CPPUNIT_ASSERT( path.SetTo(&ref) == B_OK );
	CPPUNIT_ASSERT( path.InitCheck() == B_OK );
	CPPUNIT_ASSERT( PathsAreEquivalent(pathName, path.Path()) );
	path.Unset();
	entry.Unset();
	// existing entry
	NextSubTest();
	pathName = kUsrPath;
	CPPUNIT_ASSERT( entry.SetTo(pathName) == B_OK );
	CPPUNIT_ASSERT( entry.GetRef(&ref) == B_OK );
	CPPUNIT_ASSERT( path.SetTo(&ref) == B_OK );
	CPPUNIT_ASSERT( path.InitCheck() == B_OK );
	CPPUNIT_ASSERT( PathsAreEquivalent(pathName, path.Path()) );
	path.Unset();
	entry.Unset();
	// abstract entry
	NextSubTest();
	{
		string abstractPath = JoinPath(kUsrPath, "shouldn't exist");
		pathName = abstractPath.c_str();
		CPPUNIT_ASSERT( entry.SetTo(pathName) == B_OK );
		CPPUNIT_ASSERT( entry.GetRef(&ref) == B_OK );
		CPPUNIT_ASSERT( path.SetTo(&ref) == B_OK );
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		CPPUNIT_ASSERT( PathsAreEquivalent(pathName, path.Path()) );
	}
	path.Unset();
	entry.Unset();
	// bad args (NULL entry_ref)
	NextSubTest();
	CPPUNIT_ASSERT( equals(path.SetTo((const entry_ref*)NULL), B_NO_INIT,
						   B_BAD_VALUE) );
	CPPUNIT_ASSERT( equals(path.InitCheck(), B_NO_INIT, B_BAD_VALUE) );
	CPPUNIT_ASSERT( path.Path() == NULL );
	path.Unset();
	entry.Unset();
}

// AppendTest
void
PathTest::AppendTest()
{
	BPath path;
	// uninitialized BPath
	NextSubTest();
	CPPUNIT_ASSERT( path.InitCheck() == B_NO_INIT );
	CPPUNIT_ASSERT( path.Append("test") == B_BAD_VALUE );
	CPPUNIT_ASSERT( path.Path() == NULL );
	path.Unset();
	// dir hierarchy, from existing to non-existing
	NextSubTest();
	CPPUNIT_ASSERT( path.SetTo(kRootPath) == B_OK );
	CPPUNIT_ASSERT( string(kRootPath) == path.Path() );
	CPPUNIT_ASSERT( path.Append(kUsrLeaf) == B_OK );
	CPPUNIT_ASSERT( string(kUsrPath) == path.Path() );
	CPPUNIT_ASSERT( path.Append(kUsrLibLinuxFromUsrLeaf) == B_OK );
	CPPUNIT_ASSERT( string(kUsrLibLinuxPath) == path.Path() );
	CPPUNIT_ASSERT( path.Append("non/existing") == B_OK );
	CPPUNIT_ASSERT( JoinPath(kUsrLibLinuxPath, "non/existing") == path.Path() );
	// trigger normalization
	status_t err = path.Append("at/least/not//now");
	#ifdef _WIN32
	CPPUNIT_ASSERT( err == B_OK );
	CPPUNIT_ASSERT( path.InitCheck() == B_OK );
	CPPUNIT_ASSERT( JoinPath(kUsrLibLinuxPath,
		"non/existing/at/least/not/now") == path.Path() );
	#else
	CPPUNIT_ASSERT( err == B_ENTRY_NOT_FOUND );
	CPPUNIT_ASSERT( path.InitCheck() == B_ENTRY_NOT_FOUND );
	CPPUNIT_ASSERT( path.Path() == NULL );
	#endif
	path.Unset();
	// force normalization
	NextSubTest();
	CPPUNIT_ASSERT( path.SetTo(kRootPath) == B_OK );
	CPPUNIT_ASSERT( string(kRootPath) == path.Path() );
	CPPUNIT_ASSERT( path.Append(kHomeNonExistingFromRootLeaf, true) == B_OK );
	CPPUNIT_ASSERT( JoinPath(kHomePath, "non-existing") == path.Path() );
	#ifdef _WIN32
	CPPUNIT_ASSERT( path.Append("not/now", true) == B_OK );
	CPPUNIT_ASSERT( path.InitCheck() == B_OK );
	CPPUNIT_ASSERT( JoinPath(kHomePath, "non-existing/not/now")
		== path.Path() );
	#else
	CPPUNIT_ASSERT( path.Append("not/now", true) == B_ENTRY_NOT_FOUND );
	CPPUNIT_ASSERT( path.InitCheck() == B_ENTRY_NOT_FOUND );
	CPPUNIT_ASSERT( path.Path() == NULL );
	#endif
	path.Unset();
	// bad/strange args
	NextSubTest();
	CPPUNIT_ASSERT( path.SetTo(kRootPath) == B_OK );
	CPPUNIT_ASSERT( path.Append(NULL) == B_OK );
	CPPUNIT_ASSERT( string(kRootPath) == path.Path() );
	CPPUNIT_ASSERT( path.SetTo(kRootPath) == B_OK );
	CPPUNIT_ASSERT( path.Append(kAbsoluteAppendPath) == B_BAD_VALUE );
	CPPUNIT_ASSERT( path.InitCheck() == B_BAD_VALUE );
	CPPUNIT_ASSERT( path.SetTo(kRootPath) == B_OK );
	CPPUNIT_ASSERT( path.Append("") == B_OK );
	CPPUNIT_ASSERT( string(kRootPath) == path.Path() );
	path.Unset();
}

// LeafTest
void
PathTest::LeafTest()
{
	BPath path;
	// uninitialized BPath
	NextSubTest();
	CPPUNIT_ASSERT( path.InitCheck() == B_NO_INIT );
	CPPUNIT_ASSERT( path.Leaf() == NULL );
	path.Unset();
	// root dir
	NextSubTest();
	CPPUNIT_ASSERT( path.SetTo(kRootPath) == B_OK );
	CPPUNIT_ASSERT( string("") == path.Leaf() );
	path.Unset();
	// existing dirs
	NextSubTest();
	CPPUNIT_ASSERT( path.SetTo(kUsrLocalPath) == B_OK );
	CPPUNIT_ASSERT( string(kUsrLocalLeaf) == path.Leaf() );
	CPPUNIT_ASSERT( path.SetTo(kHomePath) == B_OK );
	CPPUNIT_ASSERT( string(kHomeLeaf) == path.Leaf() );
	path.Unset();
	// non-existing dirs
	NextSubTest();
	{
		string nonExistingPath = JoinPath(kRootPath, "non-existing");
		CPPUNIT_ASSERT( path.SetTo(nonExistingPath.c_str()) == B_OK );
	}
	CPPUNIT_ASSERT( string("non-existing") == path.Leaf() );
	{
		string nonExistingPath = JoinPath(kRootPath, "non/existing/dir");
		CPPUNIT_ASSERT( path.SetTo(nonExistingPath.c_str()) == B_OK );
	}
	CPPUNIT_ASSERT( string("dir") == path.Leaf() );
	path.Unset();
}

// ParentTest
void
PathTest::ParentTest()
{
	BPath path;
	BPath parent;
// R5: crashs, when GetParent() is called on uninitialized BPath
#if !TEST_R5
	// uninitialized BPath
	NextSubTest();
	CPPUNIT_ASSERT( path.InitCheck() == B_NO_INIT );
	CPPUNIT_ASSERT( path.GetParent(&parent) == B_NO_INIT );
	path.Unset();
	parent.Unset();
#endif
	// root dir
	NextSubTest();
	CPPUNIT_ASSERT( path.SetTo(kRootPath) == B_OK );
	#ifdef _WIN32
	CPPUNIT_ASSERT( path.GetParent(&parent) == B_OK );
	CPPUNIT_ASSERT( PathsAreEquivalent(kRootPath, parent.Path()) );
	#else
	CPPUNIT_ASSERT( path.GetParent(&parent) == B_ENTRY_NOT_FOUND );
	#endif
	path.Unset();
	parent.Unset();
	// existing dirs
	NextSubTest();
	CPPUNIT_ASSERT( path.SetTo(kHomePath) == B_OK );
	CPPUNIT_ASSERT( path.GetParent(&parent) == B_OK );
	CPPUNIT_ASSERT( string(kRootPath) == parent.Path() );
	// CPPUNIT_ASSERT( path.SetTo("/boot/home") == B_OK );
	// CPPUNIT_ASSERT( path.GetParent(&parent) == B_OK );
	// CPPUNIT_ASSERT( string("/boot") == parent.Path() );
	// CPPUNIT_ASSERT( path.SetTo("/boot/home/Desktop") == B_OK );
	// CPPUNIT_ASSERT( path.GetParent(&parent) == B_OK );
	// CPPUNIT_ASSERT( string("/boot/home") == parent.Path() );
	// path.Unset();
	// parent.Unset();
	// non-existing dirs
	NextSubTest();
	{
		string nonExistingPath = JoinPath(kRootPath, "non-existing");
		CPPUNIT_ASSERT( path.SetTo(nonExistingPath.c_str()) == B_OK );
	}
	CPPUNIT_ASSERT( path.GetParent(&parent) == B_OK );
	CPPUNIT_ASSERT( string(kRootPath) == parent.Path() );
	{
		string nonExistingPath = JoinPath(kRootPath, "non/existing/dir");
		CPPUNIT_ASSERT( path.SetTo(nonExistingPath.c_str()) == B_OK );
	}
	CPPUNIT_ASSERT( path.GetParent(&parent) == B_OK );
	CPPUNIT_ASSERT( JoinPath(kRootPath, "non/existing") == parent.Path() );
	path.Unset();
	parent.Unset();
	// destructive parenting
	NextSubTest();
	{
		string nonExistingPath = JoinPath(kRootPath, "non/existing/dir");
		CPPUNIT_ASSERT( path.SetTo(nonExistingPath.c_str()) == B_OK );
	}
	CPPUNIT_ASSERT( path.GetParent(&path) == B_OK );
	CPPUNIT_ASSERT( JoinPath(kRootPath, "non/existing") == path.Path() );
	CPPUNIT_ASSERT( path.GetParent(&path) == B_OK );
	CPPUNIT_ASSERT( JoinPath(kRootPath, "non") == path.Path() );
	CPPUNIT_ASSERT( path.GetParent(&path) == B_OK );
	CPPUNIT_ASSERT( string(kRootPath) == path.Path() );
	#ifdef _WIN32
	CPPUNIT_ASSERT( path.GetParent(&path) == B_OK );
	CPPUNIT_ASSERT( PathsAreEquivalent(kRootPath, path.Path()) );
	#else
	CPPUNIT_ASSERT( path.GetParent(&path) == B_ENTRY_NOT_FOUND );
	#endif
	path.Unset();
	parent.Unset();
// R5: crashs, when passing a NULL BPath
#if !TEST_R5
	// bad args
	NextSubTest();
	{
		string nonExistingPath = JoinPath(kRootPath, "non/existing/dir");
		CPPUNIT_ASSERT( path.SetTo(nonExistingPath.c_str()) == B_OK );
	}
	CPPUNIT_ASSERT( path.GetParent(NULL) == B_BAD_VALUE );
	path.Unset();
	parent.Unset();
#endif
}

// ComparisonTest
void
PathTest::ComparisonTest()
{
	BPath path;
	// 1. ==/!= const BPath &
	BPath path2;
	// uninitialized BPaths
// R5: uninitialized paths are unequal
	NextSubTest();
	CPPUNIT_ASSERT( path.InitCheck() == B_NO_INIT );
	CPPUNIT_ASSERT( path2.InitCheck() == B_NO_INIT );
#if !TEST_R5
	CPPUNIT_ASSERT( (path == path2) == true );
	CPPUNIT_ASSERT( (path != path2) == false );
#endif
	path.Unset();
	path2.Unset();
	// uninitialized argument BPath
	NextSubTest();
	CPPUNIT_ASSERT( path.SetTo(kRootPath) == B_OK );
	CPPUNIT_ASSERT( path2.InitCheck() == B_NO_INIT );
	CPPUNIT_ASSERT( (path == path2) == false );
	CPPUNIT_ASSERT( (path != path2) == true );
	path.Unset();
	path2.Unset();
	// uninitialized this BPath
	NextSubTest();
	CPPUNIT_ASSERT( path.InitCheck() == B_NO_INIT );
	CPPUNIT_ASSERT( path2.SetTo(kRootPath) == B_OK );
	CPPUNIT_ASSERT( (path == path2) == false );
	CPPUNIT_ASSERT( (path != path2) == true );
	path.Unset();
	path2.Unset();
	// various paths
	NextSubTest();
	const char *paths[] = { kRootPath, kHomePath, kUsrLibPath };
	int32 pathCount = sizeof(paths) / sizeof(const char*);
	for (int32 i = 0; i < pathCount; i++) {
		for (int32 k = 0; k < pathCount; k++) {
			CPPUNIT_ASSERT( path.SetTo(paths[i]) == B_OK );
			CPPUNIT_ASSERT( path2.SetTo(paths[k]) == B_OK );
			CPPUNIT_ASSERT( (path == path2) == (i == k) );
			CPPUNIT_ASSERT( (path != path2) == (i != k) );
		}
	}
	path.Unset();
	path2.Unset();

	// 2. ==/!= const char *
	const char *pathName;
	// uninitialized BPath
	NextSubTest();
	CPPUNIT_ASSERT( path.InitCheck() == B_NO_INIT );
	pathName = kRootPath;
	CPPUNIT_ASSERT( (path == pathName) == false );
	CPPUNIT_ASSERT( (path != pathName) == true );
	path.Unset();
	// various paths
	NextSubTest();
	for (int32 i = 0; i < pathCount; i++) {
		for (int32 k = 0; k < pathCount; k++) {
			CPPUNIT_ASSERT( path.SetTo(paths[i]) == B_OK );
			pathName = paths[k];
			CPPUNIT_ASSERT( (path == pathName) == (i == k) );
			CPPUNIT_ASSERT( (path != pathName) == (i != k) );
		}
	}
	path.Unset();
	// bad args (NULL const char*)
// R5: initialized path equals NULL argument!
// R5: uninitialized path does not equal NULL argument!
	NextSubTest();
	CPPUNIT_ASSERT( path.SetTo(kRootPath) == B_OK );
	pathName = NULL;
#if !TEST_R5
	CPPUNIT_ASSERT( (path == pathName) == false );
	CPPUNIT_ASSERT( (path != pathName) == true );
#endif
	path.Unset();
	CPPUNIT_ASSERT( path.InitCheck() == B_NO_INIT );
#if !TEST_R5
	CPPUNIT_ASSERT( (path == pathName) == true );
	CPPUNIT_ASSERT( (path != pathName) == false );
#endif
	path.Unset();
}

// AssignmentTest
void
PathTest::AssignmentTest()
{
	// 1. copy constructor
	// uninitialized
	NextSubTest();
	{
		BPath path;
		CPPUNIT_ASSERT( path.InitCheck() == B_NO_INIT );
		BPath path2(path);
		CPPUNIT_ASSERT( path2.InitCheck() == B_NO_INIT );
	}
	// initialized
	NextSubTest();
	{
		BPath path(kHomePath);
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		BPath path2(path);
		CPPUNIT_ASSERT( path2.InitCheck() == B_OK );
		CPPUNIT_ASSERT( path == path2 );
	}

	// 2. assignment operator, const BPath &
	// uninitialized
	NextSubTest();
	{
		BPath path;
		BPath path2;
		path2 = path;
		CPPUNIT_ASSERT( path.InitCheck() == B_NO_INIT );
		CPPUNIT_ASSERT( path2.InitCheck() == B_NO_INIT );
	}
	NextSubTest();
	{
		BPath path;
		BPath path2(kRootPath);
		CPPUNIT_ASSERT( path2.InitCheck() == B_OK );
		path2 = path;
		CPPUNIT_ASSERT( path.InitCheck() == B_NO_INIT );
		CPPUNIT_ASSERT( path2.InitCheck() == B_NO_INIT );
	}
	// initialized
	NextSubTest();
	{
		BPath path(kHomePath);
		CPPUNIT_ASSERT( path.InitCheck() == B_OK );
		BPath path2;
		path2 = path;
		CPPUNIT_ASSERT( path2.InitCheck() == B_OK );
		CPPUNIT_ASSERT( path == path2 );
	}

	// 2. assignment operator, const char *
	// initialized
	NextSubTest();
	{
		const char *pathName = kHomePath;
		BPath path2;
		path2 = pathName;
		CPPUNIT_ASSERT( path2.InitCheck() == B_OK );
		CPPUNIT_ASSERT( path2 == pathName );
	}
	// bad args
	NextSubTest();
	{
		const char *pathName = NULL;
		BPath path2;
		path2 = pathName;
		CPPUNIT_ASSERT( path2.InitCheck() == B_NO_INIT );
	}
}

// FlattenableTest
void
PathTest::FlattenableTest()
{
	BPath path;
	// 1. trivial methods (IsFixedSize(), TypeCode(), AllowsTypeCode)
	// uninitialized
	NextSubTest();
	CPPUNIT_ASSERT( path.InitCheck() == B_NO_INIT );
	CPPUNIT_ASSERT( path.IsFixedSize() == false );
	CPPUNIT_ASSERT( path.TypeCode() == B_REF_TYPE );
	CPPUNIT_ASSERT( path.AllowsTypeCode(B_REF_TYPE) == true );
	CPPUNIT_ASSERT( path.AllowsTypeCode(B_STRING_TYPE) == false );
	CPPUNIT_ASSERT( path.AllowsTypeCode(B_FLOAT_TYPE) == false );
	path.Unset();	
	// initialized
	NextSubTest();
	CPPUNIT_ASSERT( path.SetTo(kHomePath) == B_OK );
	CPPUNIT_ASSERT( path.IsFixedSize() == false );
	CPPUNIT_ASSERT( path.TypeCode() == B_REF_TYPE );
	CPPUNIT_ASSERT( path.AllowsTypeCode(B_REF_TYPE) == true );
	CPPUNIT_ASSERT( path.AllowsTypeCode(B_STRING_TYPE) == false );
	CPPUNIT_ASSERT( path.AllowsTypeCode(B_FLOAT_TYPE) == false );
	path.Unset();	

	// 2. non-trivial methods
	char buffer[1024];
	// uninitialized
	NextSubTest();
	CPPUNIT_ASSERT( path.InitCheck() == B_NO_INIT );
	ssize_t size = path.FlattenedSize();
	CPPUNIT_ASSERT( size == sizeof(dev_t) + sizeof(ino_t) );
	CPPUNIT_ASSERT( path.Flatten(buffer, sizeof(buffer)) == B_OK );
	CPPUNIT_ASSERT( path.Unflatten(B_REF_TYPE, buffer, size) == B_OK );
	CPPUNIT_ASSERT( path.InitCheck() == B_NO_INIT );
	path.Unset();
	// some flatten/unflatten tests
	NextSubTest();
	string nonExistingHomePath = JoinPath(kHomePath, "non-existing");
	const char *paths[] = { kRootPath, kHomePath,
						nonExistingHomePath.c_str() };
	int32 pathCount = sizeof(paths) / sizeof(const char*);
	for (int32 i = 0; i < pathCount; i++) {
		const char *pathName = paths[i];
		// init the path and get an equivalent entry ref
		CPPUNIT_ASSERT( path.SetTo(pathName) == B_OK );
		BEntry entry;
		CPPUNIT_ASSERT( entry.SetTo(pathName) == B_OK );
		entry_ref ref;
		CPPUNIT_ASSERT( entry.GetRef(&ref) == B_OK );
		// flatten the path
		struct flattened_ref { dev_t device; ino_t directory; char name[1]; };
		size = path.FlattenedSize();
		ssize_t expectedSize
			= sizeof(dev_t) + sizeof(ino_t) + strlen(ref.name) + 1;
		CPPUNIT_ASSERT( size ==  expectedSize);
		CPPUNIT_ASSERT( path.Flatten(buffer, sizeof(buffer)) == B_OK );
		// check the flattened data
		const flattened_ref &fref = *(flattened_ref*)buffer;
		CPPUNIT_ASSERT( ref.device == fref.device );
		CPPUNIT_ASSERT( ref.directory == fref.directory );
		CPPUNIT_ASSERT( strcmp(ref.name, fref.name) == 0 );
		// unflatten the path 
		path.Unset();
		CPPUNIT_ASSERT( path.InitCheck() == B_NO_INIT );
		CPPUNIT_ASSERT( path.Unflatten(B_REF_TYPE, buffer, size) == B_OK );
		CPPUNIT_ASSERT( PathsAreEquivalent(pathName, path.Path()) );
		path.Unset();
	}
	// bad args
	NextSubTest();
// R5: crashs, when passing a NULL buffer
// R5: doesn't check the buffer size
	CPPUNIT_ASSERT( path.SetTo(kHomePath) == B_OK );
#if !TEST_R5
	CPPUNIT_ASSERT( path.Flatten(NULL, sizeof(buffer)) == B_BAD_VALUE );
	CPPUNIT_ASSERT( path.Flatten(buffer, path.FlattenedSize() - 2)
					== B_BAD_VALUE );
#endif
	path.Unset();
}

