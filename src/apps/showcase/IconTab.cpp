#include "IconTab.h"

#include <cmath>

#include <ControlLook.h>


IconTab::IconTab(BView* contentsView)
	:
	BTab(contentsView),
	fIcon(NULL),
	fDisplayLabel("")
{
}


IconTab::~IconTab()
{
	delete fIcon;
}


void
IconTab::SetLabel(const char* label)
{
	fDisplayLabel = label != NULL ? label : "";
	BTab::SetLabel(fDisplayLabel.String());
}


void
IconTab::SetView(BView* view)
{
	BTab::SetView(view);
	BTab::SetLabel(fDisplayLabel.String());
}


void
IconTab::SetIcon(const BBitmap* icon)
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
	BTab::SetLabel(fDisplayLabel.String());
}


void
IconTab::ClearIcon()
{
	delete fIcon;
	fIcon = NULL;
	BTab::SetLabel(fDisplayLabel.String());
}


const BBitmap*
IconTab::Icon() const
{
	return fIcon;
}


void
IconTab::DrawLabel(BView* owner, BRect frame)
{
	if (owner == NULL) {
		BTab::DrawLabel(owner, frame);
		return;
	}

	BTabView* tabView = dynamic_cast<BTabView*>(owner);
	if (tabView == NULL) {
		BTab::DrawLabel(owner, frame);
		return;
	}

	float rotation = 0.0f;
	BPoint center(frame.left + frame.Width() / 2.0f,
		frame.top + frame.Height() / 2.0f);

	switch (tabView->TabSide()) {
		case BTabView::kTopSide:
		case BTabView::kBottomSide:
			rotation = 0.0f;
			break;
		case BTabView::kLeftSide:
			rotation = 270.0f;
			break;
		case BTabView::kRightSide:
			rotation = 90.0f;
			break;
	}

	if (rotation != 0.0f) {
		BRect originalFrame(frame);
		frame.top = center.y - originalFrame.Width() / 2.0f;
		frame.bottom = center.y + originalFrame.Width() / 2.0f;
		frame.left = center.x - originalFrame.Height() / 2.0f;
		frame.right = center.x + originalFrame.Height() / 2.0f;
	}

	const char* label = fDisplayLabel.IsEmpty() ? NULL : fDisplayLabel.String();
	if (label == NULL && fIcon == NULL) {
		BTab::DrawLabel(owner, frame);
		return;
	}

	owner->PushState();

	BAffineTransform transform;
	transform.RotateBy(center, rotation * M_PI / 180.0f);
	owner->SetTransform(transform);

	rgb_color highColor = ui_color(B_PANEL_TEXT_COLOR);
	BRect iconFrame(frame);
	iconFrame.right = iconFrame.left + 15.0f;
	iconFrame.bottom = iconFrame.top + 15.0f;

	if (16 < frame.Height())
		iconFrame.OffsetBy(10, ceilf((frame.Height() - 15.0f) / 2));

	drawing_mode oldMode = owner->DrawingMode();
	owner->SetDrawingMode(B_OP_OVER);
	owner->DrawBitmap(fIcon, fIcon->Bounds(), iconFrame);
	owner->SetDrawingMode(oldMode);

	frame.left += 20.0f;
	be_control_look->DrawLabel(owner, label, NULL, frame, frame,
		ui_color(B_PANEL_BACKGROUND_COLOR),
		IsEnabled() ? 0 : BControlLook::B_DISABLED,
		BAlignment(B_ALIGN_HORIZONTAL_CENTER, B_ALIGN_VERTICAL_CENTER),
		&highColor);

	owner->PopState();
}


BBitmap*
IconTab::_CloneBitmap(const BBitmap* source) const
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
