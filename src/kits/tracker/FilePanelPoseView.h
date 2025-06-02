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
#include <StringList.h>
#include "ColumnListView.h"


class BRefFilter;

namespace BPrivate {

class BFilePanelPoseView : public BColumnListView
{
public:
			BFilePanelPoseView(const BRect &frame, const BEntry* startDir, const char *name, int32 resize, int32 flags, border_style border);
	virtual ~BFilePanelPoseView();

	void		LoadDirectory(const char* path);

	void		GoUp();
	void		GoBack();
	void		GoForward();

	uint32		CountSelected();
	entry_ref	SelectedPath();

	// filtering
	void		SetRefFilter(BRefFilter*);
	BRefFilter*	RefFilter() const;

private:
	BDirectory	fCurrentDirectory;

	BBitmap*	fDirectoryIcon;
	BBitmap*	fFileIcon;

	BRefFilter*	fRefFilter;

	BStringList fPathHistory;
};

class FilePanelRow : public BRow
{
public:
			FilePanelRow(BBitmap* bitmap, const char *name, const size_t size, const char *date);
};

}

#endif
