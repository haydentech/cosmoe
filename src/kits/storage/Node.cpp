/*
 * Copyright 2002-2011 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Tyler Dauwalder
 *		Ingo Weinhold, bonefish@users.sf.net
 */


#include <Node.h>

#include <errno.h>
#include <fcntl.h>
#include <new>
#include <string.h>
#include <unistd.h>

#include <compat/sys/stat.h>

#include <Directory.h>
#include <Entry.h>
#include <fs_attr.h>
#include <String.h>
#include <TypeConstants.h>


#include "kernel_interface.h"
#include "storage_support.h"


//	#pragma mark - node_ref


node_ref::node_ref()
	:
	device((dev_t)-1),
	node((ino_t)-1)
{
}


node_ref::node_ref(dev_t device, ino_t node)
	:
	device(device),
	node(node)
{
}


node_ref::node_ref(const node_ref& other)
	:
	device((dev_t)-1),
	node((ino_t)-1)
{
	*this = other;
}


bool
node_ref::operator==(const node_ref& other) const
{
	return (device == other.device && node == other.node);
}


bool
node_ref::operator!=(const node_ref& other) const
{
	return !(*this == other);
}


bool
node_ref::operator<(const node_ref& other) const
{
	if (this->device != other.device)
		return this->device < other.device;

	return this->node < other.node;
}


node_ref&
node_ref::operator=(const node_ref& other)
{
	device = other.device;
	node = other.node;
	return *this;
}


//	#pragma mark - BNode


BNode::BNode()
	:
	fFd(-1),
	fAttrFd(-1),
	fCStatus(B_NO_INIT)
{
}


BNode::BNode(const entry_ref* ref)
	:
	fFd(-1),
	fAttrFd(-1),
	fCStatus(B_NO_INIT)
{
	// fCStatus is set by SetTo(), ignore return value
	(void)SetTo(ref);
}


BNode::BNode(const BEntry* entry)
	:
	fFd(-1),
	fAttrFd(-1),
	fCStatus(B_NO_INIT)
{
	// fCStatus is set by SetTo(), ignore return value
	(void)SetTo(entry);
}


BNode::BNode(const char* path)
	:
	fFd(-1),
	fAttrFd(-1),
	fCStatus(B_NO_INIT)
{
	// fCStatus is set by SetTo(), ignore return value
	(void)SetTo(path);
}


BNode::BNode(const BDirectory* dir, const char* path)
	:
	fFd(-1),
	fAttrFd(-1),
	fCStatus(B_NO_INIT)
{
	// fCStatus is set by SetTo(), ignore return value
	(void)SetTo(dir, path);
}


BNode::BNode(const BNode& node)
	:
	fFd(-1),
	fAttrFd(-1),
	fCStatus(B_NO_INIT)
{
	*this = node;
}


BNode::~BNode()
{
	Unset();
}


status_t
BNode::InitCheck() const
{
	return fCStatus;
}


status_t
BNode::SetTo(const entry_ref* ref)
{
	return _SetTo(ref, false);
}


status_t
BNode::SetTo(const BEntry* entry)
{
	if (entry == NULL) {
		Unset();
		return (fCStatus = B_BAD_VALUE);
	}

	return _SetTo(entry->fDirFd, entry->fName, false);
}


status_t
BNode::SetTo(const char* path)
{
	return _SetTo(-1, path, false);
}


status_t
BNode::SetTo(const BDirectory* dir, const char* path)
{
	if (dir == NULL || path == NULL
		|| BPrivate::Storage::is_absolute_path(path)) {
		Unset();
		return (fCStatus = B_BAD_VALUE);
	}

	return _SetTo(dir->fDirFd, path, false);
}


void
BNode::Unset()
{
	close_fd();
	fCStatus = B_NO_INIT;
}


status_t
BNode::Lock()
{
	if (fCStatus != B_OK)
		return fCStatus;

	// This will have to wait for the new kenel
	return B_FILE_ERROR;

	// We'll need to keep lock around if the kernel function
	// doesn't just work on file descriptors
//	BPrivate::Storage::FileLock lock;
//	return BPrivate::Storage::lock(fFd, BPrivate::Storage::READ_WRITE, &lock);
}


status_t
BNode::Unlock()
{
	if (fCStatus != B_OK)
		return fCStatus;
	// This will have to wait for the new kenel
	return B_FILE_ERROR;
}


status_t
BNode::Sync()
{
	return (fCStatus != B_OK) ? B_FILE_ERROR : BPrivate::Storage::sync(fFd) ;
}


ssize_t
BNode::WriteAttr(const char* attr, type_code type, off_t offset,
	const void* buffer, size_t length)
{
	if (fCStatus != B_OK)
		return B_FILE_ERROR;

	if (attr == NULL || buffer == NULL)
		return B_BAD_VALUE;

	ssize_t result = BPrivate::Storage::write_attr(fFd, attr, type, offset, buffer, length);

	return result < 0 ? errno : result;
}


ssize_t
BNode::ReadAttr(const char* attr, type_code type, off_t offset,
	void* buffer, size_t length) const
{
	if (fCStatus != B_OK)
		return B_FILE_ERROR;

	if (attr == NULL || buffer == NULL)
		return B_BAD_VALUE;
	ssize_t result = BPrivate::Storage::read_attr(fFd, attr, type, offset, buffer, length);

	return result == -1 ? errno : result;
}


status_t
BNode::RemoveAttr(const char* name)
{
	return (fCStatus != B_OK) ? B_FILE_ERROR : BPrivate::Storage::remove_attr(fFd, name);
}


status_t
BNode::RenameAttr(const char* oldName, const char* newName)
{
	if (fCStatus != B_OK)
		return B_FILE_ERROR;
	return BPrivate::Storage::rename_attr(fFd, oldName, newName);
}


status_t
BNode::GetAttrInfo(const char* name, struct attr_info* info) const
{
	if (fCStatus != B_OK)
		return B_FILE_ERROR;

	if (name == NULL || info == NULL)
		return B_BAD_VALUE;
	return BPrivate::Storage::stat_attr(fFd, name, info);
}


status_t
BNode::GetNextAttrName(char* buffer)
{
	// We're allowed to assume buffer is at least
	// B_ATTR_NAME_LENGTH chars long, but NULLs
	// are not acceptable.

	// BeOS R5 crashed when passed NULL
	if (buffer == NULL)
		return B_BAD_VALUE;

	if (InitAttrDir() != B_OK)
		return B_FILE_ERROR;
		
	BPrivate::Storage::LongDirEntry longEntry;
	struct dirent* entry = longEntry.dirent();
	status_t error = BPrivate::Storage::read_attr_dir(fAttrFd, *entry);
	if (error == B_OK) {
		strlcpy(buffer, entry->d_name, B_ATTR_NAME_LENGTH);
		return B_OK;
	}
	return error;
}


status_t
BNode::RewindAttrs()
{
	if (InitAttrDir() != B_OK)
		return B_FILE_ERROR;

	BPrivate::Storage::rewind_attr_dir(fAttrFd);
	return B_OK;
}


status_t
BNode::WriteAttrString(const char* name, const BString* data)
{
	status_t error = (!name || !data)  ? B_BAD_VALUE : B_OK;
	if (error == B_OK) {
		int32 length = data->Length() + 1;
		ssize_t sizeWritten = WriteAttr(name, B_STRING_TYPE, 0, data->String(),
			length);
		if (sizeWritten != length)
			error = sizeWritten;
	}

	return error;
}


status_t
BNode::ReadAttrString(const char* name, BString* result) const
{
	if (name == NULL || result == NULL)
		return B_BAD_VALUE;

	attr_info info;
	status_t error;

	error = GetAttrInfo(name, &info);
	if (error != B_OK)
		return error;

	// Lock the string's buffer so we can meddle with it
	char* data = result->LockBuffer(info.size + 1);
	if (data == NULL)
		return B_NO_MEMORY;

	// Read the attribute
	ssize_t bytes = ReadAttr(name, B_STRING_TYPE, 0, data, info.size);
	// Check for failure
	if (bytes < 0) {
		error = bytes;
		bytes = 0;
			// In this instance, we simply clear the string
	} else
		error = B_OK;

	// Null terminate the new string just to be sure (since it *is*
	// possible to read and write non-NULL-terminated strings)
	data[bytes] = 0;
	result->UnlockBuffer();

	return error;
}


BNode&
BNode::operator=(const BNode& node)
{
	// No need to do any assignment if already equal
	if (*this == node)
		return *this;

	// Close down out current state
	Unset();
	// We have to manually dup the node, because R5::BNode::Dup()
	// is not declared to be const (which IMO is retarded).
	fFd = BPrivate::Storage::dup(node.fFd);
	fCStatus = (fFd < 0) ? B_NO_INIT : B_OK ;

	return *this;
}


bool
BNode::operator==(const BNode& node) const
{
	if (fCStatus == B_NO_INIT && node.InitCheck() == B_NO_INIT)
		return true;

	if (fCStatus == B_OK && node.InitCheck() == B_OK) {
		// Check if they're identical
		BPrivate::Storage::Stat s1, s2;
		if (GetStat(&s1) != B_OK)
			return false;
		if (node.GetStat(&s2) != B_OK)
			return false;
		return (s1.st_dev == s2.st_dev && s1.st_ino == s2.st_ino);
	}

	return false;
}


bool
BNode::operator!=(const BNode& node) const
{
	return !(*this == node);
}


int
BNode::Dup()
{
	int fd = BPrivate::Storage::dup(fFd);

	return (fd >= 0 ? fd : -1);
		// comply with R5 return value
}


/*! (currently unused) */
void BNode::_RudeNode1() { }
void BNode::_RudeNode2() { }
void BNode::_RudeNode3() { }
void BNode::_RudeNode4() { }
void BNode::_RudeNode5() { }
void BNode::_RudeNode6() { }


/*!	Sets the node's file descriptor.

	Used by each implementation (i.e. BNode, BFile, BDirectory, etc.) to set
	the node's file descriptor. This allows each subclass to use the various
	file-type specific system calls for opening file descriptors.

	\note This method calls close_fd() to close previously opened FDs. Thus
		derived classes should take care to first call set_fd() and set
		class specific resources freed in their close_fd() version
		thereafter.

	\param fd the file descriptor this BNode should be set to (may be -1).

	\returns \c B_OK if everything went fine, or an error code if something
		went wrong.
*/
status_t
BNode::set_fd(int fd)
{
	if (fFd != -1)
		close_fd();

	fFd = fd;

	return B_OK;
}


/*!	Closes the node's file descriptor(s).

	To be implemented by subclasses to close the file descriptor using the
	proper system call for the given file-type. This implementation calls
	_kern_close(fFd) and also _kern_close(fAttrDir) if necessary.
*/
void
BNode::close_fd()
{
	if (fAttrFd >= 0) {
		BPrivate::Storage::close_attr_dir(fAttrFd);
		fAttrFd = -1;
	}
	if (fFd >= 0) {
		close(fFd);
		fFd = -1;
	}
}


/*!	Sets the BNode's status.

	To be used by derived classes instead of accessing the BNode's private
	\c fCStatus member directly.

	\param newStatus the new value for the status variable.
*/
void
BNode::set_status(status_t newStatus)
{
	fCStatus = newStatus;
}


/*!	Initializes the BNode's file descriptor to the node referred to
	by the given FD and path combo.

	\a path must either be \c NULL, an absolute or a relative path.
	In the first case, \a fd must not be \c NULL; the node it refers to will
	be opened. If absolute, \a fd is ignored. If relative and \a fd is >= 0,
	it will be reckoned off the directory identified by \a fd, otherwise off
	the current working directory.

	The method will first try to open the node with read and write permission.
	If that fails due to a read-only FS or because the user has no write
	permission for the node, it will re-try opening the node read-only.

	The \a fCStatus member will be set to the return value of this method.

	\param fd Either a directory FD or a value < 0. In the latter case \a path
	       must be specified.
	\param path Either \a NULL in which case \a fd must be given, absolute, or
	       relative to the directory specified by \a fd (if given) or to the
	       current working directory.
	\param traverse If the node identified by \a fd and \a path is a symlink
	       and \a traverse is \c true, the symlink will be resolved recursively.

	\returns \c B_OK if everything went fine, or an error code otherwise.
*/
status_t
BNode::_SetTo(int fd, const char* path, bool traverse)
{
	Unset();

	status_t error = (fd >= 0 || path ? B_OK : B_BAD_VALUE);
	if (error == B_OK) {
//FIXME
	}

	return error;
}


/*!	Initializes the BNode's file descriptor to the node referred to
	by the given entry_ref.

	The method will first try to open the node with read and write permission.
	If that fails due to a read-only FS or because the user has no write
	permission for the node, it will re-try opening the node read-only.

	The \a fCStatus member will be set to the return value of this method.

	\param ref An entry_ref identifying the node to be opened.
	\param traverse If the node identified by \a ref is a symlink and
	       \a traverse is \c true, the symlink will be resolved recursively.

	\returns \c B_OK if everything went fine, or an error code otherwise.
*/
status_t
BNode::_SetTo(const entry_ref* ref, bool traverse)
{
	Unset();

	status_t result = (ref ? B_OK : B_BAD_VALUE);
	if (result == B_OK) {
		//FIXME
	}
	return result;
}


/*!	Modifies a certain setting for this node based on \a what and the
	corresponding value in \a st.

	Inherited from and called by BStatable.

	\param st a stat structure containing the value to be set.
	\param what specifies what setting to be modified.

	\returns \c B_OK if everything went fine, or an error code otherwise.
*/
status_t
BNode::set_stat(struct stat& stat, uint32 what)
{
	if (fCStatus != B_OK)
		return B_FILE_ERROR;

	return BPrivate::Storage::set_stat(fFd, stat, what);
}



/*!	Verifies that the BNode has been properly initialized, and then
	(if necessary) opens the attribute directory on the node's file
	descriptor, storing it in fAttrDir.

	\returns \c B_OK if everything went fine, or an error code otherwise.
*/
status_t
BNode::InitAttrDir()
{
	if (fCStatus == B_OK && fAttrFd < 0) {
		return BPrivate::Storage::open_attr_dir(fFd, fAttrFd);
	}

	return fCStatus;
}


status_t
BNode::GetStat(struct stat* stat) const
{


	return (fCStatus != B_OK)
		? fCStatus
		: BPrivate::Storage::get_stat(fFd, stat);
}


status_t
BNode::_GetStat(struct stat_beos* stat) const
{
	struct stat newStat;
	status_t error = GetStat(&newStat);
	if (error != B_OK)
		return error;

	convert_to_stat_beos(&newStat, stat);

	return B_OK;
}


//	#pragma mark - symbol versions



