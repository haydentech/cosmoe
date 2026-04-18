#include "IconOutlineListView.h"

#include <algorithm>

#include <ControlLook.h>
#include <View.h>


BIconStringItem::BIconStringItem(const char* text, const BBitmap* icon,
	uint32 outlineLevel, bool expanded)
	:
	BStringItem(text, outlineLevel, expanded),
	fIcon(NULL)
{
	SetIcon(icon);
}


BIconStringItem::~BIconStringItem()
{
	delete fIcon;
}


void
BIconStringItem::SetIcon(const BBitmap* icon)
{
	if (icon == NULL) {
		ClearIcon();
		return;
	}

	BBitmap* clone = _CloneBitmap(icon);
	if (clone == NULL)
		return;

	delete fIcon;
	fIcon = clone;
}


void
BIconStringItem::ClearIcon()
{
	delete fIcon;
	fIcon = NULL;
}


const BBitmap*
BIconStringItem::Icon() const
{
	return fIcon;
}


void
BIconStringItem::DrawItem(BView* owner, BRect frame, bool complete)
{
	if (owner == NULL)
		return;

	const char* text = Text();
	if (text == NULL)
		text = "";

	rgb_color lowColor = owner->LowColor();

	if (IsSelected() || complete) {
		rgb_color color;
		if (IsSelected())
			color = ui_color(B_LIST_SELECTED_BACKGROUND_COLOR);
		else
			color = owner->ViewColor();

		owner->SetLowColor(color);
		owner->FillRect(frame, B_SOLID_LOW);
	} else
		owner->SetLowColor(owner->ViewColor());

	float x = frame.left + be_control_look->DefaultLabelSpacing();
	if (fIcon != NULL) {
		BPoint iconPos(x,
			frame.top + floorf((frame.Height() - 16) / 2.0f));

		owner->PushState();
		owner->SetDrawingMode(B_OP_OVER);
		owner->DrawBitmap(fIcon, fIcon->Bounds(), BRect(iconPos, iconPos + BPoint(15, 15)), B_FILTER_BITMAP_BILINEAR);
		owner->PopState();

		x += 16.0f + be_control_look->DefaultLabelSpacing() + 1.0f;
	}

	owner->MovePenTo(x, frame.top + BaselineOffset());
	owner->DrawString(text);

	owner->SetLowColor(lowColor);
}


void
BIconStringItem::Update(BView* owner, const BFont* font)
{
	BStringItem::Update(owner, font);

	if (fIcon != NULL) {
		const float spacing = be_control_look->DefaultLabelSpacing() + 1.0f;
		SetWidth(Width() + 15.0f + spacing);
		SetHeight(std::max(Height(), 15.0f + 4.0f));
	}
}


BBitmap*
BIconStringItem::_CloneBitmap(const BBitmap* source) const
{
	if (source == NULL || source->InitCheck() != B_OK)
		return NULL;

	BBitmap* clone = new(std::nothrow) BBitmap(source->Bounds(), 0,
		source->ColorSpace());
	if (clone == NULL)
		return NULL;

	if (clone->InitCheck() != B_OK || clone->ImportBits(source) != B_OK) {
		delete clone;
		return NULL;
	}

	return clone;
}


BIconOutlineListView::BIconOutlineListView(BRect frame, const char* name,
	list_view_type type, uint32 resizingMode, uint32 flags)
	:
	BOutlineListView(frame, name, type, resizingMode, flags)
{
}


BIconOutlineListView::BIconOutlineListView(const char* name,
	list_view_type type, uint32 flags)
	:
	BOutlineListView(name, type, flags)
{
}


BIconOutlineListView::~BIconOutlineListView()
{
}
