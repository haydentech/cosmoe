#include "MiniTrackerApp.h"

#include <FilePanel.h>


namespace {

class MiniTrackerPanel : public BFilePanel {
public:
	MiniTrackerPanel()
		:	BFilePanel(B_TRACKER_PANEL)
	{
	}

	void WasHidden() override
	{
		be_app->PostMessage(B_QUIT_REQUESTED);
	}
};

}

MiniTrackerApp::MiniTrackerApp()
	:	BApplication("application/x-vnd.Cosmoe-MiniTracker")
{
	// Doesn't get much more mini than this!
	BFilePanel* trackerPanel = new MiniTrackerPanel();
	trackerPanel->Show();
}