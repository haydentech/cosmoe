#ifndef ICON_TAB_H
#define ICON_TAB_H

#include <Bitmap.h>
#include <String.h>
#include <TabView.h>

class IconTab : public BTab {
public:
						IconTab(BView* contentsView = NULL);
	virtual				~IconTab();

	virtual	void			SetLabel(const char* label);
	virtual	void			SetView(BView* view);
						void SetIcon(const BBitmap* icon);
						void ClearIcon();
	const BBitmap*			Icon() const;

	virtual	void			DrawLabel(BView* owner, BRect frame);

private:
						BBitmap* _CloneBitmap(const BBitmap* source) const;

private:
	BBitmap*				fIcon;
	BString				fDisplayLabel;
};

#endif
