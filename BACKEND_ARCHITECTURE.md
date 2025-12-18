# Window Backend Architecture

The window backend system allows Cosmoe to support multiple windowing systems (Wayland, X11, etc.) at runtime.

## Architecture Overview

The backend system consists of three layers:

1. **Backend Interface** (`CosmoeBackend.h`) - Pure virtual C++ interface
2. **Backend Implementations** - Wayland and X11 specific code
3. **C API Wrapper** (`CosmoeBackendAPI.h`) - Stable C interface for use by BApplication/BWindow

```
┌─────────────────────────────────────┐
│   BApplication / BWindow / etc.     │
│   (Use C API)                       │
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│   CosmoeBackendAPI.cpp              │
│   (C wrapper - stable interface)    │
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│   CosmoeBackendFactory              │
│   (Runtime backend selection)       │
└──────────────┬──────────────────────┘
               │
      ┌────────┴────────┐
      │                 │
┌─────▼──────┐   ┌─────▼──────┐
│  Wayland   │   │    X11     │
│  Backend   │   │  Backend   │
│  Plugin    │   │  Plugin    │
└────────────┘   └────────────┘
```

## Files

### Headers
- `headers/private/interface/CosmoeBackend.h` - Backend interface definitions
- `headers/private/interface/CosmoeBackendAPI.h` - C API for end users

### Implementation
- `src/kits/interface/CosmoeBackendFactory.cpp` - Factory for loading backends
- `src/kits/interface/CosmoeBackendAPI.cpp` - C API wrapper implementation

### Backend Plugins
- `src/system/wayland/WaylandBackend.cpp` - Wraps existing Wayland code
- `src/system/X11/X11Backend.cpp` - Wraps existing X11 code

### Window Code wrapped by backends
- `src/system/wayland/clients/window.c` - Wayland implementation
- `headers/libs/wayland/window.h` - Wayland window API

- `src/system/X11/window.c` - X11 implementation
- `src/system/X11/window.h` - X11 window API

## Backend Selection

The backend is selected automatically at runtime based on environment:

1. Check `COSMOE_BACKEND` environment variable (`wayland` or `x11`)
2. Check for `WAYLAND_DISPLAY` → use Wayland
3. Check for `DISPLAY` → use X11
4. Check `XDG_SESSION_TYPE` → use specified type
5. Default to Wayland if nothing detected

### Manual Selection

Users can force a specific backend:
```bash
export COSMOE_BACKEND=x11
./myapp
```

Or programmatically:
```cpp
cosmoe_backend_set_preferred("wayland");
```

## Backends are Plugins

Each backend compiles to a separate shared library:

- `libcosmoe-wayland.so` - Wayland backend (links Wayland libs)
- `libcosmoe-x11.so` - X11 backend (links X11/Xlib/xcb libs)

The main `libbe.so` does NOT link against either windowing library.
Backends are dynamically loaded with at runtime.


## Debugging

When a Cosmoe app launches, CosmoeBackendFactory prints the backend selection
// Look for lines like:
// "CosmoeBackendFactory: Detected Wayland environment"
// "CosmoeBackendFactory: Successfully loaded Wayland backend"
// "Using Wayland backend"
```

Check current backend at runtime:
```cpp
const char* name = cosmoe_backend_get_current_name();
printf("Using backend: %s\n", name);
```
