# TODO

## Known Bugs on All Platforms

- Cairo complains on exit due to non-empty object hash (i.e. memory leak)
  - This assert is currently disabled.

- Modal alerts are not fully modal on Wayland
  - The alert stays frontmost, but input will still be processed in the parent window.

- ShowImage selection rectangle does not do the "marching ants" animation

- `BRecentFilesList` / `BRecentFolderList` / `BRecentAppList` only partially implemented

- Drag-n-Drop from external sources is not yet implemented on any backend

- Toolbar menus hang the window when opened

- `BView::RotateBy()` creates clipping issues
  - This currently only affects rotated tab labels as `BTab` is the only known code to use this functionality.
  - These clipping issues are the same on Haiku (See Tracker's Get Info window in the Permissions tab)

- Several APIs are empty stubs or absent altogether
  - `get_mouse()`
  - `BWindowStack` is all stubs
  - `BPrintJob` and other printing routines have no print server to talk to
  - `BFont::GetGlyphShapes`, `BFont::LoadFont`, `BFont::UnloadFont` are stubs
  - Media Kit audio writing functions and all video functions
  - Game Kit advanced sound playback and streaming functionality

- Deskbar does not position correctly when using GNOME Mutter as the Wayland compositor
  - This will likely never be fixed as GNOME intentionally omits support for the zwlr_layer_shell_v1 protocol, so no third-party panels are possible.  Yet another reason to dislike GNOME!
  - Menus also work very inconsistently in Deskbar under Mutter, though the reason is less clear.  Other apps' menus work fine under Mutter.

- `B_OP_SELECT` drawing should not transfer transparent pixels, but it does
  - `B_OP_SELECT` bitmap drawing transfers transparency to the target surface.
  - This can lead to views revealing the view underneath, or in the case of Wayland, views and windows that shows all the way through the window itself.
  - Future draws to the surface simply darken that area instead of overwriting it as they should, since the transparency has been transferred.
  - I literally can't find an app that uses B_OP_SELECT, other than Showcase's drawing tests, so not a big deal

- `BChannelSlider` can cause occasional hangs when the slider is moved and the tooltip shows

- Sometimes views will draw without erasing the background, causing drawing to overlay previous drawing, especially noticeable when the drawing is semi-transparent

- When a view's pen size is an even number, stroked drawing comes out blurry
  - This is partially a function of our conversion from Haiku to Cairo coordinates
  - We draw lines "on-center" by offseting by a half-pixel, but for an even pen size or scale factor, we end up drawing in-between pixels again
  - Haiku shifts the drawing up and left by another half-pixel to compensate in even-pen-width situations, so we could to the same, but I'm not sure that's best.  It's a special-case fix, not a general solution.

- Many window looks and feels are not reflected in the backend
  - If the app asks for a utility window look, or a system floating feel, currently you get just get a regular window.
  - Only borderless windows are currently supported (ironically via B_BORDERED_WINDOW)

- Enhancement: optional native file open/save dialogs on Windows and Mac

- The hack to make translators both shared libraries and launchable executables no longer works

- GetBoundingBoxes returns incorrect boxes if font shear or rotation are changed from the default

- `entry_ref` only works if `Name` holds an absolute path or dot-relative path
  - Converting from a `BEntry` or `GetNextRef` fills this out correctly.
  - Considering how extensively `entry_ref` is used, it's certain this is causing issues somewhere.
  - That said, I don't know of any remaining problems in the Cosmoe codebase.

- `send_data()` and `receive_data()` use a static 512-byte area to pass information
  - The current implementation was a quick hack to get menus working, since they use this functionality.
  - I tried to go back and code it to allocate memory dynamically, but it turned out to be more complicated than expected.
  - See `src/system/kernel/thread.cpp`.
  - Doing this dynamically turns out to probably be harder than it's worth, as this is not used very often, and when it is, very small amounts of data are passed.

- Tracker/libtracker/Deskbar bugs
  - "Get Info" on a file can sometimes hang Tracker (haven't seen this in a while)
  - Dragging a file/folder to a new window (e.g. to copy/move it to a new location) does not work
  - "Open With" always shows an empty menu
  - Tracker menus are layered behind the Deskbar on Wayland
  - Error on startup: "FlatIconImporter::_ParseSections() - error parsing shapes: Unknown error -1"
    - This is due to a malformed Person vector icon in Haiku
  - If you open an Open File Panel a second time after having opened a file the first time, it locks up the window (and the app)
    - Save panel doesn't do that though
	  - If you cancel, you can open as many Open panels as you want, it's only when you really open a file that it happens
  - Drag selecting often leaves a small amount of stale pixels behind from the selection rectangle
  - Resizing the columns on open/save panels produces graphical artifacts and/or shows through to the window below
  - Certain Deskbar placements that should put Deskbar in the corner of the screen position it away from the corner horizontally


## Historical Bugs -- not specifically fixed, but haven't been seen in a long time, so may be fixed by another change

- If you use an app for long enough it will crash with a corrupt `BMessage` header
  - Crash typically happens in `UsePreferredTarget`.
  - Seen on all platforms.
  - Since this is almost 100% unmodified Haiku code, I'm surprised they haven't seen this before.

- Sometimes views don't draw completely on the inital draw, and a refresh/resize will be needed to force a full paint

- Opening a menu can occasionally cause a crash, though haven't seen this in a while (Wayland)


## Platform-Specific Bugs

- When resizing windows, occasionally the window content will go transparent for an instant (Wayland)

- `ColumnListView` column resizing has slight redraw issues (Mac, Wayland HiDPI)
  - This is related to `CopyBits` trying to copy "half" a logical pixel in hidpi mode

- Very few `find_directory` entries work yet (Windows)
  - One side-effect of this is that Translators don't work on Windows yet because they can't be found.

- UNC paths are not recognized as full paths (Windows)

- File panel file listing background color is white on Linux and Windows, but gray on Mac

- Deskbar can't activate running apps (Windows)

- Deskbar doesn't support closing an app's window(s) (Mac or Windows)
  - The larger issue is that we don't yet have a way to deliver BMessages across apps on those platforms


## Cosmoe porting notes

Cosmoe is designed to be as compatible as possible with Haiku/Be code, but there some minor things to be aware of:

- `BIconUtils::GetAppIcon` doesn't exist on Haiku.

- `node_ref` cannot be used to find/create filesystem objects on Cosmoe.
  - A small number of member functions, e.g. `BDirectory(node_ref)`, are absent due to this, since there is no mechanism to make it work on non-Haiku platforms.

- `entry_ref` must always contain a full path in the name field on Cosmoe.
  - Many apps assume there will be just the leaf name in there, as would be expected on Haiku, so additional code is required if you need to extract just the leaf name.

- `B_CMAP8` is not a valid color space for drawing.

- `BRecentFilesList` / `BRecentFolderList` / `BRecentAppList` are per-app per-launch lists, not system-wide and remembered

- Cosmoe combines several libraries into libbe.so that are separate on Haiku.  If you use makefile-engine, this is handled for you.  If not, you need to remove these libraries from your link command:
  - shared translation network agg columnlistview media

- Application resources can only be added at compile time
  - xres is only used for manipulating standalone rsrc files, not applications

- BRoster does not return results unless a BApplication has initialized the connection to the backend
  - On Haiku, BRoster works without a BApplication

- Attributes are limited to 4K total on Linux under ext4, which can prevent large attributes (e.g. large mime icons) or a large number of attributes from working
  - Workaround: use XFS or Btrfs

- BFont.StringWidth() takes an extra optional parameter in Cosmoe to set scaling mode
  - If you were originally passing something besides an int32 into the second parameter, you'll need to cast to int32 to avoid an ambigious call between the 2 StringWidth methods.

- Apps that need runtime lookup of their own Class::Instantiate(BMessage*) symbols need the following linker flags in their Makefile. Apps using only the built-in archivable classes do not need it.
	ifeq ($(shell uname -s),Linux)
	LINKER_FLAGS += -Wl,--export-dynamic
	endif
	ifeq ($(shell uname -s),Darwin)
	LINKER_FLAGS += -Wl,-export_dynamic
	endif
	# Windows support for runtime lookup is uncertain at this point
	# May rely on --export-all-symbols and/or explicit exports via .def file or __declspec(dllexport)
	# meson-based builds should use "export_dynamic: true" in the executable definition, which handles all platforms


## Unit tests issues for drawing

- inverse_clipping:  busted
- benchmark: Cosmoe is slower than Haiku by 4x in Strings
- clip_to_picture: busted
- on the plus side, many tests, especially BPicture tests, actually work better on Cosmoe