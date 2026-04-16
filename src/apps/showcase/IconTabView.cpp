#include "IconTabView.h"

#include <algorithm>

#include <ControlLook.h>

#include "IconTab.h"


BIconTabView::BIconTabView(const char* name, button_width width, uint32 flags)
	:
	BTabView(name, width, flags)
{
}


BIconTabView::BIconTabView(BRect frame, const char* name, button_width width,
	uint32 resizeMask, uint32 flags)
	:
	BTabView(frame, name, width, resizeMask, flags)
{
}


BRect
BIconTabView::TabFrame(int32 index) const
{
	if (index >= CountTabs() || index < 0)
		return BRect();

	const float padding = ceilf(be_control_look->DefaultLabelSpacing() * 3.3f);
	const float height = TabHeight();
	const float offset = BControlLook::ComposeSpacing(B_USE_WINDOW_SPACING);
	const BRect bounds(Bounds());

	float width = padding * 5.0f;
	switch (TabWidth()) {
		case B_WIDTH_FROM_LABEL:
		{
			float x = 0.0f;
			for (int32 i = 0; i < index; i++)
				x += _TabContentWidth(i, padding);

			const float tabWidth = _TabContentWidth(index, padding);
			switch (TabSide()) {
				case kTopSide:
					return BRect(offset + x, 0.0f,
						offset + x + tabWidth, height);
				case kBottomSide:
					return BRect(offset + x, bounds.bottom - height,
						offset + x + tabWidth, bounds.bottom);
				case kLeftSide:
					return BRect(0.0f, offset + x, height,
						offset + x + tabWidth);
				case kRightSide:
					return BRect(bounds.right - height, offset + x,
						bounds.right, offset + x + tabWidth);
				default:
					return BRect();
			}
		}

		case B_WIDTH_FROM_WIDEST:
		{
			width = 0.0f;
			for (int32 i = 0; i < CountTabs(); i++)
				width = std::max(width, _TabContentWidth(i, padding));
			break;
		}

		case B_WIDTH_AS_USUAL:
		default:
		{
			// Match stock BTabView behavior: fixed-width tabs in AS_USUAL mode.
			// Ensure width can at least fit the icon portion if present.
			for (int32 i = 0; i < CountTabs(); i++) {
				IconTab* iconTab = dynamic_cast<IconTab*>(TabAt(i));
				if (iconTab == NULL || iconTab->Icon() == NULL)
					continue;

				const float iconOnlyWidth = iconTab->Icon()->Bounds().Width()
					+ be_control_look->DefaultLabelSpacing() + 1.0f + padding;
				width = std::max(width, iconOnlyWidth);
			}
			break;
		}
	}

	switch (TabSide()) {
		case kTopSide:
			return BRect(offset + index * width, 0.0f,
				offset + index * width + width, height);
		case kBottomSide:
			return BRect(offset + index * width, bounds.bottom - height,
				offset + index * width + width, bounds.bottom);
		case kLeftSide:
			return BRect(0.0f, offset + index * width, height,
				offset + index * width + width);
		case kRightSide:
			return BRect(bounds.right - height, offset + index * width,
				bounds.right, offset + index * width + width);
		default:
			return BRect();
	}
}


float
BIconTabView::_TabContentWidth(int32 index, float padding) const
{
	BTab* tab = TabAt(index);
	if (tab == NULL)
		return padding;

	const char* label = tab->Label();
	float width = StringWidth(label != NULL ? label : "") + padding;

	IconTab* iconTab = dynamic_cast<IconTab*>(tab);
	if (iconTab != NULL && iconTab->Icon() != NULL) {
		// Match BeControlLook::DrawLabel(icon) horizontal contribution.
		width += iconTab->Icon()->Bounds().Width()
			+ be_control_look->DefaultLabelSpacing() + 1.0f;
	}

	return width;
}
