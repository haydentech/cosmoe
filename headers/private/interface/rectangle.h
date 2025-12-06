/*
 * Copyright 2025, Cosmoe Project
 * Distributed under the terms of the MIT License.
 *
 * Common rectangle structure used across all backends
 */

#ifndef _RECTANGLE_H
#define _RECTANGLE_H

#include <stdint.h>

struct rectangle {
	int32_t x;
	int32_t y;
	int32_t width;
	int32_t height;
};

#endif /* _RECTANGLE_H */
