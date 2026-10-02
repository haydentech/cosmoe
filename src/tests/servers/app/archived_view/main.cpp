// main.cpp
#include <stdio.h>
#include <inttypes.h>

#include <Application.h>
#include <Archivable.h>
#include <Button.h>
#include <Errors.h>
#include <File.h>
#include <Message.h>
#include <View.h>
#include <Window.h>

static const char* kAppSignature = "application/x.vnd-Haiku.ArchivedView";
static const uint32 kMsgIncrementData = 0x494e4352;

class TestView : public BView {

 public:
							TestView(BRect frame, const char* name,
									 uint32 resizeFlags, uint32 flags);

							TestView(BMessage* archive);

	virtual	status_t		Archive(BMessage* into, bool deep = true) const;

	static	BArchivable*	Instantiate(BMessage* archive);

	virtual	void			AttachedToWindow();
	virtual	void			DetachedFromWindow();
	virtual	void			MessageReceived(BMessage* message);

	virtual	void			Draw(BRect updateRect);

 private:
			void			_ConfigureDataButton();
			status_t		_SaveArchive() const;
			int32			fData;
};


// constructor
TestView::TestView(BRect frame, const char* name,
				   uint32 resizeFlags, uint32 flags)
	: BView(frame, name, resizeFlags, flags),
	  fData(42)
{
	SetViewColor(216, 216, 116);
	_ConfigureDataButton();
}

// constructor
TestView::TestView(BMessage* archive)
	: BView(archive),
	  fData(0)
{
	archive->FindInt32("data", &fData);
	printf("restored data: %" PRId32 "\n", fData);
	_ConfigureDataButton();
}

// _ConfigureDataButton
void
TestView::_ConfigureDataButton()
{
	BButton* button = dynamic_cast<BButton*>(FindView("incrementData"));
	if (button == NULL) {
		BRect r = Bounds();
		r.left = r.right - 170;
		r.top = r.bottom - 35;
		r.bottom = r.top + 30;
		button = new BButton(r, "incrementData", "Increment data",
			new BMessage(kMsgIncrementData));
		AddChild(button);
	} else
		button->SetMessage(new BMessage(kMsgIncrementData));
}

// AttachedToWindow
void
TestView::AttachedToWindow()
{
	BButton* button = dynamic_cast<BButton*>(FindView("incrementData"));
	if (button != NULL)
		button->SetTarget(this);
}

// DetachedFromWindow
void
TestView::DetachedFromWindow()
{
	status_t err = _SaveArchive();
	if (err != B_OK)
		fprintf(stderr, "error saving archive on detach: %s\n", strerror(err));
}

// _SaveArchive
status_t
TestView::_SaveArchive() const
{
	BFile file("/tmp/archived_view",
			   B_CREATE_FILE | B_ERASE_FILE | B_WRITE_ONLY);
	status_t err = file.InitCheck();
	if (err != B_OK) {
		return err;
	}
	BMessage archive;
	err = Archive(&archive);
	if (err != B_OK)
		return err;
	err = archive.Flatten(&file);
	if (err != B_OK)
		return err;
	printf("saved data: %" PRId32 "\n", fData);
	return B_OK;
}

// MessageReceived
void
TestView::MessageReceived(BMessage* message)
{
	if (message->what == kMsgIncrementData) {
		fData++;
		printf("data changed to: %" PRId32 "\n", fData);
		status_t err = _SaveArchive();
		if (err != B_OK)
			fprintf(stderr, "error saving updated archive: %s\n", strerror(err));
		return;
	}

	BView::MessageReceived(message);
}

// Draw
void
TestView::Draw(BRect updateRect)
{
	BRect r(Bounds());

	rgb_color light = tint_color(ViewColor(), B_LIGHTEN_2_TINT);
	rgb_color shadow = tint_color(ViewColor(), B_DARKEN_2_TINT);

	BeginLineArray(4);
		AddLine(r.LeftTop(), r.RightTop(), light);
		AddLine(r.RightTop(), r.RightBottom(), shadow);
		AddLine(r.RightBottom(), r.LeftBottom(), shadow);
		AddLine(r.LeftBottom(), r.LeftTop(), light);
	EndLineArray();
}

// Archive
status_t
TestView::Archive(BMessage* into, bool deep) const
{
	status_t ret = BView::Archive(into, deep);

	if (ret == B_OK)
		ret = into->AddInt32("data", fData);

	if (ret == B_OK)
		ret = into->AddString("add_on", kAppSignature);

	return ret;
}

// Instantiate
BArchivable *
TestView::Instantiate(BMessage* archive)
{
	if (!validate_instantiation(archive, "TestView"))
		return NULL;
	int32 data;
	if (archive->FindInt32("data", &data) != B_OK) {
		fprintf(stderr, "TestView archive is missing its data field\n");
		return NULL;
	}

	return new TestView(archive);
}

// #pragma mark -

// show_window
status_t
show_window(BRect frame, const char* name)
{
	BView* view = NULL;
	BFile file("/tmp/archived_view", B_READ_ONLY);
	status_t err = file.InitCheck();
	if (err == B_OK) {
		printf("found archive file\n");
		BMessage archive;
		err = archive.Unflatten(&file);
		if (err != B_OK) {
			fprintf(stderr, "failed to unflatten archive: %s\n", strerror(err));
			return err;
		}

		BArchivable* archivable = instantiate_object(&archive);
		if (archivable == NULL) {
			fprintf(stderr, "failed to instantiate archived view\n");
			return B_BAD_DATA;
		}
		view = dynamic_cast<BView*>(archivable);
		if (view == NULL) {
			delete archivable;
			fprintf(stderr, "archive instantiated an object that is not a BView\n");
			return B_BAD_TYPE;
		}
		printf("restored archived BView\n");
	} else if (err != B_ENTRY_NOT_FOUND) {
		fprintf(stderr, "failed to open archive file: %s\n", strerror(err));
		return err;
	} else {
		printf("no archive found; creating a new view\n");
	}

	BWindow* window = new BWindow(frame, name, B_TITLED_WINDOW,
		B_ASYNCHRONOUS_CONTROLS | B_QUIT_ON_WINDOW_CLOSE);
	if (view == NULL)
		view = new TestView(window->Bounds(), "test", B_FOLLOW_ALL, B_WILL_DRAW);

	window->Lock();
	window->AddChild(view);
	window->Unlock();
	window->Show();
	return B_OK;
}

// main
int
main(int argc, char** argv)
{
	BApplication* app = new BApplication(kAppSignature);

	BRect frame(50.0, 50.0, 300.0, 250.0);
	status_t err = show_window(frame, "BView Archiving Test");
	if (err != B_OK) {
		delete app;
		return 1;
	}

	app->Run();

	delete app;
	return 0;
}
