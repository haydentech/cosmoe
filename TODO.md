# TODO

## Known Bugs on All Platforms

- Cairo complains on exit due to non-empty object hash (i.e. memory leak)
  - This assert is currently disabled.

- TextEdit control works but has issues
  - Selection changes sometimes don't show up until the next repaint.
  - When scrolled, selecting text does not always highlight the correct range.

- Window repaint is inefficient
  - After any `Invalidate()`, the entire window backing buffer is copied to the window.
  - It should be limited to the inval rect.
  - This is so fast on modern hardware that it's not even noticeable, but we need to fix it eventually.

- Modal alerts are not fully modal on Wayland
  - The alert stays frontmost, but input will still be processed in the parent window.

- `B_CMAP8` (and a few others) are not valid color spaces for drawing
  - This is due to lack of support in Cairo for these color spaces.
  - Should we auto-upgrade these color spaces to `B_RGB32` at `BBitmap` creation to avoid issues, or would this just cause new ones as apps try to insert 8-bit bitmap data?
  - You can still set or import a `BBitmap`'s bits with data from `B_CMAP8` or other unsupported-for-drawing color spaces.  It will get correctly converted to the `BBitmap`'s supported color space.

- If you use an app for long enough it will crash with a corrupt `BMessage` header
  - Crash typically happens in `UsePreferredTarget`.
  - Seen on all platforms.
  - Since this is almost 100% unmodified Haiku code, I'm surprised they haven't seen this before.
  - This used to be frequent, but I haven't seen this in a couple months

- `BRecentFilesList` / `BRecentFolderList` / `BRecentAppList` only partially implemented

- Drag-n-Drop from external sources is not yet implemented on any backend

- Toolbar menus hang the window when opened

- `BView::RotateBy()` creates clipping issues
  - This currently only affects rotated tab labels as `BTab` is the only known code to use this functionality.

- Several APIs are empty stubs or absent altogether
  - `get_mouse()`
  - `BWindowStack` is all stubs
  - Anything to do with printing
  - `BFont::GetGlyphShapes`, `BFont::LoadFont`, `BFont::UnloadFont` are stubs
  - `BDirectWindow` is not present (and likely will not be supported)
  - Media Kit audio writing functions and all video functions
  - Game Kit advanced sound playback and streaming functionality

- Deskbar can't activate running apps on Windows

- Deskbar doesn't support closing an app's window(s) on Mac or Windows
  - The larger issue is that we don't yet have a way to deliver BMessages across apps on those platforms

- Deskbar does not position correctly when using GNOME Mutter as the Wayland compositor
  - This will likely never be fixed as GNOME intentionally omits support for the zwlr_layer_shell_v1 protocol, so no third-party panels are possible.  Yet another reason to dislike GNOME!
  - Menus also work very inconsistently in Deskbar under Mutter, though the reason is less clear.  Other apps' menus work fine under Mutter.

- The first click in a dialog box sometimes gets ignored or not fully processed
  - e.g. The OK button will depress but will require a second click to invoke

- `B_OP_SELECT` drawing should not transfer transparent pixels, but it does
  - `B_OP_SELECT` bitmap drawing transfers transparency to the target surface.
  - This can lead to views revealing the view underneath, or in the case of Wayland, views and windows that shows all the way through the window itself.
  - Future draws to the surface simply darken that area instead of overwriting it as they should, since the transparency has been transferred.
  - I literally can't find an app that uses B_OP_SELECT, other than Showcase's drawing tests, so not a big deal

- `BChannelSlider` can cause occasional hangs when the slider is moved and the tooltip shows

- Sometimes views don't draw completely on the inital draw, and a refresh/resize will be needed to force a full paint
  - One odd case of this is ShowImage, where loading JPG images shows them immediately, but PNG images don't show until the window is resized

- Sometimes views will draw without erasing the background, causing drawing to overlay previous drawing, especially noticeable when the drawing is semi-transparent

- When a view's pen size is an even number, stroked drawing comes out blurry
  - This is partially a function of our conversion from Haiku to Cairo coordinates
  - We draw lines "on-center" by offseting by a half-pixel, but for an even pen size or scale factor, we end up drawing in-between pixels again
  - Haiku shifts the drawing up and left by another half-pixel to compensate in even-pen-width situations, so we could to the same, but I'm not sure that's best.  It's a special-case fix, not a general solution.

- Many window looks and feels are not reflected in the backend
  - If the app asks for a utility window, or a floating window, currently you get just get a regular window.
  - Only borderless windows are currently supported (ironically via B_BORDERED_WINDOW)

- Enhancement: optional native file open/save dialogs on Windows and Mac

- Icon-O-Matic draws its grid slightly offset (3 pixels?)
  - This likely means our DrawBitmap implementation has a small issue when scaling up

- The hack to make translators both shared libraries and launchable executables no longer works

- `entry_ref` only works if `Name` holds an absolute path or dot-relative path
  - Converting from a `BEntry` or `GetNextRef` fills this out correctly.
  - Considering how extensively `entry_ref` is used, it's certain this is causing issues somewhere.
  - That said, I don't know of any remaining problems in the Cosmoe codebase.

- `send_data()` and `receive_data()` use a static 512-byte area to pass information
  - The current implementation was a quick hack to get menus working, since they use this functionality.
  - I tried to go back and code it to allocate memory dynamically, but it turned out to be more complicated than expected.
  - See `src/system/kernel/thread.cpp`.
  - Doing this dynamically turns out to probably be harder than it's worth, as this is not used very often, and when it is, very small amounts of data are passed.

- Unit tests are not quite 100% passing yet
  - NodeInfo tests still have a failing test regarding tracker icons

- Tracker bugs
  - You can rename a file, but it doesn't take effect
  - "Get Info" on a file hangs tracker
  - Dragging a file/icon drags a large white rectangle with it
  - Dragging a file to a new location does not initiate a copy
  - "Open With" always shows an empty menu
  - Tracker menus are layered behind the Deskbar
  - Tracker cannot move the item to the trash
    - if you persist and ask Tracker to delete immediately, it crashes
  - There is no trash icon on the desktop
  - Hard drives do not show on the Desktop
    - if you request the Disks icon to be on the desktop, it shows up but shows no disks


## Platform-Specific Bugs

- Opening a menu can occasionally cause a crash (Wayland)

- `ColumnListView` column resizing has slight redraw issues (Mac, Wayland HiDPI)
  - This is likely related to the `CopyBits` issue mentioned above.

- Very few `find_directory` entries work yet (Windows)
  - One side-effect of this is that Translators don't work on Windows yet because they can't be found.

- Deskbar's window slowly expands horizontally until it reaches its maximum width (Wayland)

- UNC paths are not recognized as full paths (Windows)


## Cosmoe porting notes

Cosmoe is designed to be as compatible as possible with Haiku/Be code, but there some minor things to be aware of:

- `image_id` is a pointer type on Cosmoe, not an integer like on Haiku.
  - Accordingly, a bad `image_id` on Haiku is a negative number (typically -1), while a bad `image_id` on Cosmoe is NULL. 

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



  inverse_clipping:
  total disaster

    benchmark:
  Cosmoe is slower by 2x in RandomLines and 2.5x in Strings

  clip_to_picture:
  busted