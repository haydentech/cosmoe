#ifndef _DISAPP_H
#define _DISAPP_H

#include <Application.h>

class DisWindow;

class DisApplication : public BApplication 
{
	public :
		DisApplication();
		virtual ~DisApplication(){};
		virtual void	Pulse();

	private:
		DisWindow*		fWindow;
};

#endif
