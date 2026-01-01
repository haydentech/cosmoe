# Building Cosmoe for Windows (via WSL Cross-Compilation)

This guide explains how to build Windows executables for Cosmoe using WSL (Windows Subsystem for Linux) with cross-compilation. This approach provides a clean Linux build environment while generating native Windows .exe files.

## Prerequisites

### 1. Windows Subsystem for Linux (WSL)

**Install WSL 2:**
```powershell
# In PowerShell (as Administrator)
wsl --install
```

This installs Ubuntu by default. Reboot if prompted.

### 2. MSYS2 (for Dependencies)

MSYS2 provides pre-built Windows libraries that we'll reuse during cross-compilation.

**Download & Install:**
- Download from: https://www.msys2.org/
- Install to default location (C:\msys64)
- Launch "MSYS2 UCRT64" terminal

**Install Required Libraries:**
```bash
# Update package database
pacman -Syu

# Install all required dependencies
pacman -S mingw-w64-ucrt-x86_64-cairo \
          mingw-w64-ucrt-x86_64-libpng \
          mingw-w64-ucrt-x86_64-libjpeg-turbo \
          mingw-w64-ucrt-x86_64-libwebp \
          mingw-w64-ucrt-x86_64-pango \
          mingw-w64-ucrt-x86_64-fontconfig \
          mingw-w64-ucrt-x86_64-glib2 \
          mingw-w64-ucrt-x86_64-icu \
          mingw-w64-ucrt-x86_64-libxkbcommon
```

### 3. WSL Build Tools

**In WSL terminal:**
```bash
# Update apt
sudo apt update

# Install cross-compilation toolchain
sudo apt install -y \
    mingw-w64 \
    g++-mingw-w64-x86-64 \
    meson \
    ninja-build \
    pkg-config \
    flex \
    bison \
    git

```

## Building the Project

### 1. Access the Project in WSL

The cosmoe directory on your Windows C: drive is accessible from WSL at `/mnt/c/`:

```bash
cd /mnt/c/git/cosmoe
```

### 2. Configure the Build

Set up pkg-config to find MSYS2 libraries and configure Meson with the cross-compilation file:

```bash
# Set environment variables for pkg-config
export PKG_CONFIG_PATH=/mnt/c/msys64/ucrt64/lib/pkgconfig
export PKG_CONFIG_SYSROOT_DIR=/mnt/c/msys64/ucrt64

# Configure with cross-compilation settings
meson setup --cross-file windows-cross.txt builddir-wsl
```

The `windows-cross.txt` file (included in the repository) tells Meson to use the mingw-w64 cross-compiler and points to MSYS2's libraries.

### 3. Build

```bash
ninja -C builddir-wsl
```

This compiles all source files and generates Windows .exe and .dll files in `builddir-wsl/`.

### 4. Build Options

**Debug build (default):**
```bash
meson setup --cross-file windows-cross.txt builddir-wsl
```

**Release build (optimized):**
```bash
meson setup --cross-file windows-cross.txt --buildtype=release builddir-wsl
```

**Clean rebuild:**
```bash
rm -rf builddir-wsl
meson setup --cross-file windows-cross.txt builddir-wsl
ninja -C builddir-wsl
```

**Continue building despite errors:**
```bash
ninja -C builddir-wsl -k 0
```

## Testing

The compiled Windows executables are in `builddir-wsl/`. You can run them directly from Windows:

```bash
# In Windows Explorer, navigate to:
C:\git\cosmoe\builddir-wsl\src\apps\clock\Clock.exe

# Or from PowerShell:
cd C:\git\cosmoe\builddir-wsl\src\apps\clock
.\Clock.exe
```

## Incremental Builds & Performance

### Problem: Ninja Always Rebuilds Everything

If you notice that `ninja` always rebuilds the entire project even when only a few files have changed, this is caused by timestamp inconsistencies when the source and build directories are on the Windows filesystem (`/mnt/c`). WSL's translation layer causes Ninja to think files have changed when they haven't.

### Solution: Move Build Directory to WSL Native Filesystem

Keep your source code on Windows (easy to edit with Windows tools), but place the build artifacts on WSL's native filesystem for fast, consistent incremental builds:

```bash
# In WSL - create a build directory in your WSL home
cd ~
mkdir cosmoe-build

# Configure Meson to build in the WSL-native directory
cd /mnt/c/git/cosmoe
export PKG_CONFIG_PATH=/mnt/c/msys64/ucrt64/lib/pkgconfig
export PKG_CONFIG_SYSROOT_DIR=/mnt/c/msys64/ucrt64
meson setup --cross-file windows-cross.txt ~/cosmoe-build

# Build from the WSL-native directory
cd ~/cosmoe-build
ninja
```

**Benefits:**
- ✅ Incremental builds work correctly (only changed files rebuild)
- ✅ Much faster compilation (native Linux filesystem I/O)
- ✅ Source code stays on Windows (edit with VS Code, Notepad++, etc.)
- ✅ Timestamps and file metadata are consistent

**After building, access the Windows executables:**

```bash
# The built executables are in your WSL home directory
ls ~/cosmoe-build/src/apps/

# Copy them to Windows for testing
cp ~/cosmoe-build/src/apps/minimal/minimal.exe /mnt/c/Users/YourUsername/Desktop/
```

**To clean up the old build directory:**

```bash
# Remove the old Windows-filesystem build directory
rm -rf /mnt/c/git/cosmoe/builddir-wsl
```

This approach gives you the best of both worlds: Windows-based source editing with fast, reliable WSL builds.

## Advantages of WSL Cross-Compilation

1. **Case-sensitive filesystem**: Avoids conflicts between `locale.h` (system) and `headers/os/locale/locale.h` (BeOS)
2. **Clean build environment**: Linux tools work natively without Windows compatibility issues
3. **Better dependency handling**: pkg-config works reliably with MSYS2 packages
4. **Faster compilation**: Native Linux file I/O is faster than MSYS2
5. **Standard toolchain**: Uses standard mingw-w64 cross-compiler

## Troubleshooting

### Common Issues

**1. "Package 'pango' not found"**
```bash
# Ensure PKG_CONFIG_PATH is set before running meson
export PKG_CONFIG_PATH=/mnt/c/msys64/ucrt64/lib/pkgconfig
export PKG_CONFIG_SYSROOT_DIR=/mnt/c/msys64/ucrt64
```

**2. "Program 'flex' not found"**
```bash
# Install flex and bison in WSL
sudo apt install flex bison
```

**3. "An exe_wrapper is needed"**
- This error should not occur if `needs_exe_wrapper = false` is set in `windows-cross.txt`
- If it persists, verify the cross-file contains this setting

**4. Case-sensitivity warnings**
- If you see warnings about case mismatches, they're informational
- WSL's case-sensitive filesystem handles `locale.h` vs `Locale.h` correctly

**5. "Cannot find library -lcairo"**
```bash
# Verify MSYS2 packages are installed and accessible
ls /mnt/c/msys64/ucrt64/lib/libcairo.a
# Should show the file

# Check pkg-config can find it
PKG_CONFIG_PATH=/mnt/c/msys64/ucrt64/lib/pkgconfig pkg-config --libs cairo
```

**6. Slow compilation**
- Building on WSL with `/mnt/c/` paths can be slower than native filesystem
- For faster builds, copy the project to WSL's native filesystem:
  ```bash
  cp -r /mnt/c/git/cosmoe ~/cosmoe
  cd ~/cosmoe
  # Update windows-cross.txt to use /home/youruser/cosmoe paths
  ```

### Common Windows Porting Issues

When porting code to Windows, be aware of these common issues:

**`alloca()` header location:**
- **POSIX**: `#include <alloca.h>`
- **Windows**: `#include <malloc.h>`
- **Solution**: Use conditional includes:
  ```cpp
  #ifdef _WIN32
  #include <malloc.h>
  #else
  #include <alloca.h>
  #endif
  ```

**`mkdir()` function signature:**
- **POSIX**: `mkdir(const char* path, mode_t mode)` - two arguments
- **Windows**: `mkdir(const char* path)` - one argument (mode is ignored)
- **Solution**: Use conditional compilation:
  ```cpp
  #ifdef _WIN32
  mkdir(path);
  #else
  mkdir(path, 0777);
  #endif
  ```

**`strcasestr()` function:**
- **POSIX**: Available in `<strings.h>`
- **Windows**: Not available, must use custom implementation
- **Solution**: Include `string_helper.h` on Windows which provides this function

**Other common differences:**
- `lstat()` → use `stat()` on Windows (no symbolic link distinction)
- `O_NOFOLLOW`, `O_CLOEXEC` flags → not available on Windows, define as 0
- `S_ISLNK()` macro → not available on Windows, define as returning false
- `gmtime_r()`, `localtime_r()` → use `gmtime_s()`, `localtime_s()` wrappers
- Time structures: POSIX uses `st_atim` (timespec), Windows uses `st_atime` (time_t)

### Verifying the Setup

Test that all tools and dependencies are available:

```bash
# Check cross-compiler
x86_64-w64-mingw32-gcc --version

# Check build tools
meson --version
ninja --version

# Check pkg-config can find libraries
export PKG_CONFIG_PATH=/mnt/c/msys64/ucrt64/lib/pkgconfig
pkg-config --modversion cairo pango libpng icu-i18n
```

All commands should succeed without errors.

## Running the Applications

After building, executables are in `builddir-wsl/src/apps/` and can be run directly from Windows:

**From Windows Explorer:**
- Navigate to `C:\git\cosmoe\builddir-wsl\src\apps\`
- Double-click any `.exe` file (e.g., `Clock.exe`, `minimal.exe`)

**From PowerShell:**
```powershell
cd C:\git\cosmoe\builddir-wsl\src\apps\clock
.\Clock.exe
```

### Required DLLs

The executables need these DLLs at runtime (from MSYS2):
- `libcairo-2.dll`
- `libpng16-16.dll`
- `libglib-2.0-0.dll`
- `libpango-1.0-0.dll`
- `libpangocairo-1.0-0.dll`
- `libfontconfig-1.dll`
- `libicuuc*.dll`, `libicuin*.dll`
- Various dependency DLLs (libfreetype, libharfbuzz, etc.)


**Copy DLLs to application directory:**
```bash
# From WSL, copy MSYS2 DLLs next to your executable
cp /mnt/c/msys64/ucrt64/bin/*.dll /mnt/c/git/cosmoe/builddir-wsl/src/apps/minimal/
```

Or use `ldd` equivalent to find required DLLs:
```bash
# In WSL with mingw-w64 tools
x86_64-w64-mingw32-objdump -p builddir-wsl/src/apps/minimal/minimal.exe | grep "DLL Name"
```

## Advanced Topics

### Performance Notes

- **Debug builds** include symbols and assertions (slower but easier to debug)
- **Release builds** enable optimizations (-O2/-O3)
- **WSL I/O**: Building on `/mnt/c/` is slower than WSL's native filesystem
  - For fastest builds, copy project to `~/cosmoe` in WSL

### Creating Standalone Distributions

To package applications for distribution:

1. **Collect all DLLs:**
   ```bash
   # Create a distribution directory
   mkdir -p /mnt/c/git/cosmoe-dist
   cp builddir-wsl/src/apps/clock/Clock.exe /mnt/c/git/cosmoe-dist/
   
   # Copy required MSYS2 DLLs
   cp /mnt/c/msys64/ucrt64/bin/libcairo-2.dll /mnt/c/git/cosmoe-dist/
   cp /mnt/c/msys64/ucrt64/bin/libglib-2.0-0.dll /mnt/c/git/cosmoe-dist/
   # ... copy all required DLLs
   ```

2. **Test on a clean Windows machine** without MSYS2 installed

### Build Troubleshooting Commands

```bash
# See all registered build targets
ninja -C builddir-wsl -t targets | head -20

# Rebuild just one target
ninja -C builddir-wsl src/kits/libbe.a

# Clean and rebuild
ninja -C builddir-wsl clean
ninja -C builddir-wsl

# Verbose build (see full compile commands)
ninja -C builddir-wsl -v
```

## Architecture

### Windows Backend Components

- `src/system/windows/`: Win32 backend implementation
  - `window.c`: Native window creation and management
  - `Win32Backend.cpp`: Backend interface
- `src/system/kernel/`: Platform abstraction layer
  - `thread-windows.c`: BeOS thread API → Windows threads
  - `semaphore.c`: POSIX semaphore wrapper for Windows
  
### Threading Model

The project uses multiple threading APIs:
1. **BeOS Thread API**: Mapped to Windows CreateThread
2. **POSIX Threads**: Via MinGW-w64's winpthreads library  
3. **Windows Semaphores**: Custom POSIX wrapper

### Graphics Stack

```
Application Code (BView, BWindow)
         ↓
    Interface Kit (BView drawing)
         ↓
    Cairo (2D rendering)
         ↓
   Win32 GDI (Windows display)
```

## Contributing

When building on Windows for development:
1. Use debug builds during development
2. Test both x86 and x64 builds if targeting 32-bit Windows
3. Run the test suite before committing changes
4. Document any Windows-specific workarounds in code comments

## References

- [Meson Build System](https://mesonbuild.com/)
- [MSYS2](https://www.msys2.org/)
- [MinGW-w64](https://www.mingw-w64.org/)
- [Cairo Graphics](https://www.cairographics.org/)
- [WSL Documentation](https://docs.microsoft.com/en-us/windows/wsl/)

---

**Last Updated:** December 2025
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
