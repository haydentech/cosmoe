/*This is Disview.h*/
#ifndef DIS_VIEW_H
#define DIS_VIEW_H

#ifndef _STRING_VIEW_H
#include <View.h>
#endif



class DisView : public BView
{
public:
	DisView (BRect aRect, const char *name);
	virtual ~DisView(){};
	
	virtual void Draw(BRect r);

private:
	BBitmap* fIcon;	// The icon to draw
};

#endif
