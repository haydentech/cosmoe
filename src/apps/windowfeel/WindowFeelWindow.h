#ifndef WINDOW_FEEL_WINDOW_H
#define WINDOW_FEEL_WINDOW_H

#include <Window.h>


class BMenu;
class BStringView;


class WindowFeelWindow : public BWindow {
public:
						WindowFeelWindow();
						WindowFeelWindow(window_look look, window_feel feel);

	virtual	bool			QuitRequested();
	virtual	void			MessageReceived(BMessage* message);

private:
			void			_AddLookItems(BMenu* menu);
			void			_AddFeelItems(BMenu* menu);
			void			_ApplyLook(window_look look);
			void			_ApplyFeel(window_feel feel);
			void			_UpdateStatus();

			BStringView*		fStatusView;
};


#endif