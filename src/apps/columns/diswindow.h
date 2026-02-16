
#ifndef DIS_WINDOW_H
#define DIS_WINDOW_H

#include <Window.h>
#include <FilePanel.h>

class DisWindow : public BWindow
{
public:
		DisWindow (BRect aRect);
virtual ~DisWindow() {};

virtual	bool			QuitRequested();
virtual	void			MessageReceived(BMessage* message);

		void			Populate();
private:
		BBitmap*		fIcon;
		BFilePanel*		fFilePanel;
};
#endif
