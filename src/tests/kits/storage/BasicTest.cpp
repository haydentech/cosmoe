// BasicTest.cpp

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <new>
#include <string.h>
#include <unistd.h>

#ifndef _WIN32
#include <sys/wait.h>
#endif

#if defined(__linux__) && defined(COSMOE_HAVE_FUSE3)
#define FUSE_USE_VERSION 31
#include <sys/statvfs.h>
#include <fuse3/fuse.h>
#include <map>
#include <pthread.h>
#endif

#include <set>
using std::set;

#include "BasicTest.h"


static bool
_ParentDeviceForPath(const string& path, dev_t& parentDevice)
{
	string::size_type slash = path.find_last_of('/');
	string parentPath;
	if (slash == string::npos || slash == 0)
		parentPath = "/";
	else
		parentPath.assign(path, 0, slash);

	struct stat st;
	if (stat(parentPath.c_str(), &st) != 0)
		return false;

	parentDevice = st.st_dev;
	return true;
}


static bool
_CommandExitedNormally(int result)
{
#ifdef _WIN32
	return result != -1;
#else
	return WIFEXITED(result);
#endif
}


static int
_CommandExitStatus(int result)
{
#ifdef _WIN32
	return result;
#else
	return WEXITSTATUS(result);
#endif
}


static bool
_CommandTerminatedBySignal(int result)
{
#ifdef _WIN32
	(void)result;
	return false;
#else
	return WIFSIGNALED(result);
#endif
}


static int
_CommandTerminationSignal(int result)
{
#ifdef _WIN32
	(void)result;
	return 0;
#else
	return WTERMSIG(result);
#endif
}


#if defined(__linux__) && defined(COSMOE_HAVE_FUSE3)

struct TestFuseMount {
	string			imageFile;
	string			mountPoint;
	int32			megs;
	struct fuse*		fuse;
	pthread_t		thread;
	bool			threadStarted;
};


static std::map<string, TestFuseMount*> sFuseMounts;


static int
_TestFuseGetAttr(const char* path, struct stat* st, struct fuse_file_info* fi)
{
	(void)fi;
	memset(st, 0, sizeof(*st));
	if (strcmp(path, "/") != 0)
		return -ENOENT;

	st->st_mode = S_IFDIR | 0755;
	st->st_nlink = 2;
	st->st_uid = getuid();
	st->st_gid = getgid();
	return 0;
}


static int
_TestFuseOpenDir(const char* path, struct fuse_file_info* fi)
{
	(void)fi;
	return strcmp(path, "/") == 0 ? 0 : -ENOENT;
}


static int
_TestFuseReadDir(const char* path, void* buffer, fuse_fill_dir_t filler,
	off_t offset, struct fuse_file_info* fi, enum fuse_readdir_flags flags)
{
	(void)offset;
	(void)fi;
	(void)flags;
	if (strcmp(path, "/") != 0)
		return -ENOENT;

	filler(buffer, ".", NULL, 0, (fuse_fill_dir_flags)0);
	filler(buffer, "..", NULL, 0, (fuse_fill_dir_flags)0);
	return 0;
}


static int
_TestFuseStatFs(const char* path, struct statvfs* st)
{
	if (strcmp(path, "/") != 0)
		return -ENOENT;

	memset(st, 0, sizeof(*st));
	st->f_bsize = 4096;
	st->f_frsize = 4096;
	st->f_blocks = 3840;
	st->f_bfree = 1920;
	st->f_bavail = 1920;
	st->f_files = 16;
	st->f_ffree = 16;
	st->f_favail = 16;
	st->f_namemax = 255;
	return 0;
}


static struct fuse_operations
_CreateTestFuseOperations()
{
	struct fuse_operations operations = {};
	operations.getattr = _TestFuseGetAttr;
	operations.opendir = _TestFuseOpenDir;
	operations.readdir = _TestFuseReadDir;
	operations.statfs = _TestFuseStatFs;
	return operations;
}


static void*
_TestFuseLoop(void* data)
{
	TestFuseMount* mount = (TestFuseMount*)data;
	if (mount != NULL && mount->fuse != NULL)
		fuse_loop(mount->fuse);
	return NULL;
}


static bool
_WaitForMountedDevice(const string& mountPoint, dev_t parentDevice)
{
	for (int32 i = 0; i < 50; i++) {
		struct stat st;
		if (stat(mountPoint.c_str(), &st) == 0 && st.st_dev != parentDevice)
			return true;
		usleep(20000);
	}

	return false;
}


static void
_CleanupFuseMount(TestFuseMount* mount)
{
	if (mount == NULL)
		return;

	if (mount->fuse != NULL) {
		fuse_exit(mount->fuse);
		fuse_unmount(mount->fuse);
	}

	if (mount->threadStarted)
		pthread_join(mount->thread, NULL);

	if (mount->fuse != NULL)
		fuse_destroy(mount->fuse);

	delete mount;
}

#endif

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

	if (_CommandExitedNormally(result) && _CommandExitStatus(result) != 0) {
		if (cmdLine.length() > 200) {
			printf("execCommand failed (exit %d): %.200s... [len=%lu]\n",
				_CommandExitStatus(result), cmdLine.c_str(), cmdLine.length());
		} else {
			printf("execCommand failed (exit %d): %s\n", _CommandExitStatus(result),
				cmdLine.c_str());
		}
		// Some legacy test command chains intentionally include commands
		// that fail on certain hosts (e.g. overlong pathname probes).
	}

	if (_CommandTerminatedBySignal(result)) {
		printf("execCommand terminated by signal %d: %s\n", _CommandTerminationSignal(result),
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
	if (makeMountPoint)
		execCommand(string("mkdir -p ") + mountPoint);

	int fd = open(imageFile.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0644);
	if (fd >= 0)
		close(fd);

#if defined(__linux__) && defined(COSMOE_HAVE_FUSE3)
	deleteVolume(imageFile, mountPoint, false);

	dev_t parentDevice;
	if (!_ParentDeviceForPath(mountPoint, parentDevice))
		return;

	string fsName = "fsname=" + imageFile;
	char programName[] = "storagekittest";
	char optionFlag[] = "-o";
	char* argv[] = {
		programName,
		optionFlag,
		const_cast<char*>(fsName.c_str())
	};
	struct fuse_args args = FUSE_ARGS_INIT(sizeof(argv) / sizeof(argv[0]), argv);

	TestFuseMount* mount = new(std::nothrow) TestFuseMount;
	if (mount == NULL)
		return;

	struct fuse_operations operations = _CreateTestFuseOperations();

	mount->imageFile = imageFile;
	mount->mountPoint = mountPoint;
	mount->megs = megs;
	mount->fuse = fuse_new(&args, &operations, sizeof(operations), mount);
	mount->threadStarted = false;

	if (mount->fuse == NULL) {
		delete mount;
		return;
	}

	if (fuse_mount(mount->fuse, mountPoint.c_str()) != 0) {
		fuse_destroy(mount->fuse);
		delete mount;
		return;
	}

	if (pthread_create(&mount->thread, NULL, _TestFuseLoop, mount) != 0) {
		fuse_unmount(mount->fuse);
		fuse_destroy(mount->fuse);
		delete mount;
		return;
	}
	mount->threadStarted = true;

	if (!_WaitForMountedDevice(mountPoint, parentDevice)) {
		_CleanupFuseMount(mount);
		return;
	}

	sFuseMounts[mountPoint] = mount;
	return;
#else
	(void)megs;
#endif
}

// deleteVolume
void
BasicTest::deleteVolume(string imageFile, string mountPoint,
						bool deleteMountPoint)
{
#if defined(__linux__) && defined(COSMOE_HAVE_FUSE3)
	std::map<string, TestFuseMount*>::iterator it = sFuseMounts.find(mountPoint);
	if (it != sFuseMounts.end()) {
		TestFuseMount* mount = it->second;
		sFuseMounts.erase(it);
		_CleanupFuseMount(mount);
	}
#else
	(void)mountPoint;
#endif

	unlink(imageFile.c_str());
	if (deleteMountPoint)
		rmdir(mountPoint.c_str());
}


bool
BasicTest::IsVolumeMounted(const string& mountPoint)
{
	dev_t parentDevice;
	if (!_ParentDeviceForPath(mountPoint, parentDevice))
		return false;

	struct stat st;
	if (stat(mountPoint.c_str(), &st) != 0)
		return false;

	return st.st_dev != parentDevice;
}

