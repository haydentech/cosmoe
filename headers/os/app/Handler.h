/*
 * Copyright 2001-2014 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Erik Jaesler, erik@cgsoftware.com
 */
#ifndef _HANDLER_H
#define _HANDLER_H

#include <cstddef>

class BHandler {
public:
							BHandler(const char* name = NULL);
	virtual					~BHandler();

			void			SetName(const char* name);
			const char*		Name() const;

private:
			void			_InitData(const char* name);

			char*			fName;
};

#endif	// _HANDLER_H
