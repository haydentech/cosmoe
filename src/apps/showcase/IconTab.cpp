#include "IconTab.h"

#include <algorithm>
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
	if (fIcon == NULL) {
		be_control_look->DrawLabel(owner, label, NULL, frame, frame,
			ui_color(B_PANEL_BACKGROUND_COLOR),
			IsEnabled() ? 0 : BControlLook::B_DISABLED,
			BAlignment(B_ALIGN_HORIZONTAL_CENTER, B_ALIGN_VERTICAL_CENTER),
			&highColor);
		owner->PopState();
		return;
	}

	const float iconSize = IconTab::kDrawnIconBoundsWidth;
	const float iconSpacing = be_control_look->DefaultLabelSpacing();
	float contentWidth = iconSize;
	float contentHeight = iconSize;
	float textWidth = 0.0f;

	if (label != NULL) {
		textWidth = ceilf(owner->StringWidth(label));
		contentWidth += iconSpacing + textWidth;
		font_height fontHeight;
		owner->GetFontHeight(&fontHeight);
		const float textHeight = ceilf(fontHeight.ascent)
			+ ceilf(fontHeight.descent);
		contentHeight = std::max(contentHeight, textHeight);
	}

	BRect alignedFrame(frame);
	alignedFrame.left = frame.left + floorf((frame.Width() + 1.0f - contentWidth)
		/ 2.0f);
	alignedFrame.right = alignedFrame.left + contentWidth - 1.0f;
	alignedFrame.top = frame.top + floorf((frame.Height() + 1.0f - contentHeight)
		/ 2.0f);
	alignedFrame.bottom = alignedFrame.top + contentHeight - 1.0f;

	BRect iconFrame(alignedFrame.left, alignedFrame.top,
		alignedFrame.left + iconSize - 1.0f,
		alignedFrame.top + iconSize - 1.0f);
	if (iconSize < contentHeight) {
		iconFrame.OffsetBy(0.0f,
			ceilf((contentHeight - iconSize) / 2.0f));
	}

	drawing_mode oldMode = owner->DrawingMode();
	owner->SetDrawingMode(B_OP_OVER);
	owner->DrawBitmap(fIcon, fIcon->Bounds(), iconFrame, B_FILTER_BITMAP_BILINEAR);
	owner->SetDrawingMode(oldMode);

	if (label != NULL) {
		BRect textFrame(frame);
		textFrame.left = iconFrame.right + iconSpacing - 6.0f;
		be_control_look->DrawLabel(owner, label, NULL, textFrame, frame,
			ui_color(B_PANEL_BACKGROUND_COLOR),
			IsEnabled() ? 0 : BControlLook::B_DISABLED,
			BAlignment(B_ALIGN_HORIZONTAL_CENTER, B_ALIGN_VERTICAL_CENTER),
			&highColor);
	}

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
