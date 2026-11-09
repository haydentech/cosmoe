/*
 * Copyright 2026, Bill Hayden, hayden@haydentech.com.
 * Distributed under the terms of the MIT License.
 */
#ifndef _QUERY_ENGINE_H
#define _QUERY_ENGINE_H


#include <OS.h>
#include <SupportDefs.h>


namespace BPrivate {
namespace Storage {
namespace Query {


/*! \brief Delivers a B_QUERY_UPDATE message for a live query.

	The query engine lives in libroot, which cannot build BMessages; the
	Storage Kit installs a sender that knows how to. \a name is the entry's
	full path, following Cosmoe's entry_ref convention.
*/
typedef status_t (*QueryUpdateSender)(port_id port, int32 token, uint32 opcode,
	dev_t device, ino_t directory, ino_t node, const char* name);

//! Installs the sender used for live query notifications (NULL to disable).
void		set_query_update_sender(QueryUpdateSender sender);


}	// namespace Query
}	// namespace Storage
}	// namespace BPrivate


#endif	// _QUERY_ENGINE_H
