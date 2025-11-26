/*------------------------------------------------------------------------------
//	Copyright (c) 2004-2024, Bill Hayden
//
//	Permission is hereby granted, free of charge, to any person obtaining a
//	copy of this software and associated documentation files (the "Software"),
//	to deal in the Software without restriction, including without limitation
//	the rights to use, copy, modify, merge, publish, distribute, sublicense,
//	and/or sell copies of the Software, and to permit persons to whom the
//	Software is furnished to do so, subject to the following conditions:
//
//	The above copyright notice and this permission notice shall be included in
//	all copies or substantial portions of the Software.
//
//	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//	FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//	DEALINGS IN THE SOFTWARE.
//
//	File Name:		fs.cpp
//	Authors:		Bill Hayden (hayden@haydentech.com)
//----------------------------------------------------------------------------*/

#include <stdio.h>
#include <unistd.h>

#include <fs_attr.h>
#include <fs_info.h>

#include <TypeConstants.h>

#include "../../../config.h"

#if defined(HAVE_SYS_XATTR_H)
#include <sys/xattr.h>
#else
#warning Cosmoe does not support attributes on this platform
#endif


status_t _kstart_watching_vnode_(dev_t device, ino_t node,
								 uint32 flags, port_id port, int32 handlerToken);
status_t _kstop_watching_vnode_(dev_t device, ino_t node,
								port_id port, int32 handlerToken);
status_t _kstop_notifying_(port_id port, int32 handlerToken);



ssize_t  read_pos(int fd, off_t pos, void *buffer, size_t count)
{
	long origPos = lseek(fd, 0, SEEK_CUR);
	lseek(fd, pos, SEEK_SET);
	ssize_t result = read(fd, buffer, count);
	lseek(fd, origPos, SEEK_SET);
	return result;
}

ssize_t  write_pos(int fd, off_t pos, const void *buffer, size_t count)
{
	long origPos = lseek(fd, 0, SEEK_CUR);
	lseek(fd, pos, SEEK_SET);
	ssize_t result = write(fd, buffer, count);
	lseek(fd, origPos, SEEK_SET);
	return result;
}

dev_t dev_for_path(const char *path)
{
	printf( "Cosmoe: UNIMPLEMENTED: dev_for_path\n" );
	return B_FILE_ERROR;
}

dev_t next_dev(int32 *pos)
{
	printf( "Cosmoe: UNIMPLEMENTED: next_dev\n" );
	return B_BAD_VALUE;
}

int	fs_stat_dev(dev_t dev, fs_info *info)
{
	return -1;
}


ssize_t	fs_write_attr(int fd, const char *attribute, uint32 type, off_t pos, const void *buffer, size_t writeBytes)
{
	if (attribute && (strlen(attribute) > B_ATTR_NAME_LENGTH)) {
		// Setting errno a B_ value intentionally to match BeBook API
		errno = B_NAME_TOO_LONG;
		return (ssize_t)-1;
	}

#if defined(HAVE_SYS_XATTR_H)
	char attrName[NAME_MAX];
	snprintf(attrName, NAME_MAX, "user.%s", attribute);
	int err = fsetxattr(fd, attrName, buffer, writeBytes, 0);
	if (err != 0) {
		// Setting errno a B_ value intentionally to match BeBook API
		errno = B_BAD_VALUE;
		return (ssize_t)-1;
	}
	
	errno = 0;
	return (ssize_t)writeBytes;
#else
	printf( "Cosmoe: fs_write_attr UNSUPPORTED since xattr support was not compiled in\n" );
	errno = B_ERROR;
	return (ssize_t)-1;
#endif
}


ssize_t	fs_read_attr(int fd, const char *attribute, uint32 type, off_t pos, void *buffer, size_t readBytes)
{
	if (attribute && (strlen(attribute) > B_ATTR_NAME_LENGTH)) {
		// Setting errno a B_ value intentionally to match BeBook API
		errno = B_NAME_TOO_LONG;
		return (ssize_t)-1;
	}

#if defined(HAVE_SYS_XATTR_H)
	char attrName[B_ATTR_NAME_LENGTH];
	snprintf(attrName, B_ATTR_NAME_LENGTH, "user.%s", attribute);

	ssize_t err = fgetxattr(fd, attrName, buffer, readBytes);
	if (err < 0) {
		// Setting errno a B_ value intentionally to match BeBook API
		errno = B_ENTRY_NOT_FOUND;
		return (ssize_t)-1;
	}

	errno = 0;
	return (ssize_t)err;
#else
	printf( "Cosmoe: fs_read_attr UNSUPPORTED since xattr support was not compiled in\n" );
	errno = B_ERROR;
	return (ssize_t)-1;
#endif
}


int	fs_remove_attr(int fd, const char *attribute)
{
	if (attribute && (strlen(attribute) > B_ATTR_NAME_LENGTH)) {
		// Setting errno a B_ value intentionally to match BeBook API
		errno = B_NAME_TOO_LONG;
		return (ssize_t)-1;
	}

#if defined(HAVE_SYS_XATTR_H)
	char attrName[B_ATTR_NAME_LENGTH];
	snprintf(attrName, B_ATTR_NAME_LENGTH, "user.%s", attribute);
	int err = fremovexattr(fd, attrName);
	if (err < 0) {
		// Setting errno a B_ value intentionally to match BeBook API
		errno = B_ENTRY_NOT_FOUND;
		return -1;
	}

	errno = 0;
	return B_OK;
#else
	printf( "Cosmoe: fs_remove_attr UNSUPPORTED since xattr support was not compiled in\n" );
	return -1;
#endif
}


int	fs_stat_attr(int fd, const char *attribute, struct attr_info *attrInfo)
{
#if defined(HAVE_SYS_XATTR_H)
	char attrName[B_OS_NAME_LENGTH];
	snprintf(attrName, B_OS_NAME_LENGTH, "user.%s", attribute);

	int size = fgetxattr(fd, attrName, NULL, 0);

	if (size < 0)
		return B_ENTRY_NOT_FOUND;

	if (attrInfo) {
		attrInfo->size = size;
		attrInfo->type = B_RAW_TYPE;
	}

	return B_OK;
#else
	printf( "Cosmoe: fs_stat_attr UNSUPPORTED since xattr support was not compiled in\n" );
	return -1;
#endif
}


status_t _kstart_watching_vnode_(dev_t device, ino_t node,
											uint32 flags, port_id port,
											int32 handlerToken)
{
	return B_ERROR;
}

/*!	\brief Unsubscribes a target from watching a node.
	\param device The device the node resides on (node_ref::device).
	\param node The node ID of the node (node_ref::device).
	\param port The port of the target (a looper port).
	\param handlerToken The token of the target handler. \c -2, if the
		   preferred handler of the looper is the target.
	\return \c B_OK, if everything went fine, another error code otherwise.
*/
status_t _kstop_watching_vnode_(dev_t device, ino_t node,
										   port_id port, int32 handlerToken)
{
	return B_ERROR;
}


/*!	\brief Unsubscribes a target from node and mount monitoring.
	\param port The port of the target (a looper port).
	\param handlerToken The token of the target handler. \c -2, if the
		   preferred handler of the looper is the target.
	\return \c B_OK, if everything went fine, another error code otherwise.
*/
status_t _kstop_notifying_(port_id port, int32 handlerToken)
{
	return B_ERROR;
}


