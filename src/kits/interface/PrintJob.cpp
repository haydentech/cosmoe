/*
 * Copyright 2001-2009, Haiku.
 * Copyright 2026, Bill Hayden.
 * Distributed under the terms of the MIT license.
 *
 * Authors:
 *		I.R. Adema
 *		Stefano Ceccherini (burton666@libero.it)
 *		Michael Pfeiffer
 *		julun <host.haiku@gmx.de>
 *		Bill Hayden <haydne@haydentech.com>
 */


#include <PrintJob.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <Alert.h>
#include <Application.h>
#include <Bitmap.h>
#include <Button.h>
#include <Debug.h>
#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <FindDirectory.h>
#include <GridLayout.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <OS.h>
#include <Path.h>
#include <Picture.h>
#include <PopUpMenu.h>
#include <RadioButton.h>
#include <Region.h>
#include <String.h>
#include <StringView.h>
#include <SystemCatalog.h>
#include <TextControl.h>
#include <View.h>
#include <Window.h>

#include <pr_server.h>
#include <ViewPrivate.h>

#include <cairo.h>
#include <cairo-pdf.h>

#include <algorithm>
#include <cmath>
#include <new>
#include <vector>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "PrintJob"

#undef B_TRANSLATE
#define B_TRANSLATE(str) \
	gSystemCatalog.GetString(B_TRANSLATE_MARK(str), "PrintJob")


using BPrivate::gSystemCatalog;


namespace {

const float kDefaultMargin = 36.0f;
const int32 kDefaultResolution = 300;

const char* kDefaultPrinterName = "Print to File (PDF)";
const char* kOutputPathField = "output_path";
const char* kSettingsFileName = "PrintSettings";


BRect
DefaultPaperRect()
{
	// A4 in PostScript points (1/72 inch).
	return BRect(0, 0, 595, 842);
}


BRect
DefaultPrintableRect()
{
	BRect paper = DefaultPaperRect();
	return BRect(paper.left + kDefaultMargin, paper.top + kDefaultMargin,
		paper.right - kDefaultMargin, paper.bottom - kDefaultMargin);
}


BMessage*
MakeDefaultSettings()
{
	BMessage* settings = new(std::nothrow) BMessage;
	if (settings == NULL)
		return NULL;

	BRect paper = DefaultPaperRect();
	BRect printable = DefaultPrintableRect();

	settings->AddString(PSRV_FIELD_CURRENT_PRINTER, kDefaultPrinterName);
	// Haiku's IsSettingsMessageValid() looks for this literal field.
	settings->AddString("printer_name", kDefaultPrinterName);
	settings->AddRect(PSRV_FIELD_PAPER_RECT, paper);
	settings->AddRect(PSRV_FIELD_PRINTABLE_RECT, printable);
	settings->AddInt64(PSRV_FIELD_XRES, kDefaultResolution);
	settings->AddInt64(PSRV_FIELD_YRES, kDefaultResolution);
	settings->AddInt32(PSRV_FIELD_ORIENTATION, 0);
	settings->AddInt32(PSRV_FIELD_COPIES, 1);
	settings->AddInt32(PSRV_FIELD_FIRST_PAGE, 0);
	settings->AddInt32(PSRV_FIELD_LAST_PAGE, INT32_MAX);
	settings->AddString(kOutputPathField, "");

	return settings;
}


status_t
SettingsPath(BPath* path, bool createDirectory)
{
	status_t status = find_directory(B_USER_PRINTERS_DIRECTORY, path);
	if (status != B_OK)
		return status;

	if (createDirectory) {
		status = create_directory(path->Path(), 0755);
		if (status != B_OK && status != B_FILE_EXISTS
			&& status != B_NAME_IN_USE)
			return status;
	}

	path->Append(kSettingsFileName);
	return path->InitCheck();
}


BMessage*
LoadSettings()
{
	BPath path;
	if (SettingsPath(&path, false) != B_OK)
		return NULL;

	BFile file(path.Path(), B_READ_ONLY);
	if (file.InitCheck() != B_OK)
		return NULL;

	BMessage* settings = new(std::nothrow) BMessage;
	if (settings == NULL)
		return NULL;

	if (settings->Unflatten(&file) != B_OK) {
		delete settings;
		return NULL;
	}

	return settings;
}


void
SaveSettings(const BMessage* settings)
{
	if (settings == NULL)
		return;

	BPath path;
	if (SettingsPath(&path, true) != B_OK)
		return;

	BFile file(path.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (file.InitCheck() != B_OK)
		return;

	settings->Flatten(&file);
}


void
ShowAlert(const char* title, const char* message)
{
	if (be_app == NULL) {
		fprintf(stderr, "%s: %s\n", title, message);
		return;
	}

	BAlert* alert = new BAlert(title, message, B_TRANSLATE("OK"));
	alert->SetFlags(alert->Flags() | B_CLOSE_ON_ESCAPE);
	// Show asynchronously. Go() blocks the caller's window thread, which can
	// deadlock when printing is invoked from a window's message loop.
	alert->Go(NULL);
}


void
ShowError(const char* message)
{
	ShowAlert(B_TRANSLATE("Error"), message);
}


struct PaperSize {
	const char*	name;
	float		width;
	float		height;
};


const PaperSize kPaperSizes[] = {
	{ "A4", 595.0f, 842.0f },
	{ "US Letter", 612.0f, 792.0f },
	{ "US Legal", 612.0f, 1008.0f }
};

const int32 kPaperSizeCount = sizeof(kPaperSizes) / sizeof(kPaperSizes[0]);


struct PrintDialogResult {
	sem_id		done;
	status_t	status;
	BMessage	settings;

	PrintDialogResult()
		:
		done(create_sem(0, "print settings dialog")),
		status(B_CANCELED)
	{
	}

	~PrintDialogResult()
	{
		if (done >= 0)
			delete_sem(done);
	}
};


class PrintSettingsWindow : public BWindow {
public:
								PrintSettingsWindow(const char* title,
									bool printMode, const BMessage& initial,
									PrintDialogResult* result);

	virtual void				MessageReceived(BMessage* message);
	virtual bool				QuitRequested();

private:
	void						_BuildPageSetup();
	void						_BuildPrint();
	void						_BuildLayout(BView* content);
	void						_Finish(status_t status);
	void						_ApplyPageSetup(BMessage& settings) const;
	void						_ApplyPrint(BMessage& settings) const;
	float						_FloatValue(BTextControl* control,
									float fallback) const;
	void						_MarkPaper(const BRect& paper, bool landscape);
	int32						_Resolution() const;

	BMessage					fInitial;
	PrintDialogResult*			fResult;
	bool						fPrintMode;
	bool						fFinished;

	BMenuField*					fPaperField;
	BPopUpMenu*					fPaperMenu;
	BRadioButton*				fPortrait;
	BRadioButton*				fLandscape;
	BTextControl*				fMarginLeft;
	BTextControl*				fMarginRight;
	BTextControl*				fMarginTop;
	BTextControl*				fMarginBottom;
	BMenuField*					fResolutionField;
	BPopUpMenu*					fResolutionMenu;

	BStringView*					fPrinterView;
	BTextControl*				fCopies;
	BRadioButton*				fColor;
	BRadioButton*				fBlackAndWhite;
	BRadioButton*				fAllPages;
	BRadioButton*				fPageRange;
	BTextControl*				fFirstPage;
	BTextControl*				fLastPage;
	BTextControl*				fOutputPath;

	BButton*						fCancel;
	BButton*						fOK;

	enum {
		kMsgPageSetupAccept = 'psok',
		kMsgPrintAccept = 'prok',
		kMsgAllPages = 'pall',
		kMsgPageRange = 'prng'
	};
};


static const char*
PaperLabel(int32 index)
{
	return B_TRANSLATE(kPaperSizes[index].name);
}


static int32
PaperIndexForSize(float width, float height)
{
	for (int32 i = 0; i < kPaperSizeCount; i++) {
		if ((fabs(kPaperSizes[i].width - width) < 0.5f
				&& fabs(kPaperSizes[i].height - height) < 0.5f)
			|| (fabs(kPaperSizes[i].width - height) < 0.5f
				&& fabs(kPaperSizes[i].height - width) < 0.5f)) {
			return i;
		}
	}

	return 0;
}


static BTextControl*
CreateNumberField(const char* name, const char* label, const char* value)
{
	BTextControl* control = new BTextControl(name, label, value, NULL);
	control->SetExplicitMinSize(BSize(72.0f, B_SIZE_UNSET));
	return control;
}


static BStringView*
CreateLabel(const char* name, const char* text)
{
	BStringView* view = new BStringView(name, text);
	view->SetExplicitMinSize(BSize(110.0f, B_SIZE_UNSET));
	return view;
}


PrintSettingsWindow::PrintSettingsWindow(const char* title, bool printMode,
	const BMessage& initial, PrintDialogResult* result)
	:
	BWindow(BRect(0, 0, 0, 0), title, B_TITLED_WINDOW_LOOK,
		B_MODAL_APP_WINDOW_FEEL,
		B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS
			| B_CLOSE_ON_ESCAPE),
	fInitial(initial),
	fResult(result),
	fPrintMode(printMode),
	fFinished(false),
	fPaperField(NULL),
	fPaperMenu(NULL),
	fPortrait(NULL),
	fLandscape(NULL),
	fMarginLeft(NULL),
	fMarginRight(NULL),
	fMarginTop(NULL),
	fMarginBottom(NULL),
	fResolutionField(NULL),
	fResolutionMenu(NULL),
	fPrinterView(NULL),
	fCopies(NULL),
	fColor(NULL),
	fBlackAndWhite(NULL),
	fAllPages(NULL),
	fPageRange(NULL),
	fFirstPage(NULL),
	fLastPage(NULL),
	fOutputPath(NULL),
	fCancel(NULL),
	fOK(NULL)
{
	if (fPrintMode)
		_BuildPrint();
	else
		_BuildPageSetup();

	CenterOnScreen();
}


void
PrintSettingsWindow::_BuildLayout(BView* content)
{
	fCancel = new BButton("cancel", B_TRANSLATE("Cancel"),
		new BMessage(B_QUIT_REQUESTED));
	fOK = new BButton("ok", fPrintMode ? B_TRANSLATE("Print")
			: B_TRANSLATE("OK"), new BMessage(fPrintMode
				? kMsgPrintAccept : kMsgPageSetupAccept));
	SetDefaultButton(fOK);

	BLayoutBuilder::Group<>(this, B_VERTICAL)
		.SetInsets(B_USE_DEFAULT_SPACING)
		.Add(content)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(fCancel)
			.Add(fOK)
		.End();
}


void
PrintSettingsWindow::_MarkPaper(const BRect& paper, bool landscape)
{
	float width = paper.Width();
	float height = paper.Height();
	if (landscape)
		std::swap(width, height);

	int32 index = PaperIndexForSize(width, height);
	for (int32 i = 0; i < kPaperSizeCount; i++) {
		BMenuItem* item = fPaperMenu->ItemAt(i);
		if (item != NULL)
			item->SetMarked(i == index);
	}
}


void
PrintSettingsWindow::_BuildPageSetup()
{
	BRect paper = DefaultPaperRect();
	BRect printable = DefaultPrintableRect();
	if (fInitial.FindRect(PSRV_FIELD_PAPER_RECT, &paper) != B_OK
		|| !paper.IsValid()) {
		paper = DefaultPaperRect();
	}
	if (fInitial.FindRect(PSRV_FIELD_PRINTABLE_RECT, &printable) != B_OK
		|| !printable.IsValid()) {
		printable = BRect(paper.left + kDefaultMargin,
			paper.top + kDefaultMargin, paper.right - kDefaultMargin,
			paper.bottom - kDefaultMargin);
	}

	int32 orientation = paper.Width() > paper.Height() ? 1 : 0;
	fInitial.FindInt32(PSRV_FIELD_ORIENTATION, &orientation);
	bool landscape = orientation != 0;

	fPaperMenu = new BPopUpMenu("paper");
	for (int32 i = 0; i < kPaperSizeCount; i++) {
		fPaperMenu->AddItem(new BMenuItem(PaperLabel(i),
			new BMessage('ppap')));
	}
	fPaperField = new BMenuField("paper size", B_TRANSLATE("Paper size:"),
		fPaperMenu);
	_MarkPaper(paper, landscape);

	fPortrait = new BRadioButton("portrait", B_TRANSLATE("Portrait"), NULL);
	fLandscape = new BRadioButton("landscape", B_TRANSLATE("Landscape"),
		NULL);
	fPortrait->SetValue(landscape ? B_CONTROL_OFF : B_CONTROL_ON);
	fLandscape->SetValue(landscape ? B_CONTROL_ON : B_CONTROL_OFF);

	float left = printable.left - paper.left;
	float right = paper.right - printable.right;
	float top = printable.top - paper.top;
	float bottom = paper.bottom - printable.bottom;
	if (left < 0) left = 0;
	if (right < 0) right = 0;
	if (top < 0) top = 0;
	if (bottom < 0) bottom = 0;

	char value[32];
	snprintf(value, sizeof(value), "%.0f", left);
	fMarginLeft = CreateNumberField("left margin", B_TRANSLATE("Left:"),
		value);
	snprintf(value, sizeof(value), "%.0f", right);
	fMarginRight = CreateNumberField("right margin", B_TRANSLATE("Right:"),
		value);
	snprintf(value, sizeof(value), "%.0f", top);
	fMarginTop = CreateNumberField("top margin", B_TRANSLATE("Top:"),
		value);
	snprintf(value, sizeof(value), "%.0f", bottom);
	fMarginBottom = CreateNumberField("bottom margin",
		B_TRANSLATE("Bottom:"), value);

	int32 resolution = kDefaultResolution;
	int64 resolution64;
	if (fInitial.FindInt64(PSRV_FIELD_XRES, &resolution64) == B_OK)
		resolution = (int32)resolution64;
	else if (fInitial.FindInt32(PSRV_FIELD_XRES, &resolution) != B_OK)
		resolution = kDefaultResolution;

	fResolutionMenu = new BPopUpMenu("resolution");
	const int32 resolutions[] = { 150, 300, 600 };
	for (size_t i = 0; i < sizeof(resolutions) / sizeof(resolutions[0]);
			i++) {
		char label[32];
		snprintf(label, sizeof(label), "%" B_PRId32 " dpi", resolutions[i]);
		fResolutionMenu->AddItem(new BMenuItem(label, new BMessage('pres')));
		if (resolutions[i] == resolution)
			fResolutionMenu->ItemAt(i)->SetMarked(true);
	}
	fResolutionField = new BMenuField("resolution",
		B_TRANSLATE("Resolution:"), fResolutionMenu);

	BView* content = new BView("content", B_WILL_DRAW);
	BGridLayout* grid = new BGridLayout(0.0f, 0.0f);
	content->SetLayout(grid);
	grid->SetSpacing(6.0f, 6.0f);
	grid->AddView(fPaperField, 0, 0, 2);
	grid->AddView(CreateLabel("orientation label",
		B_TRANSLATE("Orientation:")), 0, 1);
	BGroupView* orientationGroup = new BGroupView(B_HORIZONTAL, 6.0f);
	orientationGroup->GroupLayout()->AddView(fPortrait);
	orientationGroup->GroupLayout()->AddView(fLandscape);
	grid->AddView(orientationGroup, 1, 1);

	grid->AddView(CreateLabel("margins label", B_TRANSLATE("Margins:")),
		0, 2);
	grid->AddView(fMarginLeft, 1, 2);
	grid->AddView(fMarginRight, 1, 3);
	grid->AddView(fMarginTop, 1, 4);
	grid->AddView(fMarginBottom, 1, 5);
	grid->AddView(fResolutionField, 0, 6, 2);

	_BuildLayout(content);
}


void
PrintSettingsWindow::_BuildPrint()
{
	fPrinterView = new BStringView("printer",
		B_TRANSLATE("Printer: Print to File (PDF)"));

	int32 copies = 1;
	fInitial.FindInt32(PSRV_FIELD_COPIES, &copies);
	if (copies < 1)
		copies = 1;
	char value[32];
	snprintf(value, sizeof(value), "%" B_PRId32, copies);
	fCopies = CreateNumberField("copies", B_TRANSLATE("Copies:"), value);

	bool color = true;
	if (fInitial.FindBool("color", &color) != B_OK) {
		int32 colorValue;
		if (fInitial.FindInt32("color", &colorValue) == B_OK)
			color = colorValue != 0;
	}
	fColor = new BRadioButton("color", B_TRANSLATE("Color"), NULL);
	fBlackAndWhite = new BRadioButton("bw",
		B_TRANSLATE("Black & white"), NULL);
	fColor->SetValue(color ? B_CONTROL_ON : B_CONTROL_OFF);
	fBlackAndWhite->SetValue(color ? B_CONTROL_OFF : B_CONTROL_ON);

	int32 firstPage = 0;
	int32 lastPage = INT32_MAX;
	fInitial.FindInt32(PSRV_FIELD_FIRST_PAGE, &firstPage);
	fInitial.FindInt32(PSRV_FIELD_LAST_PAGE, &lastPage);
	bool allPages = firstPage == 0
		&& (lastPage == INT32_MAX || lastPage < 0);

	fAllPages = new BRadioButton("all pages",
		B_TRANSLATE("All pages"), new BMessage(kMsgAllPages));
	fPageRange = new BRadioButton("page range",
		B_TRANSLATE("Pages:"), new BMessage(kMsgPageRange));
	fAllPages->SetValue(allPages ? B_CONTROL_ON : B_CONTROL_OFF);
	fPageRange->SetValue(allPages ? B_CONTROL_OFF : B_CONTROL_ON);

	snprintf(value, sizeof(value), "%" B_PRId32,
		firstPage >= 0 ? firstPage + 1 : 1);
	fFirstPage = CreateNumberField("first page", B_TRANSLATE("From:"),
		value);
	if (lastPage == INT32_MAX || lastPage < 0)
		value[0] = '\0';
	else
		snprintf(value, sizeof(value), "%" B_PRId32, lastPage + 1);
	fLastPage = CreateNumberField("last page", B_TRANSLATE("To:"), value);
	fFirstPage->SetEnabled(!allPages);
	fLastPage->SetEnabled(!allPages);

	const char* outputPath = NULL;
	if (fInitial.FindString(kOutputPathField, &outputPath) != B_OK)
		outputPath = "";
	fOutputPath = new BTextControl("output path",
		B_TRANSLATE("Output file:"), outputPath, NULL);
	fOutputPath->SetExplicitMinSize(BSize(300.0f, B_SIZE_UNSET));

	BView* content = new BView("content", B_WILL_DRAW);
	BGridLayout* grid = new BGridLayout(0.0f, 0.0f);
	content->SetLayout(grid);
	grid->SetSpacing(6.0f, 6.0f);
	grid->AddView(fPrinterView, 0, 0, 2);
	grid->AddView(fCopies, 0, 1, 2);
	grid->AddView(CreateLabel("color label", B_TRANSLATE("Color:")),
		0, 2);
	BGroupView* colorGroup = new BGroupView(B_HORIZONTAL, 6.0f);
	colorGroup->GroupLayout()->AddView(fColor);
	colorGroup->GroupLayout()->AddView(fBlackAndWhite);
	grid->AddView(colorGroup, 1, 2);

	grid->AddView(CreateLabel("range label", B_TRANSLATE("Page range:")),
		0, 3);
	BGroupView* rangeGroup = new BGroupView(B_HORIZONTAL, 6.0f);
	rangeGroup->GroupLayout()->AddView(fAllPages);
	rangeGroup->GroupLayout()->AddView(fPageRange);
	grid->AddView(rangeGroup, 1, 3);

	BGroupView* pageFields = new BGroupView(B_HORIZONTAL, 6.0f);
	pageFields->GroupLayout()->AddView(fFirstPage);
	pageFields->GroupLayout()->AddView(fLastPage);
	grid->AddView(pageFields, 1, 4);

	grid->AddView(fOutputPath, 0, 5, 2);
	grid->AddView(new BStringView("output hint",
		B_TRANSLATE("Leave blank for an automatic file name.")), 0, 6, 2);

	_BuildLayout(content);
}


float
PrintSettingsWindow::_FloatValue(BTextControl* control, float fallback) const
{
	if (control == NULL || control->Text() == NULL
		|| control->Text()[0] == '\0') {
		return fallback;
	}

	return atof(control->Text());
}


int32
PrintSettingsWindow::_Resolution() const
{
	BMenuItem* item = fResolutionMenu != NULL
		? fResolutionMenu->FindMarked() : NULL;
	if (item == NULL)
		return kDefaultResolution;

	return atoi(item->Label());
}


void
PrintSettingsWindow::_ApplyPageSetup(BMessage& settings) const
{
	BMenuItem* paperItem = fPaperMenu->FindMarked();
	int32 paperIndex = 0;
	if (paperItem != NULL) {
		for (int32 i = 0; i < kPaperSizeCount; i++) {
			if (strcmp(paperItem->Label(), PaperLabel(i)) == 0) {
				paperIndex = i;
				break;
			}
		}
	}

	bool landscape = fLandscape->Value() == B_CONTROL_ON;
	float paperWidth = kPaperSizes[paperIndex].width;
	float paperHeight = kPaperSizes[paperIndex].height;
	if (landscape)
		std::swap(paperWidth, paperHeight);

	float left = _FloatValue(fMarginLeft, kDefaultMargin);
	float right = _FloatValue(fMarginRight, kDefaultMargin);
	float top = _FloatValue(fMarginTop, kDefaultMargin);
	float bottom = _FloatValue(fMarginBottom, kDefaultMargin);
	if (left < 0) left = 0;
	if (right < 0) right = 0;
	if (top < 0) top = 0;
	if (bottom < 0) bottom = 0;
	if (left + right > paperWidth - 36) {
		left = right = kDefaultMargin;
	}
	if (top + bottom > paperHeight - 36) {
		top = bottom = kDefaultMargin;
	}

	settings.RemoveName(PSRV_FIELD_PAPER_RECT);
	settings.AddRect(PSRV_FIELD_PAPER_RECT,
		BRect(0, 0, paperWidth, paperHeight));
	settings.RemoveName(PSRV_FIELD_PRINTABLE_RECT);
	settings.AddRect(PSRV_FIELD_PRINTABLE_RECT,
		BRect(left, top, paperWidth - right, paperHeight - bottom));
	settings.RemoveName(PSRV_FIELD_ORIENTATION);
	settings.AddInt32(PSRV_FIELD_ORIENTATION, landscape ? 1 : 0);

	int32 resolution = _Resolution();
	settings.RemoveName(PSRV_FIELD_XRES);
	settings.RemoveName(PSRV_FIELD_YRES);
	settings.AddInt64(PSRV_FIELD_XRES, resolution);
	settings.AddInt64(PSRV_FIELD_YRES, resolution);
}


void
PrintSettingsWindow::_ApplyPrint(BMessage& settings) const
{
	int32 copies = atoi(fCopies->Text());
	if (copies < 1)
		copies = 1;
	if (copies > 9999)
		copies = 9999;
	settings.RemoveName(PSRV_FIELD_COPIES);
	settings.AddInt32(PSRV_FIELD_COPIES, copies);

	settings.RemoveName("color");
	settings.AddBool("color", fColor->Value() == B_CONTROL_ON);

	int32 firstPage = 0;
	int32 lastPage = INT32_MAX;
	if (fPageRange->Value() == B_CONTROL_ON) {
		firstPage = atoi(fFirstPage->Text());
		if (firstPage < 1)
			firstPage = 1;
		firstPage--;

		if (fLastPage->Text() != NULL && fLastPage->Text()[0] != '\0')
			lastPage = atoi(fLastPage->Text());
		if (lastPage < 1)
			lastPage = INT32_MAX;
		else
			lastPage--;
		if (lastPage < firstPage)
			lastPage = firstPage;
	}

	settings.RemoveName(PSRV_FIELD_FIRST_PAGE);
	settings.RemoveName(PSRV_FIELD_LAST_PAGE);
	settings.AddInt32(PSRV_FIELD_FIRST_PAGE, firstPage);
	settings.AddInt32(PSRV_FIELD_LAST_PAGE, lastPage);

	settings.RemoveName(kOutputPathField);
	settings.AddString(kOutputPathField, fOutputPath->Text());
}


void
PrintSettingsWindow::_Finish(status_t status)
{
	if (fFinished)
		return;
	fFinished = true;

	if (status == B_OK) {
		BMessage settings(fInitial);
		if (fPrintMode)
			_ApplyPrint(settings);
		else
			_ApplyPageSetup(settings);
		fResult->settings = settings;
	}

	fResult->status = status;
	if (fResult->done >= 0)
		release_sem(fResult->done);
}


bool
PrintSettingsWindow::QuitRequested()
{
	_Finish(B_CANCELED);
	return false;
}


void
PrintSettingsWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgPageSetupAccept:
		case kMsgPrintAccept:
			_Finish(B_OK);
			break;

		case kMsgAllPages:
			fFirstPage->SetEnabled(false);
			fLastPage->SetEnabled(false);
			break;

		case kMsgPageRange:
			fFirstPage->SetEnabled(true);
			fLastPage->SetEnabled(true);
			break;

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


status_t
ShowSettingsWindow(const char* title, bool printMode, BMessage& settings)
{
	if (be_app == NULL || getenv("COSMOE_PRINT_QUIET") != NULL)
		return B_OK;

	PrintDialogResult result;
	if (result.done < 0)
		return B_ERROR;

	PrintSettingsWindow* window = new(std::nothrow) PrintSettingsWindow(title,
		printMode, settings, &result);
	if (window == NULL)
		return B_NO_MEMORY;

	window->Show();

	status_t status;
	do {
		status = acquire_sem(result.done);
	} while (status == B_INTERRUPTED);

	if (status == B_OK && result.status == B_OK)
		settings = result.settings;

	if (window->Lock())
		window->Quit();

	return status == B_OK ? result.status : status;
}


struct QueuedPicture {
	BPicture*	picture;
	BPoint		where;
	BRect		rect;

	QueuedPicture()
		:
		picture(NULL),
		where(B_ORIGIN),
		rect()
	{
	}
};


}	// namespace


struct _page_header_ {
	std::vector<QueuedPicture>	pictures;
	cairo_surface_t*			surface;
	BString						outputPath;
	int32						pageCount;
	bool						active;
	bool						completed;

	_page_header_()
		:
		surface(NULL),
		pageCount(0),
		active(false),
		completed(false)
	{
	}

	~_page_header_()
	{
		ClearPictures();
		if (surface != NULL)
			cairo_surface_destroy(surface);
	}

	void ClearPictures()
	{
		for (size_t i = 0; i < pictures.size(); i++)
			delete pictures[i].picture;
		pictures.clear();
	}
};


namespace {

bool
PaintPage(_page_header_* state, const BRect& paper, const BRect& printable,
	int32 xdpi, int32 ydpi)
{
	if (state == NULL || state->surface == NULL)
		return false;

	if (xdpi <= 0)
		xdpi = kDefaultResolution;
	if (ydpi <= 0)
		ydpi = kDefaultResolution;

	const double scaleX = (double)xdpi / 72.0;
	const double scaleY = (double)ydpi / 72.0;
	// The recorded picture is relative to the view's printable rect, so its
	// first line can start a little above the bitmap origin. Keep headroom
	// for that content and compensate when placing the image in the PDF.
	const double kPageTopPadding = 8.0;
	const int32 pixelWidth = (int32)ceil(paper.Width() * scaleX);
	const int32 pixelHeight = (int32)ceil(paper.Height() * scaleY);
	if (pixelWidth <= 0 || pixelHeight <= 0)
		return false;

	BBitmap bitmap(BRect(0, 0, pixelWidth - 1, pixelHeight - 1), B_RGB32,
		true);
	if (bitmap.InitCheck() != B_OK)
		return false;

	BView* page = new(std::nothrow) BView(bitmap.Bounds(), "print page",
		B_FOLLOW_NONE, 0);
	if (page == NULL)
		return false;

	bitmap.AddChild(page);

	if (!bitmap.Lock())
		return false;

	// Printed pages are white, not transparent. Fill before changing scale so
	// the scale applies only to the replayed page contents.
	page->SetHighColor(255, 255, 255);
	page->FillRect(page->Bounds());
	page->SetScale((float)scaleX);

	for (size_t i = 0; i < state->pictures.size(); i++) {
		const QueuedPicture& item = state->pictures[i];
		if (item.picture != NULL) {
			page->DrawPicture(item.picture,
				item.where + BPoint(0.0, kPageTopPadding));
		}
	}

	bitmap.Unlock();

	// Bits() synchronizes the offscreen window's backing surface into the
	// bitmap before handing out the buffer.
	unsigned char* bits = (unsigned char*)bitmap.Bits();
	if (bits == NULL)
		return false;

	cairo_surface_t* image = cairo_image_surface_create_for_data(bits,
		CAIRO_FORMAT_RGB24, pixelWidth, pixelHeight, bitmap.BytesPerRow());
	if (cairo_surface_status(image) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(image);
		return false;
	}
	cairo_surface_mark_dirty(image);

	cairo_t* context = cairo_create(state->surface);
	if (context == NULL || cairo_status(context) != CAIRO_STATUS_SUCCESS) {
		if (context != NULL)
			cairo_destroy(context);
		cairo_surface_destroy(image);
		return false;
	}

	cairo_save(context);
	// The page bitmap contains the printable area from its origin. Place the
	// finished image at the printable origin instead of translating the
	// picture itself, whose recorded clips would not follow the translation.
	cairo_translate(context, printable.left, printable.top - kPageTopPadding);
	cairo_scale(context, paper.Width() / pixelWidth,
		paper.Height() / pixelHeight);
	cairo_set_source_surface(context, image, 0, 0);
	cairo_paint(context);
	cairo_restore(context);
	cairo_show_page(context);

	cairo_status_t status = cairo_status(context);

	cairo_destroy(context);
	cairo_surface_destroy(image);

	return status == CAIRO_STATUS_SUCCESS;
}

}	// namespace


// #pragma mark -- BPrintJob


BPrintJob::BPrintJob(const char* jobName)
	:
	fPrintJobName(NULL),
	fSpoolFile(NULL),
	fPaperSize(DefaultPaperRect()),
	fUsableSize(DefaultPrintableRect()),
	fError(B_OK),
	fSetupMessage(NULL),
	fDefaultSetupMessage(NULL),
	fAbort(0),
	fFirstPage(0),
	fLastPage(INT32_MAX),
	fXResolution(kDefaultResolution),
	fYResolution(kDefaultResolution),
	fCurrentPageHeader(NULL),
	fCurrentPageHeaderOffset(0)
{
	fSpoolFileName[0] = '\0';

	if (jobName != NULL && jobName[0] != '\0')
		fPrintJobName = strdup(jobName);

	fCurrentPageHeader = new(std::nothrow) _page_header_;
	if (fCurrentPageHeader == NULL)
		fError = B_NO_MEMORY;
}


BPrintJob::~BPrintJob()
{
	CancelJob();

	free(fPrintJobName);
	delete fSetupMessage;
	delete fDefaultSetupMessage;
	delete fCurrentPageHeader;
}


status_t
BPrintJob::ConfigPage()
{
	if (fSetupMessage == NULL)
		_LoadDefaultSettings();

	if (fSetupMessage == NULL)
		return B_NO_MEMORY;

	status_t status = ShowSettingsWindow(B_TRANSLATE("Page Setup"), false,
		*fSetupMessage);
	if (status != B_OK) {
		fError = status;
		return status;
	}

	_HandlePageSetup(fSetupMessage);
	SaveSettings(fSetupMessage);

	fError = B_OK;
	return B_OK;
}


status_t
BPrintJob::ConfigJob()
{
	if (fSetupMessage == NULL)
		_LoadDefaultSettings();

	if (fSetupMessage == NULL)
		return B_NO_MEMORY;

	status_t status = ShowSettingsWindow(B_TRANSLATE("Print"), true,
		*fSetupMessage);
	if (status != B_OK) {
		fError = status;
		return status;
	}

	_HandlePrintSetup(fSetupMessage);
	SaveSettings(fSetupMessage);

	fError = B_OK;
	return B_OK;
}


void
BPrintJob::BeginJob()
{
	fError = B_ERROR;

	if (fCurrentPageHeader == NULL)
		return;

	// Cannot start a new job until the previous one has been committed or
	// cancelled.
	if (fCurrentPageHeader->active)
		return;

	if (fSetupMessage == NULL)
		_LoadDefaultSettings();
	if (fSetupMessage == NULL)
		return;

	BPath directory;
	status_t status = find_directory(B_USER_PRINTERS_DIRECTORY, &directory);
	if (status != B_OK)
		return;

	status = create_directory(directory.Path(), 0755);
	if (status != B_OK && status != B_FILE_EXISTS && status != B_NAME_IN_USE)
		return;

	const char* configuredPath = NULL;
	if (fSetupMessage->FindString(kOutputPathField, &configuredPath) == B_OK
		&& configuredPath != NULL && configuredPath[0] != '\0') {
		fCurrentPageHeader->outputPath = configuredPath;
	} else {
		char mangledName[B_FILE_NAME_LENGTH];
		_GetMangledName(mangledName, B_FILE_NAME_LENGTH);

		BString fileName(mangledName);
		fileName << ".pdf";
		directory.Append(fileName.String());
		if (directory.InitCheck() != B_OK)
			return;

		fCurrentPageHeader->outputPath = directory.Path();
	}

	if (fCurrentPageHeader->outputPath.Length()
		>= (int32)sizeof(fSpoolFileName)) {
		fError = B_NAME_TOO_LONG;
		return;
	}

	strlcpy(fSpoolFileName, fCurrentPageHeader->outputPath.String(),
		sizeof(fSpoolFileName));

	fCurrentPageHeader->ClearPictures();
	fCurrentPageHeader->pageCount = 0;
	fCurrentPageHeader->active = true;
	fCurrentPageHeader->completed = false;

	fAbort = 0;
	fError = B_OK;
}


void
BPrintJob::CommitJob()
{
	if (fCurrentPageHeader == NULL || !fCurrentPageHeader->active)
		return;

	if (fCurrentPageHeader->pageCount == 0) {
		ShowError(B_TRANSLATE("No pages to print!"));
		CancelJob();
		return;
	}

	cairo_status_t status = CAIRO_STATUS_SUCCESS;
	if (fCurrentPageHeader->surface != NULL) {
		cairo_surface_finish(fCurrentPageHeader->surface);
		status = cairo_surface_status(fCurrentPageHeader->surface);
		cairo_surface_destroy(fCurrentPageHeader->surface);
		fCurrentPageHeader->surface = NULL;
	}

	fCurrentPageHeader->active = false;
	fCurrentPageHeader->ClearPictures();

	if (status != CAIRO_STATUS_SUCCESS) {
		BEntry(fCurrentPageHeader->outputPath.String()).Remove();
		fCurrentPageHeader->outputPath.SetTo("");
		fSpoolFileName[0] = '\0';
		fError = B_ERROR;
		ShowError(B_TRANSLATE("Could not create the PDF file."));
		return;
	}

	fCurrentPageHeader->completed = true;
	fError = B_OK;

	// Tests and other headless callers can suppress the completion alert.
	if (getenv("COSMOE_PRINT_QUIET") == NULL) {
		char message[1024];
		snprintf(message, sizeof(message), B_TRANSLATE("Printed to:\n%s"),
			fCurrentPageHeader->outputPath.String());
		ShowAlert(B_TRANSLATE("Print"), message);
	}
}


void
BPrintJob::CancelJob()
{
	if (fCurrentPageHeader == NULL)
		return;

	fAbort = 1;

	if (fCurrentPageHeader->surface != NULL) {
		cairo_surface_destroy(fCurrentPageHeader->surface);
		fCurrentPageHeader->surface = NULL;
	}

	fCurrentPageHeader->ClearPictures();

	if (!fCurrentPageHeader->completed
		&& fCurrentPageHeader->outputPath.Length() > 0) {
		BEntry(fCurrentPageHeader->outputPath.String()).Remove();
	}

	fCurrentPageHeader->active = false;
	fCurrentPageHeader->completed = false;
	fCurrentPageHeader->outputPath.SetTo("");
	fCurrentPageHeader->pageCount = 0;

	fSpoolFileName[0] = '\0';
}


void
BPrintJob::SpoolPage()
{
	if (fCurrentPageHeader == NULL || !fCurrentPageHeader->active)
		return;

	if (fCurrentPageHeader->pictures.empty())
		return;

	if (fCurrentPageHeader->surface == NULL) {
		const double width = fPaperSize.Width();
		const double height = fPaperSize.Height();
		if (width <= 0 || height <= 0) {
			fError = B_BAD_VALUE;
			CancelJob();
			return;
		}

		fCurrentPageHeader->surface = cairo_pdf_surface_create(
			fCurrentPageHeader->outputPath.String(), width, height);
		if (cairo_surface_status(fCurrentPageHeader->surface)
				!= CAIRO_STATUS_SUCCESS) {
			fError = B_ERROR;
			CancelJob();
			return;
		}

		cairo_pdf_surface_set_metadata(fCurrentPageHeader->surface,
			CAIRO_PDF_METADATA_TITLE,
			fPrintJobName != NULL ? fPrintJobName : "Print Job");
		cairo_pdf_surface_set_metadata(fCurrentPageHeader->surface,
			CAIRO_PDF_METADATA_CREATOR, "Cosmoe");
	}

	if (!PaintPage(fCurrentPageHeader, fPaperSize, fUsableSize, fXResolution,
			fYResolution)
		|| cairo_surface_status(fCurrentPageHeader->surface)
			!= CAIRO_STATUS_SUCCESS) {
		fError = B_ERROR;
		CancelJob();
		return;
	}

	fCurrentPageHeader->pageCount++;
	fCurrentPageHeader->ClearPictures();
}


bool
BPrintJob::CanContinue()
{
	return fError == B_OK && !fAbort;
}


void
BPrintJob::DrawView(BView* view, BRect rect, BPoint where)
{
	if (fCurrentPageHeader == NULL || !fCurrentPageHeader->active)
		return;

	if (view == NULL)
		return;

	if (view->LockLooper()) {
		BPicture picture;
		_RecurseView(view, B_ORIGIN - rect.LeftTop(), &picture, rect);
		_AddPicture(picture, rect, where);
		view->UnlockLooper();
	}
}


BMessage*
BPrintJob::Settings()
{
	if (fSetupMessage == NULL)
		_LoadDefaultSettings();

	if (fSetupMessage == NULL)
		return NULL;

	return new BMessage(*fSetupMessage);
}


void
BPrintJob::SetSettings(BMessage* message)
{
	if (message != NULL) {
		if (fDefaultSetupMessage == NULL)
			_LoadDefaultSettings();
		_HandlePrintSetup(message);
	}

	delete fSetupMessage;
	fSetupMessage = message;
}


bool
BPrintJob::IsSettingsMessageValid(BMessage* message) const
{
	if (message == NULL)
		return false;

	const char* messageName = NULL;
	if (message->FindString("printer_name", &messageName) != B_OK) {
		message->FindString(PSRV_FIELD_CURRENT_PRINTER, &messageName);
	}

	if (messageName == NULL)
		return false;

	char* currentName = _GetCurrentPrinterName();
	if (currentName == NULL)
		return true;

	bool valid = strcmp(currentName, messageName) == 0;
	free(currentName);

	return valid;
}


BRect
BPrintJob::PaperRect()
{
	if (fDefaultSetupMessage == NULL)
		_LoadDefaultSettings();

	return fPaperSize;
}


BRect
BPrintJob::PrintableRect()
{
	if (fDefaultSetupMessage == NULL)
		_LoadDefaultSettings();

	return fUsableSize;
}


void
BPrintJob::GetResolution(int32* xdpi, int32* ydpi)
{
	if (fDefaultSetupMessage == NULL)
		_LoadDefaultSettings();

	if (xdpi != NULL)
		*xdpi = fXResolution;

	if (ydpi != NULL)
		*ydpi = fYResolution;
}


int32
BPrintJob::FirstPage()
{
	return fFirstPage;
}


int32
BPrintJob::LastPage()
{
	return fLastPage;
}


int32
BPrintJob::PrinterType(void*) const
{
	if (fDefaultSetupMessage == NULL)
		const_cast<BPrintJob*>(this)->_LoadDefaultSettings();

	bool color = true;
	if (fSetupMessage != NULL
		&& fSetupMessage->FindBool("color", &color) != B_OK) {
		int32 value;
		if (fSetupMessage->FindInt32("color", &value) == B_OK)
			color = value != 0;
	}

	return color ? B_COLOR_PRINTER : B_BW_PRINTER;
}


// #pragma mark - private


void
BPrintJob::_RecurseView(BView* view, BPoint origin, BPicture* picture,
	BRect rect)
{
	ASSERT(picture != NULL);

	BRegion region;
	region.Set(BRect(rect.left, rect.top, rect.right, rect.bottom));
	view->fState->print_rect = rect;

	view->AppendToPicture(picture);
	view->PushState();
	view->SetOrigin(origin);
	view->ConstrainClippingRegion(&region);

	if (view->ViewColor() != B_TRANSPARENT_COLOR) {
		rgb_color highColor = view->HighColor();
		view->SetHighColor(view->ViewColor());
		view->FillRect(rect);
		view->SetHighColor(highColor);
	}

	if ((view->Flags() & B_WILL_DRAW) != 0) {
		view->fIsPrinting = true;
		view->Draw(rect);
		view->fIsPrinting = false;
	}

	view->PopState();
	view->EndPicture();

	BView* child = view->ChildAt(0);
	while (child != NULL) {
		if (!child->IsHidden()) {
			BPoint leftTop(view->Bounds().LeftTop() + child->Frame().LeftTop());
			BRect printRect(rect.OffsetToCopy(rect.LeftTop() - leftTop)
				& child->Bounds());
			if (printRect.IsValid())
				_RecurseView(child, origin + leftTop, picture, printRect);
		}
		child = child->NextSibling();
	}

	if ((view->Flags() & B_DRAW_ON_CHILDREN) != 0) {
		view->AppendToPicture(picture);
		view->PushState();
		view->SetOrigin(origin);
		view->ConstrainClippingRegion(&region);
		view->fIsPrinting = true;
		view->DrawAfterChildren(rect);
		view->fIsPrinting = false;
		view->PopState();
		view->EndPicture();
	}
}


void
BPrintJob::_GetMangledName(char* buffer, size_t bufferSize) const
{
	BString base(fPrintJobName != NULL && fPrintJobName[0] != '\0'
		? fPrintJobName : "print job");
	base.ReplaceAll("/", "_");
	base.ReplaceAll("\\", "_");
	base.ReplaceAll(":", "_");

	snprintf(buffer, bufferSize, "%s@%lld", base.String(),
		(long long)(system_time() / 1000));
}


void
BPrintJob::_HandlePageSetup(BMessage* setup)
{
	if (setup == NULL)
		return;

	setup->FindRect(PSRV_FIELD_PRINTABLE_RECT, &fUsableSize);
	setup->FindRect(PSRV_FIELD_PAPER_RECT, &fPaperSize);

	// Haiku's print server historically wrote xres/yres as int64, but some
	// messages contain int32 values. Accept both.
	int64 valueInt64;
	int32 valueInt32;
	if (setup->FindInt64(PSRV_FIELD_XRES, &valueInt64) == B_OK)
		fXResolution = (short)valueInt64;
	else if (setup->FindInt32(PSRV_FIELD_XRES, &valueInt32) == B_OK)
		fXResolution = (short)valueInt32;

	if (setup->FindInt64(PSRV_FIELD_YRES, &valueInt64) == B_OK)
		fYResolution = (short)valueInt64;
	else if (setup->FindInt32(PSRV_FIELD_YRES, &valueInt32) == B_OK)
		fYResolution = (short)valueInt32;
}


bool
BPrintJob::_HandlePrintSetup(BMessage* message)
{
	if (message == NULL)
		return false;

	_HandlePageSetup(message);

	int32 value;
	if (message->FindInt32(PSRV_FIELD_FIRST_PAGE, &value) == B_OK)
		fFirstPage = value;

	if (message->FindInt32(PSRV_FIELD_LAST_PAGE, &value) == B_OK)
		fLastPage = value;

	return true;
}


void
BPrintJob::_NewPage()
{
	if (fCurrentPageHeader != NULL)
		fCurrentPageHeader->ClearPictures();
}


void
BPrintJob::_EndLastPage()
{
	// Pages are rendered as they are spooled; nothing to finish here.
}


void
BPrintJob::_AddSetupSpec()
{
	// The spool-file settings header is not used by the in-process renderer.
}


void
BPrintJob::_AddPicture(BPicture& picture, BRect& rect, BPoint& where)
{
	if (fCurrentPageHeader == NULL)
		return;

	QueuedPicture item;
	item.picture = new(std::nothrow) BPicture(picture);
	if (item.picture == NULL) {
		fError = B_NO_MEMORY;
		return;
	}

	item.rect = rect;
	item.where = where;
	fCurrentPageHeader->pictures.push_back(item);
}


/*!	Returns a copy of the application's current printer name, or NULL if it
	could not be obtained. The caller is responsible for freeing the string
	with free().
*/
char*
BPrintJob::_GetCurrentPrinterName() const
{
	if (fSetupMessage == NULL && fDefaultSetupMessage == NULL)
		const_cast<BPrintJob*>(this)->_LoadDefaultSettings();

	const char* printerName = NULL;
	if (fSetupMessage != NULL) {
		if (fSetupMessage->FindString(PSRV_FIELD_CURRENT_PRINTER,
				&printerName) != B_OK) {
			fSetupMessage->FindString("printer_name", &printerName);
		}
	}

	if (printerName == NULL)
		printerName = kDefaultPrinterName;

	return strdup(printerName);
}


void
BPrintJob::_LoadDefaultSettings()
{
	if (fDefaultSetupMessage == NULL) {
		fDefaultSetupMessage = LoadSettings();
		if (fDefaultSetupMessage == NULL)
			fDefaultSetupMessage = MakeDefaultSettings();
	}

	if (fDefaultSetupMessage == NULL) {
		fError = B_NO_MEMORY;
		return;
	}

	if (fSetupMessage == NULL) {
		fSetupMessage = new(std::nothrow) BMessage(*fDefaultSetupMessage);
		if (fSetupMessage == NULL) {
			fError = B_NO_MEMORY;
			return;
		}
		_HandlePrintSetup(fSetupMessage);
	}
}


void BPrintJob::_ReservedPrintJob1() {}
void BPrintJob::_ReservedPrintJob2() {}
void BPrintJob::_ReservedPrintJob3() {}
void BPrintJob::_ReservedPrintJob4() {}
