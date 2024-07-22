#include <Application.h>

#include "TermWindow.h"

class TermApp : public BApplication
{
public:
	TermApp();
	virtual ~TermApp();

	virtual bool        QuitRequested();
	bool OpenWindow();

	bool IsWindowOpen() { return termWindow != NULL; }

	int32 ReadPTY(void*);
	
	TermWindow*	termWindow;
};