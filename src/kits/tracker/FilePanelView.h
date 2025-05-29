/*
 * Copyright (c) 2025, Hayden Technologies, Inc.
 * Distributed under the terms of the MIT license.
 *
 * Author:
 *		Bill Hayden <hayden@haydentech.com>
 */

#ifndef FILEPANELVIEW_H
#define FILEPANELVIEW_H

#include <Directory.h>
#include "ColumnListView.h"

#define M_FILE_PANEL_SELECTION 'FPsl'
#define M_FILE_PANEL_SET_DIRECTORY 'FPsd'
#define M_FILE_PANEL_DIRECTORY_BACK 'FPba'
#define M_FILE_PANEL_DIRECTORY_UP 'FPdu'
#define M_FILE_PANEL_DIRECTORY_FWD 'FPfw'

class FilePanelView : public BColumnListView
{
public:
			FilePanelView(const BRect &frame, const char *name, int32 resize, int32 flags, border_style border);
	virtual ~FilePanelView();
			
	void LoadDirectory(const char* path);

	void GoUp();
	void GoBack();
	void GoForward();

private:
			BDirectory fCurrentDirectory;

			BBitmap* fDirectoryIcon;
			BBitmap* fFileIcon;
};

class FilePanelRow : public BRow
{
public:
			FilePanelRow(BBitmap* bitmap, const char *name, const size_t size, const char *date);
};
#endif
