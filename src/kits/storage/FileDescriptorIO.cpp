/*
 * Copyright 2009-2010, Ingo Weinhold, ingo_weinhold@gmx.de.
 * Distributed under the terms of the MIT License.
 */


#include <FileDescriptorIO.h>

#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(_WIN32)
// Windows doesn't have native pread/pwrite functions
static ssize_t
pread(int fd, void* buffer, size_t size, off_t position)
{
	off_t oldPosition = lseek(fd, 0, SEEK_CUR);
	if (oldPosition < 0)
		return -1;

	if (lseek(fd, position, SEEK_SET) < 0)
		return -1;

	ssize_t bytesRead = read(fd, buffer, size);
	int error = bytesRead < 0 ? errno : B_OK;

	if (lseek(fd, oldPosition, SEEK_SET) < 0)
		return -1;

	if (bytesRead < 0)
		errno = error;

	return bytesRead;
}


static ssize_t
pwrite(int fd, const void* buffer, size_t size, off_t position)
{
	off_t oldPosition = lseek(fd, 0, SEEK_CUR);
	if (oldPosition < 0)
		return -1;

	if (lseek(fd, position, SEEK_SET) < 0)
		return -1;

	ssize_t bytesWritten = write(fd, buffer, size);
	int error = bytesWritten < 0 ? errno : B_OK;

	if (lseek(fd, oldPosition, SEEK_SET) < 0)
		return -1;

	if (bytesWritten < 0)
		errno = error;

	return bytesWritten;
}
#endif


BFileDescriptorIO::BFileDescriptorIO(int fd, bool takeOverOwnership)
	:
	fFD(fd),
	fOwnsFD(takeOverOwnership)
{
}


BFileDescriptorIO::~BFileDescriptorIO()
{
	if (fOwnsFD)
		close(fFD);
}


ssize_t
BFileDescriptorIO::Read(void* buffer, size_t size)
{
	ssize_t bytesRead = read(fFD, buffer, size);
	return bytesRead >= 0 ? bytesRead : errno;
}


ssize_t
BFileDescriptorIO::Write(const void* buffer, size_t size)
{
	ssize_t bytesWritten = write(fFD, buffer, size);
	return bytesWritten >= 0 ? bytesWritten : errno;
}


ssize_t
BFileDescriptorIO::ReadAt(off_t position, void* buffer, size_t size)
{
	ssize_t bytesRead = pread(fFD, buffer, size, position);
	return bytesRead >= 0 ? bytesRead : errno;
}


ssize_t
BFileDescriptorIO::WriteAt(off_t position, const void* buffer, size_t size)
{
	ssize_t bytesWritten = pwrite(fFD, buffer, size, position);
	return bytesWritten >= 0 ? bytesWritten : errno;
}


off_t
BFileDescriptorIO::Seek(off_t position, uint32 seekMode)
{
	off_t result = lseek(fFD, position, seekMode);
	return result >= 0 ? result : errno;
}


off_t
BFileDescriptorIO::Position() const
{
	off_t result = lseek(fFD, 0, SEEK_CUR);
	return result >= 0 ? result : errno;
}


status_t
BFileDescriptorIO::SetSize(off_t size)
{
	return ftruncate(fFD, size) == 0 ? B_OK : errno;
}


status_t
BFileDescriptorIO::GetSize(off_t* size) const
{
	struct stat st;
	if (fstat(fFD, &st) < 0)
		return errno;

	*size = st.st_size;
	return B_OK;
}
