NAME = Showcase
TYPE = APP
APP_MIME_SIG = application/x-vnd.Cosmoe-Showcase

SRCS = \
	diswindow.cpp \
	disview.cpp \
	IconTab.cpp \
	IconTabView.cpp \
	IconOutlineListView.cpp \
	disapp.cpp \
	app.cpp \
	Placeholder.cpp \
	VectorImageButton.cpp

RDEFS = Showcase.rdef
LIBS = be tracker
APP_ICON = $(firstword $(RSRCS))
MAC_BUILD_SUBDIR = cosmoe/src/apps/showcase
MAC_STAGED_DIRECTORIES = ../../../data:cosmoe/data

DEVEL_DIRECTORY := \
	$(shell findpaths -r "makefile_engine" B_FIND_PATH_DEVELOP_DIRECTORY)

include $(DEVEL_DIRECTORY)/etc/makefile-engine
