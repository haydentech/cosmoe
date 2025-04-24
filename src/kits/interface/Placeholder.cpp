#include <Placeholder.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



BPlaceholder::BPlaceholder(BRect frame, const char* name, uint32 resizingMode, uint32 flags)
	:
	BView(frame, name, resizingMode, flags | B_WILL_DRAW | B_FRAME_EVENTS)
{
}



BPlaceholder::~BPlaceholder()
{
}


void
BPlaceholder::Draw(BRect updateRect)
{
	BRect rect(Bounds());

	rgb_color shadow = tint_color(ViewColor(), B_DARKEN_2_TINT);

	DrawString("Placeholder", BPoint(rect.left + 5, rect.top + 15));
	StrokeRect(rect);
	StrokeLine(BPoint(rect.left, rect.top),
				BPoint(rect.right, rect.bottom));
	StrokeLine(BPoint(rect.right, rect.top),
				BPoint(rect.left, rect.bottom));
}





