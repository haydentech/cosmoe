/*
 * Copyright (c) 2025, Hayden Technologies, Inc.
 * Distributed under the terms of the MIT license.
 *
 * Author:
 *		Bill Hayden <hayden@haydentech.com>
 */

#include "FilePanelPriv.h"
#include "FilePanelFileColumn.h"
#include <ColumnTypes.h>
#include <Directory.h>
#include <Path.h>

#include "Model.h"

#include <Commands.h>

#include <Bitmaps.h>
#include <stdio.h>

using namespace BPrivate;


class FilePanelRow : public BRow
{
public:
			FilePanelRow(BBitmap* bitmap, const char *name, const size_t size, const char *date);
};


BFilePanelPoseView::BFilePanelPoseView(Model* model)
	:  BPoseView(model, kListMode)
{
	Setup(model);
}

BFilePanelPoseView::~BFilePanelPoseView()
{
	delete fDirectoryIcon;
	delete fFileIcon;
}

#include <execinfo.h>
void
BFilePanelPoseView::Setup(Model* model)
{
	fDirectoryIcon = new BBitmap(BRect(0, 0, 15, 15), B_RGBA32);
	fFileIcon = new BBitmap(BRect(0, 0, 15, 15), B_RGBA32);
	GetTrackerResources()->GetIconResource(R_FolderIcon, B_MINI_ICON, fDirectoryIcon);
	GetTrackerResources()->GetIconResource(R_FileIcon, B_MINI_ICON, fFileIcon);

	float width = be_plain_font->StringWidth("000.00 MB") + 32;
	AddColumn(new FilePanelFileColumn("Name", 200, 150, 400, B_TRUNCATE_END), 0);
	AddColumn(new BSizeColumn("Size", width, 60, 150, B_ALIGN_RIGHT), 1);
	AddColumn(new BStringColumn("Modified", 195, 100, 300, B_NO_TRUNCATION), 2);

	SetInvocationMessage(new BMessage(B_REFS_RECEIVED));

	Refresh();
}


uint32 BFilePanelPoseView::CountSelected()
{
	uint32 count = 0;
	for (int32 i = 0; i < CountRows(); i++) {
		BRow* row = RowAt(i);
		if (row->IsSelected())
			count++;
	}
	return count;
}


void
BFilePanelPoseView::Refresh()
{
	_inherited::Refresh();

	Clear();

	BEntry entry;
	BPath dirpath;
	TargetModel()->GetPath(&dirpath);
	BDirectory dir(dirpath.Path());
	while (dir.GetNextEntry(&entry) == B_OK) {
		BPath entryPath;
		if (entry.GetPath(&entryPath) == B_OK) {
			struct stat st;
			if (stat(entryPath.Path(), &st) == 0) {
				BBitmap* icon = S_ISDIR(st.st_mode) ? fDirectoryIcon : fFileIcon;
				
				AddRow(new FilePanelRow(icon, entryPath.Leaf(), st.st_size, ctime(&st.st_mtime)));
			}
		}
	}
}


// -------


FilePanelRow::FilePanelRow(BBitmap* bitmap, const char *name, const size_t size, const char *date)
	: BRow()
{
	SetField(new FilePanelFileField(bitmap, name), 0);
	SetField(new BSizeField(size), 1);
	SetField(new BStringField(date), 2);
}


