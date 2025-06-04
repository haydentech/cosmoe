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
#ifndef _CONTAINER_WINDOW_H
#define _CONTAINER_WINDOW_H


#include <GroupView.h>
#include <MimeType.h>
#include <StringList.h>
#include <Window.h>

#include "LockingList.h"
#include "NavMenu.h"
#include "TaskLoop.h"


class BGridView;
class BGroupLayout;
class BGroupView;
class BPopUpMenu;
class BMenuBar;

namespace BPrivate {

class BNavigator;
class BPoseView;
class DraggableContainerIcon;
class ModelMenuItem;
class AttributeStreamNode;
class BackgroundImage;
class Model;
class ModelNodeLazyOpener;
class BorderedView;
class SelectionWindow;
class TShortcuts;
class TemplatesMenu;


#define kDefaultFolderTemplate "DefaultFolderTemplate"




enum {
	// flags that describe opening of the window
	kRestoreWorkspace	= 0x1,
	kIsHidden			= 0x2,
		// set when opening a window during initial Tracker start
	kRestoreDecor		= 0x4
};



class BContainerWindow : public BWindow {
public:
	BContainerWindow(LockingList<BWindow>* windowList, uint32 openFlags,
		window_look look = B_DOCUMENT_WINDOW_LOOK,
		window_feel feel = B_NORMAL_WINDOW_FEEL,
		uint32 windowFlags = B_WILL_ACCEPT_FIRST_CLICK | B_NO_WORKSPACE_ACTIVATION,
		uint32 workspace = B_CURRENT_WORKSPACE, bool useLayout = true);

	virtual ~BContainerWindow();

	virtual void Init(const BMessage* message = NULL);
	virtual void InitLayout();

	static BRect InitialWindowRect(window_feel);

	virtual void Quit();
	virtual bool QuitRequested();

	virtual void CreatePoseView(Model*);

	virtual bool IsShowing(const entry_ref*) const;

	void ResizeToFit();

	Model* TargetModel() const;
	BPoseView* PoseView() const;
	BNavigator* Navigator() const;

	virtual void SelectionChanged();

	virtual void MessageReceived(BMessage*);

	virtual void SwitchDirectory(const entry_ref* ref);
	virtual void OpenParent();
	void UpdateTitle();

protected:
	enum MenuContext {
		kFileMenuContext,
		kWindowMenuContext,
		kPosePopUpContext,
		kWindowPopUpContext
	};

protected:
	virtual BPoseView* NewPoseView(Model*, uint32);
		// instantiate a different flavor of BPoseView for different
		// ContainerWindows
protected:
	LockingList<BWindow>* fWindowList;
	uint32 fOpenFlags;
	bool fUsesLayout;

	BGroupLayout* fRootLayout;
	BGridView* fPoseContainer;
	BorderedView* fBorderedView;

	BNavigator* fNavigator;
	BPoseView* fPoseView;

	bool fStateNeedsSaving;

private:
	static BRect sNewWindRect;

	typedef BWindow _inherited;
};


class BorderedView : public BGroupView {
public:
	BorderedView();

	void PoseViewFocused(bool);
	virtual void Pulse();

	void EnableBorderHighlight(bool);

protected:
	virtual void WindowActivated(bool);

private:
	bool fEnableBorderHighlight;

	typedef BGroupView _inherited;
};


// inlines ---------

inline BNavigator*
BContainerWindow::Navigator() const
{
	return fNavigator;
}


inline BPoseView*
BContainerWindow::PoseView() const
{
	return fPoseView;
}



} // namespace BPrivate

using namespace BPrivate;


#endif	// _CONTAINER_WINDOW_H
