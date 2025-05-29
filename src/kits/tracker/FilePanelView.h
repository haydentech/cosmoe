/*
 * Copyright (c) 2025, Hayden Technologies, Inc.
 * Distributed under the terms of the MIT license.
 *
 * Author:
 *		Bill Hayden <hayden@haydentech.com>
 */

#ifndef FILEPANELVIEW_H
#define FILEPANELVIEW_H

#include "ColumnListView.h"

class FilePanelView : public BColumnListView
{
public:
			FilePanelView(const BRect &frame, const char *name, int32 resize, int32 flags, border_style border);
	virtual ~FilePanelView();
			
	void LoadDirectory(const char* path);

private:
			BBitmap* fDirectoryIcon;
			BBitmap* fFileIcon;
};

class FilePanelRow : public BRow
{
public:
			FilePanelRow(BBitmap* bitmap, const char *name, const size_t size, const char *date);
};
#endif
