/*
 * Copyright 2026, Bill Hayden
 * Distributed under the terms of the MIT License.
 */

#include <MediaDefs.h>

#include <MediaNode.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>


media_destination media_destination::null = media_destination();
media_source media_source::null = media_source();

const media_multi_audio_format media_multi_audio_format::wildcard = {
	{0.0f, 0, 0, 0, 0},
	{0, 0, 0, {0, 0, 0}}
};

const media_multi_audio_format media_raw_audio_format::wildcard
	= media_multi_audio_format::wildcard;

const char* B_MEDIA_SERVER_SIGNATURE = "application/x-vnd.Cosmoe-media-server";


media_destination::media_destination()
	:	port(-1),
		id(-1),
		_reserved_media_destination_{0, 0}
{
}


media_destination::media_destination(port_id portId, int32 destinationId)
	:	port(portId),
		id(destinationId),
		_reserved_media_destination_{0, 0}
{
}


media_destination::media_destination(const media_destination& other)
	:	port(other.port),
		id(other.id),
		_reserved_media_destination_{0, 0}
{
}


media_destination::~media_destination()
{
}


media_destination&
media_destination::operator=(const media_destination& other)
{
	if (this == &other)
		return *this;

	port = other.port;
	id = other.id;
	return *this;
}


media_source::media_source()
	:	port(-1),
		id(-1),
		_reserved_media_source_{0, 0}
{
}


media_source::media_source(port_id portId, int32 sourceId)
	:	port(portId),
		id(sourceId),
		_reserved_media_source_{0, 0}
{
}


media_source::media_source(const media_source& other)
	:	port(other.port),
		id(other.id),
		_reserved_media_source_{0, 0}
{
}


media_source::~media_source()
{
}


media_source&
media_source::operator=(const media_source& other)
{
	if (this == &other)
		return *this;

	port = other.port;
	id = other.id;
	return *this;
}


bool
operator==(const media_destination& a, const media_destination& b)
{
	return a.port == b.port && a.id == b.id;
}


bool
operator!=(const media_destination& a, const media_destination& b)
{
	return !(a == b);
}


bool
operator<(const media_destination& a, const media_destination& b)
{
	return a.port < b.port || (a.port == b.port && a.id < b.id);
}


bool
operator==(const media_source& a, const media_source& b)
{
	return a.port == b.port && a.id == b.id;
}


bool
operator!=(const media_source& a, const media_source& b)
{
	return !(a == b);
}


bool
operator<(const media_source& a, const media_source& b)
{
	return a.port < b.port || (a.port == b.port && a.id < b.id);
}


media_format::media_format()
	:	type(B_MEDIA_NO_TYPE),
		user_data_type(0),
		user_data{0},
		_reserved_{0, 0, 0},
		require_flags(0),
		deny_flags(0),
		meta_data(NULL),
		meta_data_size(0),
		meta_data_area(-1),
		__unused_was_use_area(-1),
		__unused_was_team(-1),
		__unused_was_thisPtr(NULL),
		u{}
{
}


media_format::media_format(const media_format& other)
	:	media_format()
{
	*this = other;
}


media_format::~media_format()
{
	std::free(meta_data);
}


media_format&
media_format::operator=(const media_format& other)
{
	if (this == &other)
		return *this;

	std::free(meta_data);
	meta_data = NULL;
	meta_data_size = 0;

	type = other.type;
	user_data_type = other.user_data_type;
	std::memcpy(user_data, other.user_data, sizeof(user_data));
	std::memcpy(_reserved_, other._reserved_, sizeof(_reserved_));
	require_flags = other.require_flags;
	deny_flags = other.deny_flags;
	meta_data_area = other.meta_data_area;
	__unused_was_use_area = other.__unused_was_use_area;
	__unused_was_team = other.__unused_was_team;
	__unused_was_thisPtr = other.__unused_was_thisPtr;
	u = other.u;

	if (other.meta_data != NULL && other.meta_data_size > 0) {
		meta_data = std::malloc(other.meta_data_size);
		if (meta_data != NULL) {
			std::memcpy(meta_data, other.meta_data, other.meta_data_size);
			meta_data_size = other.meta_data_size;
		}
	}

	return *this;
}


bool
media_format::Matches(const media_format* other) const
{
	return other != NULL && *this == *other;
}


void
media_format::SpecializeTo(const media_format* other)
{
	if (other != NULL)
		*this = *other;
}


status_t
media_format::SetMetaData(const void* data, size_t size)
{
	std::free(meta_data);
	meta_data = NULL;
	meta_data_size = 0;

	if (data == NULL || size == 0)
		return B_OK;

	meta_data = std::malloc(size);
	if (meta_data == NULL)
		return B_NO_MEMORY;

	std::memcpy(meta_data, data, size);
	meta_data_size = (int32)size;
	return B_OK;
}


const void*
media_format::MetaData() const
{
	return meta_data;
}


int32
media_format::MetaDataSize() const
{
	return meta_data_size;
}


void
media_format::Unflatten(const char* flatBuffer)
{
	if (flatBuffer != NULL)
		std::memcpy(this, flatBuffer, sizeof(*this));
}


void
media_format::Clear()
{
	std::free(meta_data);
	meta_data = NULL;
	meta_data_size = 0;
	type = B_MEDIA_NO_TYPE;
	user_data_type = 0;
	std::memset(user_data, 0, sizeof(user_data));
	std::memset(_reserved_, 0, sizeof(_reserved_));
	require_flags = 0;
	deny_flags = 0;
	meta_data_area = -1;
	__unused_was_use_area = -1;
	__unused_was_team = -1;
	__unused_was_thisPtr = NULL;
	std::memset(&u, 0, sizeof(u));
}


bool
operator==(const media_raw_audio_format& a, const media_raw_audio_format& b)
{
	return a.frame_rate == b.frame_rate && a.channel_count == b.channel_count
		&& a.format == b.format && a.byte_order == b.byte_order
		&& a.buffer_size == b.buffer_size;
}


bool
operator==(const media_multi_audio_info& a, const media_multi_audio_info& b)
{
	return a.channel_mask == b.channel_mask && a.valid_bits == b.valid_bits
		&& a.matrix_mask == b.matrix_mask;
}


bool
operator==(const media_multi_audio_format& a, const media_multi_audio_format& b)
{
	return static_cast<const media_raw_audio_format&>(a)
		== static_cast<const media_raw_audio_format&>(b)
		&& static_cast<const media_multi_audio_info&>(a)
		== static_cast<const media_multi_audio_info&>(b);
}


bool
operator==(const media_format& a, const media_format& b)
{
	if (a.type != b.type || a.user_data_type != b.user_data_type
		|| a.require_flags != b.require_flags || a.deny_flags != b.deny_flags)
		return false;

	if (std::memcmp(a.user_data, b.user_data, sizeof(a.user_data)) != 0)
		return false;

	if (a.MetaDataSize() != b.MetaDataSize())
		return false;

	if (a.MetaDataSize() > 0
		&& std::memcmp(a.MetaData(), b.MetaData(), a.MetaDataSize()) != 0)
		return false;

	return std::memcmp(&a.u, &b.u, sizeof(a.u)) == 0;
}


bool
format_is_compatible(const media_format& a, const media_format& b)
{
	return a == b;
}


bool
string_for_format(const media_format& f, char* buf, size_t size)
{
	if (buf == NULL || size == 0)
		return false;

	std::snprintf(buf, size, "media_format(type=%d)", (int)f.type);
	return true;
}


media_encode_info::media_encode_info()
	:	flags(0),
		used_data_size(0),
		start_time(0),
		time_to_encode(0),
		_pad{0},
		file_format_data(NULL),
		file_format_data_size(0),
		codec_data(NULL),
		codec_data_size(0)
{
}


media_decode_info::media_decode_info()
	:	time_to_decode(0),
		_pad{0},
		file_format_data(NULL),
		file_format_data_size(0),
		codec_data(NULL),
		codec_data_size(0)
{
}