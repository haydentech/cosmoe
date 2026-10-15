Name:           cosmoe
Version:        0.9.0
Release:        1%{?dist}
Summary:        Cross-platform C++ application framework based on BeOS
License:        MIT
URL:            https://cosmoe.org
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  gcc-c++
BuildRequires:  meson
BuildRequires:  ninja-build
BuildRequires:  pkgconf-pkg-config
BuildRequires:  flex
BuildRequires:  bison
BuildRequires:  cairo-devel
BuildRequires:  fontconfig-devel
BuildRequires:  freetype-devel
BuildRequires:  glib2-devel
BuildRequires:  libicns-devel
BuildRequires:  libicu-devel
BuildRequires:  libjpeg-turbo-devel
BuildRequires:  pango-devel
BuildRequires:  pixman-devel
BuildRequires:  libpng-devel
BuildRequires:  LibRaw-devel
BuildRequires:  libtiff-devel
BuildRequires:  wayland-devel
BuildRequires:  libwebp-devel
BuildRequires:  libX11-devel
BuildRequires:  libXcursor-devel
BuildRequires:  libXrandr-devel
BuildRequires:  libxkbcommon-devel
BuildRequires:  fuse3-devel
BuildRequires:  zlib-ng-compat-devel

Requires:       adwaita-icon-theme
Requires:       xcursor-themes

%description
Cosmoe is a C++ library for building rich applications with the BeOS API
across Linux, macOS, Windows, and Haiku. This package contains Cosmoe shared
libraries, runtime add-ons, and supporting data files.

%package devel
Summary:        Development files for the Cosmoe framework
Requires:       %{name}%{?_isa} = %{version}-%{release}

%description devel
This package contains the C++ headers, static libraries, resource compiler,
and command-line development tools required to build applications against
Cosmoe.

%package apps
Summary:        Applications bundled with the Cosmoe framework
Requires:       %{name}%{?_isa} = %{version}-%{release}

%description apps
This package contains the sample applications, desktop integrations, and
command-line tools bundled with the Cosmoe framework.

%package desktop
Summary:        Cosmoe Wayland desktop session
Requires:       %{name}%{?_isa} = %{version}-%{release}
Requires:       %{name}-apps%{?_isa} = %{version}-%{release}
Requires:       labwc
Requires:       mako
Requires:       dbus-daemon
Requires:       polkit-gnome
Requires:       xdg-desktop-portal
Requires:       xdg-desktop-portal-wlr

%description desktop
This package provides the Cosmoe Wayland desktop session, including Tracker,
Deskbar, and a display-manager session entry.

%prep
%autosetup

%build
%meson -Denable_unit_test_compile=false
%meson_build

%install
%meson_install

%files
%license License.md
%doc README.md
%{_bindir}/mimeset
%{_libdir}/addons/
%{_libdir}/libbe.so
%{_libdir}/libbnetapi.so*
%{_libdir}/libcosmoe-*.so
%{_libdir}/libgame.so
%{_libdir}/libtracker.so
%{_datadir}/cosmoe/
%{_datadir}/fonts/ttf/cosmoe/

%files devel
%exclude %{_sysconfdir}/macos-bundle.sh
%{_sysconfdir}/makefile-engine
%{_includedir}/
%{_libdir}/*.a
%{_bindir}/addattr
%{_bindir}/catattr
%{_bindir}/collectcatkeys
%{_bindir}/copyattr
%{_bindir}/dumpcatalog
%{_bindir}/finddir
%{_bindir}/findpaths
%{_bindir}/getarch
%{_bindir}/hey
%{_bindir}/linkcatkeys
%{_bindir}/listattr
%{_bindir}/listres
%{_bindir}/message
%{_bindir}/mkdepend
%{_bindir}/rc
%{_bindir}/roster
%{_bindir}/vicn2png
%{_bindir}/xres

%files apps
%exclude %{_bindir}/gui-torture
%exclude %{_bindir}/minimal
%exclude %{_bindir}/minimal-noui
%exclude /usr/lib/debug/usr/bin/gui-torture-*.debug
%exclude /usr/lib/debug/usr/bin/minimal-*.debug
%exclude /usr/lib/debug/usr/bin/minimal-noui-*.debug
%exclude %{_datadir}/applications/mimeinfo.cache
%exclude %{_datadir}/applications/x-vnd.Be-TSKB.desktop
%exclude %{_datadir}/applications/x-vnd.Cosmoe-Tracker.desktop
%exclude %{_datadir}/applications/x-vnd.Haiku-Deskbar-Preferences.desktop
%exclude %{_datadir}/applications/x-vnd.Haiku-TrackerPreferences.desktop
%exclude %{_datadir}/icons/hicolor/icon-theme.cache
%exclude %{_datadir}/icons/hicolor/*/apps/deskbar.png
%exclude %{_datadir}/icons/hicolor/*/apps/deskbarpreferences.png
%exclude %{_datadir}/icons/hicolor/*/apps/tracker.png
%exclude %{_datadir}/icons/hicolor/*/apps/trackerpreferences.png
%{_bindir}/AboutSystem
%{_bindir}/Appearance
%{_bindir}/Backgrounds
%{_bindir}/CharacterMap
%{_bindir}/Chart
%{_bindir}/Clock
%{_bindir}/Columns
%{_bindir}/DeskCalc
%{_bindir}/DiskUsage
%{_bindir}/FontDemo
%{_bindir}/Gradients
%{_bindir}/Icon-O-Matic
%{_bindir}/Keymap
%{_bindir}/Mandelbrot
%{_bindir}/Pairs
%{_bindir}/Pulse
%{_bindir}/ResEdit
%{_bindir}/setcontrollook
%{_bindir}/Showcase
%{_bindir}/ShowImage
%{_bindir}/StyledEdit
%{_bindir}/Sudoku
%{_bindir}/Terminal
%{_bindir}/Time
%{_datadir}/applications/*.desktop
%{_datadir}/icons/hicolor/*/apps/*.png

%files desktop
%{_bindir}/Deskbar
%{_bindir}/DeskbarPreferences
%{_bindir}/Tracker
%{_bindir}/TrackerPreferences
%{_bindir}/cosmoe-desktop.sh
%{_datadir}/applications/x-vnd.Be-TSKB.desktop
%{_datadir}/applications/x-vnd.Cosmoe-Tracker.desktop
%{_datadir}/applications/x-vnd.Haiku-Deskbar-Preferences.desktop
%{_datadir}/applications/x-vnd.Haiku-TrackerPreferences.desktop
%{_datadir}/icons/hicolor/*/apps/deskbar.png
%{_datadir}/icons/hicolor/*/apps/deskbarpreferences.png
%{_datadir}/icons/hicolor/*/apps/tracker.png
%{_datadir}/icons/hicolor/*/apps/trackerpreferences.png
%{_datadir}/wayland-sessions/cosmoe.desktop

%changelog
* Mon Oct 12 2026 Bill Hayden <bill@haydentech.com> - 0.9.0-1
- Initial Fedora RPM packaging.