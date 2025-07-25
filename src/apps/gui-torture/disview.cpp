
#ifndef DIS_VIEW_H	
#include "disview.h"	
#endif				

#include <Font.h>

DisView::DisView(BRect rect,
		 const char *name,
		 const char *text)
					: BStringView(rect,
							name,
							text,
							B_FOLLOW_ALL_SIDES,
							B_WILL_DRAW | B_PULSE_NEEDED)
{
	SetFont(be_bold_font);
	SetFontSize(48);
}

void DisView::Pulse()
{
	count++;
	BString str;
	str.SetToFormat("%d", count);
	SetText(str);
}

