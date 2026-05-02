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

- Some Be apps set their initial window size to `0,0,0,0` and it gets auto-resized
  - We now have a mechanism to support this in the backend message handler for `AS_WINDOW_CREATE`, but we can't use it yet because it hangs the redraw semaphore.
  - This should be tested on Terminal and Pairs since their original code does this trick.
  - This will also be how we set Wayland window `fFrame` to `(0,0)`.

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

- `BRecentFilesList` / `BRecentFolderList` / `BRecentAppList` only partially implemented

- Drag-n-Drop from external sources is not yet implemented on any backend

- Toolbar menus hang the window when opened

- `BView::RotateBy()` creates clipping issues
  - This currently only affects rotated tab labels as `BTab` is the only known code to use this functionality.

- `B_NOT_RESIZABLE` flag incorrectly limits programmatic resizing of the window
  - It should only prevent user resizing.

- Many APIs are just empty stubs or absent altogether
  - `get_mouse()`
  - `BWindowStack`
  - Anything to do with printing
  - `BPicture`, and `BView` methods dealing with `BPicture`
  - `BFont::GetGlyphShapes`
  - `BDirectWindow`
  - Media Kit audio writing functions and all video functions
  - Game Kit advanced sound playback and streaming functionality

- Deskbar shows running apps as of when it started, but does not update the list
  - Only X11 is supported at this time.
  - Other platforms show no apps at all yet.

- `CopyBits` doesn't correctly invalidate the bits left behind after a copy
  - For example, if you `CopyBits` a rect 5 pixels to the left, the right-most 5 pixels of the original rect will need to be invalidated so the view can redraw that content.
  - We are attempting to do this, but the math appears to be off, leading to stale pixels left on screen.
  - We have to be very careful to not invalidate even 1 pixel too much, however, as that leads to redraw-loops.
  - Most noticeable when scrolling back in the terminal.

- `B_OP_COPY` drawing should not copy transparent pixels, but it does
  - This causes `B_OP_COPY` bitmap drawing to preserve transparency.
  - There is also a larger issue that transparency sticks on the window surface.
  - Future draws to the surface will simply darken that area instead of overwriting it as they should.

- `BChannelSlider` can cause hangs when the slider is moved and the tooltip shows

- Programmatically moving a window changes its frame top-left on Wayland
  - On Wayland, the window frame should have its top-left anchored at `0,0`.

- `entry_ref` only works if `Name` holds an absolute path or dot-relative path
  - Converting from a `BEntry` or `GetNextRef` fills this out correctly.
  - Considering how extensively `entry_ref` is used, it's certain this is causing issues somewhere.
  - I don't know of any remaining problems in the Cosmoe codebase though.

- `send_data()` and `receive_data()` use a static 512-byte area to pass information
  - The current implementation was a quick hack to get menus working, since they use this functionality.
  - I've never gone back and coded it to allocate memory dynamically.
  - See `src/system/kernel/thread.cpp`.
  - Doing this dynamically turns out to probably be harder than it's worth, as this is not used very often, and when it is, very small amounts of data are passed.

- Unit tests aren't even close to 100% passing
  - Cosmoe is synced with Haiku unit tests.
  - Unfortunately, they use a custom unit test library that is a pain to work with, since determining exactly where a test failed is very time-consuming.
  - I know this will be very valuable in making Cosmoe more stable and compliant though.

## Platform-Specific Bugs

- Opening a menu can occasionally cause a crash (Wayland)

- Opening a sub-menu off of a menu causes a crash (Mutter/HyprLand only)

- `ColumnListView` column resizing has slight redraw issues (Mac, Wayland HiDPI)
  - This is likely related to the `CopyBits` issue mentioned above.

- Very few `find_directory` entries work yet (Windows)
  - One side-effect of this is that Translators don't work on Windows yet because they can't be found.

## Portability Caveats

Cosmoe is designed to be as compatible as possible with Haiku/Be code, but there are a few concerns to be aware of:

- `image_id` is a pointer type on Cosmoe, not an integer like on Haiku.
  - Accordingly, a bad `image_id` on Haiku would be -1, while a bad `image_id` on Cosmoe would be NULL. 

- `BIconUtils::GetAppIcon` doesn't exist on Haiku.

- `node_ref` cannot be used to find/create filesystem objects on Cosmoe.
  - A small number of member functions, e.g. `BDirectory(node_ref)`, are absent due to this, since there is no mechanism to make it work on non-Haiku platforms.

- `entry_ref` must always contain a full path in the name field on Cosmoe.
  - Many apps assume there will be just the leaf name in there, as would be expected on Haiku, so additional code is required if you need to extract just the leaf name.

- `B_CMAP8` is not a valid color space for drawing.

- `BRecentFilesList` / `BRecentFolderList` / `BRecentAppList` are per-app per-launch lists, not system-wide and remembered