/*
 * BPath Windows Compatibility Test
 * 
 * Tests BPath behavior with Windows-style paths, including:
 * - Drive letters (C:\, D:\, etc.)
 * - Backslashes vs forward slashes
 * - UNC paths (\\server\share)
 * - Parent directory navigation
 * - Path normalization
 * - Mixed separators
 */

#include <stdio.h>
#include <string.h>
#include <Path.h>
#include <Entry.h>
#include <Directory.h>

// Test result tracking
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_START(name) \
	do { \
		printf("\n=== Test: %s ===\n", name); \
		tests_run++; \
	} while(0)

#define TEST_ASSERT(condition, message) \
	do { \
		if (condition) { \
			printf("  ✓ PASS: %s\n", message); \
			tests_passed++; \
		} else { \
			printf("  ✗ FAIL: %s\n", message); \
			tests_failed++; \
		} \
	} while(0)

#define TEST_EQUAL_STR(actual, expected, message) \
	do { \
		if (strcmp(actual, expected) == 0) { \
			printf("  ✓ PASS: %s\n", message); \
			printf("    Expected: '%s', Got: '%s'\n", expected, actual); \
			tests_passed++; \
		} else { \
			printf("  ✗ FAIL: %s\n", message); \
			printf("    Expected: '%s', Got: '%s'\n", expected, actual); \
			tests_failed++; \
		} \
	} while(0)

void test_basic_windows_paths()
{
	TEST_START("Basic Windows Paths");
	
	BPath path;
	
	// Test simple Windows path
	status_t err = path.SetTo("C:\\Windows\\System32");
	TEST_ASSERT(err == B_OK, "SetTo() with C:\\Windows\\System32");
	printf("    Path: '%s'\n", path.Path());
	
	// Test forward slashes (should work too)
	err = path.SetTo("C:/Windows/System32");
	TEST_ASSERT(err == B_OK, "SetTo() with C:/Windows/System32");
	printf("    Path: '%s'\n", path.Path());
	
	// Test mixed separators
	err = path.SetTo("C:\\Windows/System32\\drivers/etc");
	TEST_ASSERT(err == B_OK, "SetTo() with mixed separators");
	printf("    Path: '%s'\n", path.Path());
	
	// Test different drive letters
	err = path.SetTo("D:\\Data\\Documents");
	TEST_ASSERT(err == B_OK, "SetTo() with D: drive");
	printf("    Path: '%s'\n", path.Path());
}

void test_parent_directory()
{
	TEST_START("Parent Directory Navigation");
	
	BPath path;
	
	// Start with a deep path
	path.SetTo("C:\\Users\\billh\\Documents\\Projects\\test.txt");
	printf("    Initial: '%s'\n", path.Path());
	
	// Navigate to parent
	BPath parent;
	status_t err = path.GetParent(&parent);
	TEST_ASSERT(err == B_OK, "GetParent() returns B_OK");
	printf("    Parent:  '%s'\n", parent.Path());
	
	// Continue up
	BPath grandparent;
	err = parent.GetParent(&grandparent);
	TEST_ASSERT(err == B_OK, "GetParent() again returns B_OK");
	printf("    Grandparent: '%s'\n", grandparent.Path());
	
	// Navigate to root
	BPath root;
	err = grandparent.GetParent(&root);
	TEST_ASSERT(err == B_OK, "Navigate to Users");
	err = root.GetParent(&root);
	TEST_ASSERT(err == B_OK, "Navigate to C:\\");
	printf("    Root:    '%s'\n", root.Path());
	
	// Try to go past root (should fail or return same)
	BPath beyond;
	err = root.GetParent(&beyond);
	printf("    Beyond root status: %ld, path: '%s'\n", err, beyond.Path());
}

void test_path_components()
{
	TEST_START("Path Component Access");
	
	BPath path("C:\\Program Files\\MyApp\\bin\\app.exe");
	printf("    Full path: '%s'\n", path.Path());
	
	// Get leaf name
	const char* leaf = path.Leaf();
	TEST_ASSERT(leaf != NULL, "Leaf() returns non-NULL");
	if (leaf) {
		printf("    Leaf: '%s'\n", leaf);
		TEST_ASSERT(strcmp(leaf, "app.exe") == 0 || 
		            strstr(leaf, "app.exe") != NULL, 
		            "Leaf is 'app.exe'");
	}
}

void test_path_append()
{
	TEST_START("Path Append Operations");
	
	BPath path("C:\\Users\\billh");
	printf("    Base: '%s'\n", path.Path());
	
	// Append relative path
	status_t err = path.Append("Documents");
	TEST_ASSERT(err == B_OK, "Append('Documents')");
	printf("    After append: '%s'\n", path.Path());
	
	// Append multiple components
	err = path.Append("Projects/MyProject");
	TEST_ASSERT(err == B_OK, "Append('Projects/MyProject')");
	printf("    After append: '%s'\n", path.Path());
	
	// Append with backslashes
	err = path.Append("src\\main.cpp");
	TEST_ASSERT(err == B_OK, "Append('src\\\\main.cpp')");
	printf("    Final: '%s'\n", path.Path());
}

void test_normalization()
{
	TEST_START("Path Normalization");
	
	BPath path;
	
	// Path with . and ..
	status_t err = path.SetTo("C:\\Users\\billh\\Documents\\..\\Downloads");
	TEST_ASSERT(err == B_OK, "SetTo() with '..' component");
	printf("    Input:  'C:\\\\Users\\\\billh\\\\Documents\\\\..\\\\Downloads'\n");
	printf("    Result: '%s'\n", path.Path());
	
	// Multiple .. components
	err = path.SetTo("C:\\Users\\billh\\Documents\\..\\..\\Public");
	TEST_ASSERT(err == B_OK, "SetTo() with multiple '..'");
	printf("    Input:  'C:\\\\Users\\\\billh\\\\Documents\\\\..\\\\..\\\\Public'\n");
	printf("    Result: '%s'\n", path.Path());
	
	// . component
	err = path.SetTo("C:\\Windows\\.\\System32");
	TEST_ASSERT(err == B_OK, "SetTo() with '.' component");
	printf("    Input:  'C:\\\\Windows\\\\.\\\\System32'\n");
	printf("    Result: '%s'\n", path.Path());
}

void test_absolute_vs_relative()
{
	TEST_START("Absolute vs Relative Paths");
	
	BPath path;
	
	// Absolute Windows path
	path.SetTo("C:\\Windows");
	TEST_ASSERT(path.Path() != NULL && strlen(path.Path()) > 0, 
	            "Absolute path C:\\Windows");
	printf("    Absolute: '%s'\n", path.Path());
	
	// Relative path
	status_t err = path.SetTo("Documents\\file.txt");
	printf("    Relative: '%s' (status=%ld)\n", 
	       path.Path() ? path.Path() : "(null)", err);
	
	// Current directory relative
	err = path.SetTo(".\\Documents");
	printf("    Current dir relative: '%s' (status=%ld)\n",
	       path.Path() ? path.Path() : "(null)", err);
}

void test_unc_paths()
{
	TEST_START("UNC Path Support");
	
	BPath path;
	
	// UNC path
	status_t err = path.SetTo("\\\\server\\share\\folder\\file.txt");
	printf("    UNC path status: %ld\n", err);
	if (err == B_OK && path.Path()) {
		printf("    Path: '%s'\n", path.Path());
		TEST_ASSERT(true, "UNC path accepted");
	} else {
		printf("    UNC paths may not be supported (expected)\n");
	}
}

void test_special_paths()
{
	TEST_START("Special Windows Paths");
	
	BPath path;
	
	// Program Files with space
	status_t err = path.SetTo("C:\\Program Files\\MyApp");
	TEST_ASSERT(err == B_OK, "Path with spaces");
	printf("    With spaces: '%s'\n", path.Path());
	
	// Very long path component
	err = path.SetTo("C:\\ThisIsAVeryLongDirectoryNameThatMightCauseIssues\\file.txt");
	TEST_ASSERT(err == B_OK, "Long directory name");
	printf("    Long name: '%s'\n", path.Path());
	
	// Multiple sequential separators
	err = path.SetTo("C:\\\\\\Windows\\\\\\System32");
	TEST_ASSERT(err == B_OK, "Multiple sequential separators");
	printf("    Multiple separators: '%s'\n", path.Path());
}

void test_path_comparison()
{
	TEST_START("Path Comparison");
	
	BPath path1("C:\\Windows\\System32");
	BPath path2("C:\\Windows\\System32");
	BPath path3("C:/Windows/System32");  // Same but with forward slashes
	BPath path4("C:\\Windows\\SysWOW64");
	
	TEST_ASSERT(path1 == path2, "Same paths are equal");
	TEST_ASSERT(path1 != path4, "Different paths are not equal");
	
	// Test if forward/backslash normalization works
	printf("    Backslash path: '%s'\n", path1.Path());
	printf("    Forward slash path: '%s'\n", path3.Path());
}

void test_initialization_methods()
{
	TEST_START("Various Initialization Methods");
	
	// Constructor with path
	BPath path1("C:\\Windows");
	TEST_ASSERT(path1.Path() != NULL, "Constructor with path string");
	printf("    Constructor: '%s'\n", path1.Path());
	
	// Copy constructor
	BPath path2(path1);
	TEST_ASSERT(path2.Path() != NULL, "Copy constructor");
	printf("    Copy: '%s'\n", path2.Path());
	
	// Assignment
	BPath path3;
	path3 = path1;
	TEST_ASSERT(path3.Path() != NULL, "Assignment operator");
	printf("    Assignment: '%s'\n", path3.Path());
	
	// SetTo
	BPath path4;
	path4.SetTo("D:\\Data");
	TEST_ASSERT(path4.Path() != NULL, "SetTo method");
	printf("    SetTo: '%s'\n", path4.Path());
}

void test_edge_cases()
{
	TEST_START("Edge Cases");
	
	BPath path;
	
	// Empty path
	status_t err = path.SetTo("");
	printf("    Empty string: status=%ld, path='%s'\n", 
	       err, path.Path() ? path.Path() : "(null)");
	
	// NULL path
	err = path.SetTo((const char*)NULL);
	printf("    NULL: status=%ld\n", err);
	TEST_ASSERT(err != B_OK, "NULL path rejected");
	
	// Just drive letter
	err = path.SetTo("C:");
	printf("    Just 'C:': status=%ld, path='%s'\n",
	       err, path.Path() ? path.Path() : "(null)");
	
	// Drive with backslash
	err = path.SetTo("C:\\");
	TEST_ASSERT(err == B_OK, "Drive root 'C:\\\\'");
	printf("    'C:\\\\': '%s'\n", path.Path());
	
	// Trailing separator
	err = path.SetTo("C:\\Windows\\System32\\");
	printf("    Trailing separator: '%s'\n", path.Path());
}

int main(int argc, char** argv)
{
	printf("╔════════════════════════════════════════════════════════════╗\n");
	printf("║         BPath Windows Compatibility Test Suite            ║\n");
	printf("╚════════════════════════════════════════════════════════════╝\n");
	
#ifdef _WIN32
	printf("\nRunning on: Windows (native or Wine)\n");
#else
	printf("\nRunning on: Unix-like system\n");
#endif
	
	// Run all tests
	test_basic_windows_paths();
	test_parent_directory();
	test_path_components();
	test_path_append();
	test_normalization();
	test_absolute_vs_relative();
	test_unc_paths();
	test_special_paths();
	test_path_comparison();
	test_initialization_methods();
	test_edge_cases();
	
	// Summary
	printf("\n");
	printf("╔════════════════════════════════════════════════════════════╗\n");
	printf("║                      Test Summary                          ║\n");
	printf("╠════════════════════════════════════════════════════════════╣\n");
	printf("║  Total Tests:  %-4d                                        ║\n", tests_run);
	printf("║  Passed:       %-4d  ✓                                     ║\n", tests_passed);
	printf("║  Failed:       %-4d  ✗                                     ║\n", tests_failed);
	printf("╚════════════════════════════════════════════════════════════╝\n");
	
	if (tests_failed == 0) {
		printf("\n🎉 All tests passed!\n");
		return 0;
	} else {
		printf("\n⚠️  Some tests failed. Review output above.\n");
		return 1;
	}
}
