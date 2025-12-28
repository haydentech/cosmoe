# Building Cosmoe on Windows

This guide explains how to build the Cosmoe project on Windows.

## Required Software

### 1. Core Build Tools

#### Option A: MSYS2 (Recommended)
MSYS2 provides a Unix-like environment on Windows with modern package management.

**Download & Install:**
- Download from: https://www.msys2.org/
- Install to default location (C:\msys64)
- Launch "MSYS2 UCRT64" (not MSYS, MINGW32, or MINGW64)

**Install Build Toolchain:**
```bash
# Update package database
pacman -Syu

# Install core build tools
pacman -S mingw-w64-ucrt-x86_64-gcc \
          mingw-w64-ucrt-x86_64-meson \
          mingw-w64-ucrt-x86_64-ninja \
          mingw-w64-ucrt-x86_64-pkg-config \
          git
```

#### Option B: Visual Studio
- Visual Studio 2019 or later with C++ Desktop Development workload
- Install Python 3.8+ (for Meson)
- Install Ninja build system
- Install pkg-config

### 2. Required Libraries

All libraries can be installed via MSYS2 pacman:

```bash
# Core dependencies (required)
pacman -S mingw-w64-ucrt-x86_64-cairo \
          mingw-w64-ucrt-x86_64-libpng \
          mingw-w64-ucrt-x86_64-libjpeg-turbo \
          mingw-w64-ucrt-x86_64-libwebp \
          mingw-w64-ucrt-x86_64-pango \
          mingw-w64-ucrt-x86_64-fontconfig \
          mingw-w64-ucrt-x86_64-glib2 \
          mingw-w64-ucrt-x86_64-pixman \
          mingw-w64-ucrt-x86_64-icu

# Keyboard handling
pacman -S mingw-w64-ucrt-x86_64-libxkbcommon

# Optional: For running tests
pacman -S mingw-w64-ucrt-x86_64-cppunit
```

### 3. Windows-Specific Dependencies

The following are automatically provided by Windows SDK:
- `gdi32.lib` - Graphics Device Interface
- `user32.lib` - Window management
- `kernel32.lib` - Core Windows APIs
- `comdlg32.lib` - Common dialogs
- `psapi.lib` - Process status API

## Detailed Library Requirements

| Library | Version | Purpose | Windows Package |
|---------|---------|---------|-----------------|
| **Cairo** | Any | 2D graphics rendering | `mingw-w64-ucrt-x86_64-cairo` |
| **libpng** | Any | PNG image support | `mingw-w64-ucrt-x86_64-libpng` |
| **libjpeg** | Any | JPEG image support | `mingw-w64-ucrt-x86_64-libjpeg-turbo` |
| **libwebp** | Any | WebP image support | `mingw-w64-ucrt-x86_64-libwebp` |
| **Pango** | Any | Text rendering | `mingw-w64-ucrt-x86_64-pango` |
| **Fontconfig** | Any | Font configuration | `mingw-w64-ucrt-x86_64-fontconfig` |
| **GLib** | ≥ 2.36 | Utilities | `mingw-w64-ucrt-x86_64-glib2` |
| **Pixman** | ≥ 0.25.2 | Pixel manipulation | `mingw-w64-ucrt-x86_64-pixman` |
| **ICU** | ≥ 60.0 | Unicode/i18n | `mingw-w64-ucrt-x86_64-icu` |
| **xkbcommon** | ≥ 0.3.0 | Keyboard handling | `mingw-w64-ucrt-x86_64-libxkbcommon` |
| **cppunit** | Any | Unit tests (optional) | `mingw-w64-ucrt-x86_64-cppunit` |

## Building the Project

### Using MSYS2 (Recommended)

1. **Clone the repository:**
```bash
cd /c/Users/YourName/Projects
git clone https://github.com/yourusername/cosmoe.git
cd cosmoe
```

2. **Configure the build:**
```bash
meson setup builddir --buildtype=debug
```

Or for a release build:
```bash
meson setup builddir --buildtype=release
```

3. **Compile:**
```bash
meson compile -C builddir
```

Or using Ninja directly:
```bash
ninja -C builddir
```

4. **Install (optional):**
```bash
meson install -C builddir
```

### Using Visual Studio

1. **Set up environment:**
   - Open "x64 Native Tools Command Prompt for VS 2019"
   - Ensure Python, Meson, and Ninja are in PATH
   - Ensure pkg-config can find library .pc files

2. **Configure:**
```cmd
meson setup builddir --backend=vs2019
```

3. **Build:**
```cmd
meson compile -C builddir
```

Or open the generated Visual Studio solution in `builddir/`.

## Build Configuration Options

### Platform-Specific Settings

On Windows, Meson automatically:
- Detects Windows platform and builds **Win32 backend only**
- Links against Windows system libraries (gdi32, user32, kernel32)
- Uses Win32-specific source files (window.c, Win32Backend.cpp)

### Custom Options

```bash
# Disable unit tests
meson setup builddir -Denable_unit_test_compile=false

# Change installation prefix
meson setup builddir --prefix=C:/Cosmoe

# Set C++ standard (default is c++17)
meson setup builddir -Dcpp_std=c++20
```

## Compiler Compatibility

### Supported Compilers

✅ **GCC (MSYS2/MinGW-w64)** - Recommended, version 11.0+  
✅ **MSVC (Visual Studio 2019+)** - Supported with some limitations  
✅ **Clang (LLVM/Clang-cl)** - Should work but less tested  

### Compiler-Specific Notes

**GCC/MinGW:**
- Fully supported
- Best compatibility with POSIX code
- Includes semaphore wrapper for Windows

**MSVC:**
- May require adjustments for C99/C11 features
- Windows SDK provides semaphores natively
- Use `/std:c++17` or later

## Troubleshooting

### Common Issues

**1. "cairo-win32 not found"**
```bash
# Install Cairo with Win32 backend support
pacman -S mingw-w64-ucrt-x86_64-cairo
```

**2. "xkbcommon not found"**
```bash
# xkbcommon is available on Windows via MSYS2
pacman -S mingw-w64-ucrt-x86_64-libxkbcommon
```

**3. "Cannot find gdi32.lib"**
- Ensure you're using the UCRT64 environment (not MSYS or MINGW32)
- For MSVC, ensure Windows SDK is installed

**4. "semaphore.h: No such file or directory"**
- This is normal - the project includes a Windows semaphore wrapper
- Located in `src/system/kernel/semaphore.c` and `semaphore.h`
- Automatically included on Windows builds

**5. Link errors with pthread**
- Windows uses native threads, not pthread
- `thread-windows.c` provides Windows thread implementation
- No pthread library needed on Windows

### Library Path Issues

If pkg-config can't find libraries:

**MSYS2:**
```bash
export PKG_CONFIG_PATH="/ucrt64/lib/pkgconfig:$PKG_CONFIG_PATH"
```

**Visual Studio:**
```cmd
set PKG_CONFIG_PATH=C:\msys64\ucrt64\lib\pkgconfig
```

## Running the Applications

After building, executables are in `builddir/src/apps/`:

```bash
# Run from MSYS2 terminal
./builddir/src/apps/minimal/minimal.exe

# Or from Windows Explorer
explorer builddir\src\apps\minimal\minimal.exe
```

### Required DLLs

Applications need these DLLs (automatically found in MSYS2 environment):
- `libcairo-2.dll`
- `libpng16-16.dll`
- `libglib-2.0-0.dll`
- `libpango-1.0-0.dll`
- `libpangocairo-1.0-0.dll`
- `libfontconfig-1.dll`
- `libicu*.dll`
- MSYS2 runtime DLLs

To create standalone distribution, copy DLLs next to executables or use:
```bash
ldd builddir/src/apps/minimal/minimal.exe
```

## Windows-Specific Features

The Win32 backend provides:
- Native Windows windows using Win32 API
- Cairo rendering with GDI integration
- Windows clipboard support
- Mouse and keyboard input
- Window decorations (title bar, borders)
- Multi-monitor support (via GetSystemMetrics)

### Threading Support

The project uses two threading implementations:

1. **Windows Native Threads** (`thread-windows.c`):
   - BeOS thread API mapped to Windows CreateThread/WaitForSingleObject
   - Used for `spawn_thread()`, `resume_thread()`, etc.

2. **POSIX Threads via winpthreads**:
   - MSYS2/MinGW-w64 includes winpthreads library
   - Provides `pthread_mutex_*`, `pthread_cond_*`, `pthread_once`
   - Used throughout the codebase (Application.cpp, Window.cpp, etc.)
   - No code changes needed - works automatically with MinGW

3. **Windows Semaphores** (`semaphore.c`):
   - POSIX semaphore wrapper using Windows CreateSemaphore API
   - Provides `sem_init()`, `sem_wait()`, `sem_post()`, etc.

## Performance Notes

- **Debug builds** include symbols and assertions (slower but easier to debug)
- **Release builds** enable optimizations (-O2/-O3)
- Cairo with GDI backend is well-optimized on Windows
- Consider using `-Db_lto=true` for Link-Time Optimization

## Next Steps

After building:

1. **Run the test suite:**
```bash
meson test -C builddir
```

2. **Try the sample applications:**
```bash
builddir/src/apps/minimal/minimal.exe
builddir/src/apps/aboutsystem/AboutSystem.exe
```

3. **Install system-wide (optional):**
```bash
meson install -C builddir
```

## Additional Resources

- **MSYS2 Documentation**: https://www.msys2.org/docs/
- **Meson Build System**: https://mesonbuild.com/
- **Cairo Graphics**: https://www.cairographics.org/
- **Project README**: See main `README.md` for project overview

## Getting Help

If you encounter issues:

1. Check `builddir/meson-logs/meson-log.txt` for detailed error messages
2. Verify all dependencies are installed: `pkg-config --list-all`
3. Ensure you're in the correct MSYS2 environment (UCRT64)
4. Check compiler version: `gcc --version` or `cl.exe`
5. File an issue on the project's GitHub repository
