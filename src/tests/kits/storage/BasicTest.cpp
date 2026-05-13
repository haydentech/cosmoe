// BasicTest.cpp

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include <set>
using std::set;

#include "BasicTest.h"

// count_available_fds
static
int32
count_available_fds()
{
	set<int> fds;
	int fd;
	while ((fd = dup(1)) != -1)
		fds.insert(fd);
	for (set<int>::iterator it = fds.begin(); it != fds.end(); it++)
		close(*it);
	return fds.size();
}

// constructor
BasicTest::BasicTest()
		 : BTestCase(),
		   fSubTestNumber(0),
		   fAvailableFDs(0)
{
}

// setUp
void
BasicTest::setUp()
{
	BTestCase::setUp();
	fAvailableFDs = count_available_fds();
	SaveCWD();
	fSubTestNumber = 0;
}

// tearDown
void
BasicTest::tearDown()
{
	RestoreCWD();
	int32 availableFDs = count_available_fds();
	if (availableFDs != fAvailableFDs) {
		printf("WARNING: Number of available file descriptors has changed "
			   "during test: %d -> %d\n", fAvailableFDs, availableFDs);
		fAvailableFDs = availableFDs;
	}
	BTestCase::tearDown();
}

// execCommand
//
// Calls system() with the supplied string.
void
BasicTest::execCommand(const string &cmdLine)
{
	int result = system(cmdLine.c_str());
	if (result == -1) {
		CPPUNIT_FAIL("system() failed while executing test setup command");
	}

	if (WIFEXITED(result) && WEXITSTATUS(result) != 0) {
		if (cmdLine.length() > 200) {
			printf("execCommand failed (exit %d): %.200s... [len=%lu]\n",
				WEXITSTATUS(result), cmdLine.c_str(), cmdLine.length());
		} else {
			printf("execCommand failed (exit %d): %s\n", WEXITSTATUS(result),
				cmdLine.c_str());
		}
		// Some legacy test command chains intentionally include commands
		// that fail on certain hosts (e.g. overlong pathname probes).
	}

	if (WIFSIGNALED(result)) {
		printf("execCommand terminated by signal %d: %s\n", WTERMSIG(result),
			cmdLine.c_str());
		CPPUNIT_FAIL("setup/teardown command terminated by signal");
	}
}

// dumpStat
void
BasicTest::dumpStat(struct stat &st)
{
	printf("stat:\n");
	printf("  st_dev    : %lx\n", st.st_dev);
	printf("  st_ino    : %lx\n", st.st_ino);
	printf("  st_mode   : %x\n", st.st_mode);
	printf("  st_nlink  : %lx\n", st.st_nlink);
	printf("  st_uid    : %x\n", st.st_uid);
	printf("  st_gid    : %x\n", st.st_gid);
	printf("  st_size   : %ld\n", st.st_size);
#ifndef _WIN32
	printf("  st_blksize: %ld\n", st.st_blksize);
#endif
	printf("  st_atime  : %lx\n", st.st_atime);
	printf("  st_mtime  : %lx\n", st.st_mtime);
	//printf("  st_ctime  : %lx\n", st.st_ctime);
	//printf("  st_crtime : %lx\n", st.st_crtime);
}

// createVolume
void
BasicTest::createVolume(string imageFile, string mountPoint, int32 megs,
						bool makeMountPoint)
{
	char megsString[16];
	sprintf(megsString, "%d", megs);
	execCommand(string("dd if=/dev/zero of=") + imageFile
					+ " bs=1M count=" + megsString
					+ " &> /dev/null"
				+ " ; mkfs.ext4 " + imageFile
					+ " > /dev/null"
				+ " ; sync"
				+ (makeMountPoint ? " ; mkdir " + mountPoint : "")
				+ " ; mount " + imageFile + " " + mountPoint);
}

// deleteVolume
void
BasicTest::deleteVolume(string imageFile, string mountPoint,
						bool deleteMountPoint)
{
	execCommand(string("sync")
				+ " ; unmount " + mountPoint
				+ (deleteMountPoint ? " ; rmdir " + mountPoint : "")
				+ " ; rm " + imageFile);
}

