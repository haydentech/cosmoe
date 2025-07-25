
#ifndef DIS_WINDOW_H
#define DIS_WINDOW_H

#include <Window.h>



class DisWindow : public BWindow
{
	public : 
				DisWindow (BRect aRect, const char *name);
		virtual ~DisWindow (){};
		virtual bool QuitRequested();

		static int fCount;
};
#endif
