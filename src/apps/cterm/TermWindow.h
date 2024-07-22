#ifndef TERM_WINDOW_H
#define TERM_WINDOW_H


#include <Rect.h>
#include <Message.h>
#include <Window.h>
#include <MenuItem.h>

#include "TermView.h"

enum
{
    ID_SCROLL,
    ID_REFRESH
};

class TermView;


class TermWindow : public BWindow {
public:
					TermWindow(BRect cFrame, const char* title, window_type type, uint32 flags);
	virtual						~TermWindow();

	int         Write(const char* pBuffer, int nSize);
	void        RefreshDisplay(bool bAll);

	BScrollBar* scrollBar;

protected:
	virtual bool				QuitRequested();
	virtual void				MessageReceived(BMessage* message);

	TermView*  termView;

private:
				void				_InitWindow();
};


#endif // TERM_WINDOW_H
