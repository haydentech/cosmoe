
#ifndef DIS_WINDOW_H
#define DIS_WINDOW_H

#include <Window.h>
#include <MenuBar.h>
#include <StatusBar.h>
#include <FilePanel.h>

class BSplitView;
class BSlider;
class BView;

class DisWindow : public BWindow
{
public:
		DisWindow (BRect aRect);
virtual ~DisWindow() {};

virtual	bool			QuitRequested();
virtual	void			MessageReceived(BMessage* message);
virtual	void			IncrementBar();

		void			Populate();
private:
		void			SetupMenus();

		BMenuBar*		mMenuBar;
		BStatusBar*		mStatusBar;

		BBitmap*		fIcon;
		BFilePanel*		fFilePanel;
		BSplitView*		fLayoutVerticalSplit = NULL;
		BSplitView*		fLayoutHorizontalSplit = NULL;
		BSlider*		fLayoutSpacingSlider = NULL;
		BView*			fLayoutMiddlePane = NULL;
		bool			fLayoutMiddleVisible = true;
		bool			fLayoutVertical = true;

		bool			fResized = false;
};
#endif
