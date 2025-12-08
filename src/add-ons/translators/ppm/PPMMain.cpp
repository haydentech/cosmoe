/*
	Copyright 1999, Be Incorporated.   All Rights Reserved.
	This file may be used under the terms of the Be Sample Code License.
*/

#include <Alert.h>
#include <Application.h>
#include <Catalog.h>
#include <LayoutBuilder.h>
//#include <Screen.h>
#include <TranslatorAddOn.h>
#include <View.h>
#include <Window.h>


#include <stdio.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "PPMMain"


#include <stdio.h>
#include <stdlib.h>

// Used to make this simultaneously an executable and a shared library
#ifdef __x86_64__
const char service_interp[] __attribute__((section(".interp"))) = "/lib/ld-linux-x86-64.so.2";
#elif __aarch64__
const char service_interp[] __attribute__((section(".interp"))) = "/lib/ld-linux-aarch64.so.1";
#else
#error "Unsupported architecture - add the appropriate path for your platform"
#endif


// See https://stackoverflow.com/questions/1449987/building-a-so-that-is-also-an-executable/68339111#68339111
#ifdef __GLIBC__
/* magic to make glibc work more reliably. */
extern "C" {
extern int _IO_stdin_used;
int _IO_stdin_used __attribute__((weak)) = 131073;
}
#endif



BPoint get_window_origin();
void set_window_origin(BPoint pt);

class PPMWindow : public BWindow
{
public:
	PPMWindow(BRect area)
		: BWindow(area, B_TRANSLATE("PPM Settings"), B_TITLED_WINDOW,
			  B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS)
	{
		BLayoutBuilder::Group<>(this, B_HORIZONTAL);
	}
	~PPMWindow()
	{
		// BPoint pt(0, 0);
		// ConvertToScreen(&pt);
		// set_window_origin(pt);
		be_app->PostMessage(B_QUIT_REQUESTED);
	}
};

int
main()
{
	BApplication app("application/x-vnd.Haiku-PPMTranslator");
	BView* v = NULL;
	BRect r(0, 0, 1, 1);
	if (MakeConfig(NULL, &v, &r)) {
		BAlert* err = new BAlert("Error",
			B_TRANSLATE("Something is wrong with the PPMTranslator!"),
			B_TRANSLATE("OK"));
		err->SetFlags(err->Flags() | B_CLOSE_ON_ESCAPE);
		err->Go();
		return 1;
	}
	PPMWindow* w = new PPMWindow(r);
	v->ResizeTo(r.Width(), r.Height());
	w->AddChild(v);
	// BPoint o = get_window_origin();
	// {
	// 	BScreen scrn;
	// 	BRect f = scrn.Frame();
	// 	f.InsetBy(10, 23);
	// 	/* if not in a good place, start where the cursor is */
	// 	if (!f.Contains(o)) {
	// 		uint32 i;
	// 		v->GetMouse(&o, &i, false);
	// 		o.x -= r.Width() / 2;
	// 		o.y -= r.Height() / 2;
	// 		/* clamp location to screen */
	// 		if (o.x < f.left)
	// 			o.x = f.left;
	// 		if (o.y < f.top)
	// 			o.y = f.top;
	// 		if (o.x > f.right)
	// 			o.x = f.right;
	// 		if (o.y > f.bottom)
	// 			o.y = f.bottom;
	// 	}
	// }
	// w->MoveTo(o);
	w->Show();
	app.Run();
	return 0;
}
