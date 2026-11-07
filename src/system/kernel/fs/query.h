/*
 * Copyright 2026, Bill Hayden, hayden@haydentech.com.
 * Distributed under the terms of the MIT License.
 */
#ifndef KERNEL_QUERY_H
#define KERNEL_QUERY_H


#include <OS.h>
#include <SupportDefs.h>


struct dirent;

namespace BPrivate {
namespace Storage {
namespace Query {


/*! Opens a query on \a device, walks the volume and returns a descriptor
	positioned at the first result, or -1 with errno set.
*/
int			open_query(dev_t device, const char* predicate, size_t length,
				uint32 flags, port_id port, int32 token);

//! Returns whether \a fd is a query result descriptor.
bool		is_query_fd(int fd);

/*! Reads the next result as a dirent holding the entry's full path.
	\return 1 on success, 0 at the end of the results, an error code on
		failure.
*/
ssize_t		read_query_dir(int fd, struct dirent* buffer, size_t length,
				uint32 maxCount);

//! Forgets \a fd; the caller still closes the descriptor itself.
status_t	close_query(int fd);


}	// namespace Query
}	// namespace Storage
}	// namespace BPrivate


#endif	// KERNEL_QUERY_H
