/*
Open Tracker License

Terms and Conditions

Copyright (c) 1991-2000, Be Incorporated. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice applies to all licensees
and shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF TITLE, MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
BE INCORPORATED BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF, OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of Be Incorporated shall not be
used in advertising or otherwise to promote the sale, use or other dealings in
this Software without prior written authorization from Be Incorporated.

Tracker(TM), Be(R), BeOS(R), and BeIA(TM) are trademarks or registered trademarks
of Be Incorporated in the United States and other countries. Other brand product
names are registered trademarks or trademarks of their respective holders.
All rights reserved.
*/
#ifndef _POSE_VIEW_H
#define _POSE_VIEW_H


// BPoseView is a container for poses, handling all of the interaction, drawing,
// etc. The three different view modes are handled here.
//
// this is by far the fattest Tracker class and over time will undergo a lot of
// trimming


#include "ContainerWindow.h"
#include "Model.h"

#include <ColumnListView.h>


class BRefFilter;
class BList;

namespace BPrivate {

class BContainerWindow;


const uint32 kMiniIconMode = 'Tmic';
const uint32 kIconMode = 'Ticn';
const uint32 kListMode = 'Tlst';

const uint32 kCheckTypeahead = 'Tcty';

const uint32 kMsgMouseDragged = 'Mdrg';
const uint32 kMsgMouseLongDown = 'Mold';


class BPoseView : public BColumnListView {
public:
	BPoseView(Model*, uint32 viewMode);
	virtual ~BPoseView();

	// setup, teardown
	virtual void Init(const BMessage&);

	// base class of these are not virtual but ours are
	virtual void AdoptSystemColors();
	virtual bool HasSystemColors() const;

	virtual bool Represents(const entry_ref*) const;

	BContainerWindow* ContainerWindow() const;
	Model* TargetModel() const;

	virtual bool IsFilePanel() const;

	// re-use the pose view for a new directory
	virtual void SwitchDir(const entry_ref*);

	// in the rare cases where a pose view needs to be explicitly
	// refreshed (for instance in a query window with a dynamic
	// date query), this is used
	virtual void Refresh();

	// callbacks
	virtual void AttachedToWindow();

	// misc. mode setters
	void SetMultipleSelection(bool);

	// filtering
	void SetRefFilter(BRefFilter*);
	BRefFilter* RefFilter() const;

private:
	Model* fModel;

	BRefFilter* fRefFilter;
	bigtime_t fLastKeyTime;
};


// inlines follow


inline BContainerWindow*
BPoseView::ContainerWindow() const
{
	return dynamic_cast<BContainerWindow*>(Window());
}


inline Model*
BPoseView::TargetModel() const
{
	return fModel;
}

inline bool
BPoseView::IsFilePanel() const
{
	return false;
}

inline void
BPoseView::SetMultipleSelection(bool state)
{
	SetSelectionMode(state ? B_MULTIPLE_SELECTION_LIST : B_SINGLE_SELECTION_LIST);
}


inline void
BPoseView::SetRefFilter(BRefFilter* filter)
{
	fRefFilter = filter;
}


inline BRefFilter*
BPoseView::RefFilter() const
{
	return fRefFilter;
}


} // namespace BPrivate

using namespace BPrivate;


#endif	// _POSE_VIEW_H
