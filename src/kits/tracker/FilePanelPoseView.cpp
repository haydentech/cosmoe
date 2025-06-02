/*
 * Copyright (c) 2025, Hayden Technologies, Inc.
 * Distributed under the terms of the MIT license.
 *
 * Author:
 *		Bill Hayden <hayden@haydentech.com>
 */

#include "FilePanelPoseView.h"
#include "FilePanelFileColumn.h"
#include <ColumnTypes.h>
#include <Directory.h>
#include <Path.h>

#include <Commands.h>

#include <Bitmaps.h>
#include <stdio.h>

BFilePanelPoseView::BFilePanelPoseView(const BRect &frame, const BEntry* startDir, const char *name, int32 resize, int32 flags,
						border_style border)
	:  BColumnListView(frame,name,resize,flags,border)
{
	fDirectoryIcon = new BBitmap(BRect(0, 0, 15, 15), B_RGBA32);
	fFileIcon = new BBitmap(BRect(0, 0, 15, 15), B_RGBA32);
	GetTrackerResources()->GetIconResource(R_BeosFolderIcon, B_MINI_ICON, fDirectoryIcon);
	GetTrackerResources()->GetIconResource(R_FileIcon, B_MINI_ICON, fFileIcon);

	float width = be_plain_font->StringWidth("000.00 MB") + 32;
	AddColumn(new FilePanelFileColumn("Name", 200, 150, 400, B_TRUNCATE_END), 0);
	AddColumn(new BSizeColumn("Size", width, 60, 150, B_ALIGN_RIGHT), 1);
	AddColumn(new BStringColumn("Modified", 195, 100, 300, B_NO_TRUNCATION), 2);

	SetInvocationMessage(new BMessage(B_REFS_RECEIVED));

	const char* startingDir = getenv("HOME");
	if (startingDir == NULL || strlen(startingDir) == 0)
		startingDir = "/";

	BPath path;

	if (startDir != NULL) {
		if (startDir->GetPath(&path) == B_OK)
			startingDir = path.Path();
	}

	LoadDirectory(startingDir);
}


BFilePanelPoseView::~BFilePanelPoseView()
{
	delete fDirectoryIcon;
	delete fFileIcon;
}


void BFilePanelPoseView::LoadDirectory(const char* path)
{
	BEntry entry;

	fCurrentDirectory.SetTo(path);
	// FIXME: add this to the history stack, and delete the forward history

	Clear();

	while (fCurrentDirectory.GetNextEntry(&entry) == B_OK) {
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


void BFilePanelPoseView::GoUp()
{
	BPath parentPath(&fCurrentDirectory);
	if (parentPath.GetParent(&parentPath) == B_OK) {
		LoadDirectory(parentPath.Path());
	}
}


void BFilePanelPoseView::GoBack()
{
	// We will need to maintain a history stack to implement this.
}


void BFilePanelPoseView::GoForward()
{
	// We will need to maintain a history stack to implement this.
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


entry_ref BFilePanelPoseView::SelectedPath()
{
	entry_ref ref;

	for (int32 i = 0; i < CountRows(); i++) {
		BRow* row = RowAt(i);
		if (row->IsSelected()) {
			FilePanelFileField* field = static_cast<FilePanelFileField*>(row->GetField(0));
			BPath path(&fCurrentDirectory);
			path.Append(field->String());
			ref.set_name(path.Path());
		}
	}

	return entry_ref(ref);
}


void
BFilePanelPoseView::SetRefFilter(BRefFilter* filter)
{
	fRefFilter = filter;
	// FIXME: actually use the filter on the view
}


BRefFilter*
BFilePanelPoseView::RefFilter() const
{
	return fRefFilter;
}


// -------


FilePanelRow::FilePanelRow(BBitmap* bitmap, const char *name, const size_t size, const char *date)
	: BRow()
{
	SetField(new FilePanelFileField(bitmap, name), 0);
	SetField(new BSizeField(size), 1);
	SetField(new BStringField(date), 2);
}


