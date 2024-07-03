/*
 * Copyright 2005, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef	_FILE_PANEL_H
#define _FILE_PANEL_H

#include <sys/stat.h>
#include <Directory.h>
#include <Entry.h>
#include <Node.h>

#include <Window.h>
#include <string>



class DirectoryView;
class BTextView;
class BButton;


class BRefFilter {
	public:
virtual	bool	Filter(const char* pzPath, const struct stat * psStat ) = 0;
};


enum file_panel_mode {
	B_OPEN_PANEL,
	B_SAVE_PANEL
};

enum file_panel_button {
	B_CANCEL_BUTTON,
	B_DEFAULT_BUTTON
};

class BWindow;
class BMessenger;
class BMessage;


class BFilePanel : public BWindow
{
public:
    enum { NODE_FILE = 0x01, NODE_DIR = 0x02 };
  
					BFilePanel( file_panel_mode mode = B_OPEN_PANEL,
							BMessenger *target = NULL,
							const char* pzPath = NULL,
							uint32 node_flavors = NODE_FILE,
							bool allow_multiple_selection = true,
							BMessage *message = NULL,
							BRefFilter* pcFilter = NULL,
							bool  modal = false,
							bool hide_when_done = true,
							const char* pzOkLabel = NULL,
							const char* pzCancelLabel = NULL );
    virtual void	MessageReceived( BMessage* pcMessage );
    virtual void	FrameResized( float inWidth, float inHeight );

    void			SetPath( const std::string& cPath );
    std::string		GetPath() const;
	
private:
	void Layout();
	
	enum { ID_PATH_CHANGED = 1,
		   ID_SEL_CHANGED,
		   ID_INVOKED,
		   ID_CANCEL,
		   ID_OK,
		   ID_ALERT };

	BMessage*	    m_pcMessage;
	BMessenger*	    m_pcTarget;

	file_panel_mode m_nMode;
	uint32	        m_nNodeType;
	bool	        m_bHideWhenDone;
	DirectoryView*  m_pcDirView;
	BTextView*	    m_pcPathView;
	BButton*	    m_pcOkButton;
	BButton*	    m_pcCancelButton;
};

#endif	/* _FILE_PANEL_H */
