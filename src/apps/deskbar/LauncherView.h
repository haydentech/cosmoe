#ifndef LAUNCHER_VIEW_H
#define LAUNCHER_VIEW_H


#include <Entry.h>
#include <Handler.h>
#include <BitmapButton.h>
#include <ObjectList.h>
#include <String.h>
#include <View.h>


class BBitmap;
class TBarView;


class TLauncherView : public BView {
public:
						TLauncherView(TBarView* barView);
	virtual				~TLauncherView();

	virtual	void		MessageReceived(BMessage* message);
	virtual	void		AttachedToWindow();

			void		Refresh();
			float		PreferredWidth() const;
			bool		HasLaunchers() const;

private:
	struct LauncherItem {
						LauncherItem(const entry_ref& ref, const BString& label);
						~LauncherItem();

		entry_ref		fRef;
		BString			fLabel;
		BBitmapButton*	fButton;
	};

			status_t	_GetShortcutsDirectory(BDirectory& directory) const;
			status_t	_AddLauncher(const BEntry& entry);
			BBitmap*	_FetchIcon(const entry_ref& ref) const;
			BString		_FetchLabel(const entry_ref& ref) const;
			BBitmapButton* _CreateButton(const LauncherItem& item,
							int32 index) const;
			BBitmapButton* _CreateAddButton() const;
			status_t	_OpenDeskbarDirectoryInTracker();
			void		_ClearLaunchers();
			float		_ButtonWidth() const;
			float		_AddButtonWidth() const;

			TBarView*				fBarView;
			BObjectList<LauncherItem, true>	fLaunchers;
			BBitmapButton*			fAddShortcutButton;
};


inline bool
TLauncherView::HasLaunchers() const
{
	return !fLaunchers.IsEmpty();
}


#endif	// LAUNCHER_VIEW_H