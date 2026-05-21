/*
 * Copyright 2026, Bill Hayden
 * Distributed under the terms of the MIT License.
 */

#include <MediaNode.h>

#include <cstring>

// media_node

const media_node media_node::null = media_node();


media_node::media_node()
	:
	node(-1),
	port(-1),
	kind(0),
	_reserved_{0, 0, 0}
{
}


media_node::~media_node()
{
}

// media_input

media_input::media_input()
	:	node(),
		source(),
		destination(),
		format(),
		name{0},
		_reserved_media_input_{0, 0, 0, 0}
{
}


media_input::~media_input()
{
}

// media_output

media_output::media_output()
	:	node(),
		source(),
		destination(),
		format(),
		name{0},
		_reserved_media_output_{0, 0, 0, 0}
{
}


media_output::~media_output()
{
}

// live_node_info

live_node_info::live_node_info()
	:
	node(),
	hint_point(),
	name{0},
	reserved{0}
{
}


live_node_info::~live_node_info()
{
}


bool
operator==(const media_node& a, const media_node& b)
{
	return a.node == b.node && a.port == b.port && a.kind == b.kind;
}


bool
operator!=(const media_node& a, const media_node& b)
{
	return !(a == b);
}


bool
operator<(const media_node& a, const media_node& b)
{
	if (a.node != b.node)
		return a.node < b.node;
	if (a.port != b.port)
		return a.port < b.port;
	return a.kind < b.kind;
}
