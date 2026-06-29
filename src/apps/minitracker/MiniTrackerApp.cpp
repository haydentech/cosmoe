#include "MiniTrackerApp.h"

#include <FilePanel.h>


namespace {

class MiniTrackerPanel : public BFilePanel {
public:
	MiniTrackerPanel(const char* startupPath)
		:	BFilePanel(B_TRACKER_PANEL)
	{
		if (startupPath != NULL && startupPath[0] != '\0')
			SetPanelDirectory(startupPath);
	}

	void WasHidden() override
	{
		be_app->PostMessage(B_QUIT_REQUESTED);
	}
};

}

MiniTrackerApp::MiniTrackerApp(const char* startupPath)
	:	BApplication("application/x-vnd.Cosmoe-MiniTracker")
{
	// Doesn't get much more mini than this!
	BFilePanel* trackerPanel = new MiniTrackerPanel(startupPath);
	trackerPanel->Show();
}