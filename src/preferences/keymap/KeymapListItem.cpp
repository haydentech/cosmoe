/*
 * Copyright 2004-2006 Haiku Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Sandor Vroemisse
 *		Jérôme Duval
 */


#include "KeymapListItem.h"


KeymapListItem::KeymapListItem(entry_ref& keymap, const char* name,
	const char* token)
	:
	BStringItem(name != NULL ? name : keymap.name),
	fKeymap(keymap),
	fToken(token != NULL ? token : keymap.name)
{
}
