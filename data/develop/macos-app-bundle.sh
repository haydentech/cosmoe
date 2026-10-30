#!/bin/sh
#
# macos-app-bundle.sh
#
# Turns a freshly installed executable into a self-contained .app bundle: all
# non-Apple dylibs it depends on are copied into Contents/Frameworks and the
# load commands are rewritten by macos-bundle.sh (the same script the
# makefile-engine uses for user applications), then the icon and Info.plist are
# written and the whole bundle is signed.
#
# usage: macos-app-bundle.sh APP_NAME PREFIX ICNS_PATH [VERSION]
#
# APP_NAME is the installed executable's name in PREFIX/bin and becomes the
# bundle name. ICNS_PATH may be empty when the app ships no icon.
#
# Honors DESTDIR so it can run from `meson install` against a staging tree.
#
# Environment:
#   COSMOE_BUNDLE_SCRIPT       path to macos-bundle.sh (default: next to this file)
#   COSMOE_BUNDLE_LIBRARY_PATHS
#                              search path for non-Apple dependencies
#   COSMOE_CODESIGN_IDENTITY   signing identity (default: "-", i.e. ad-hoc)
#   COSMOE_CODESIGN_OPTIONS    extra codesign flags, e.g. "--options runtime --timestamp"

set -eu

if [ "$#" -lt 3 ] || [ "$#" -gt 4 ]; then
	echo "usage: $0 APP_NAME PREFIX ICNS_PATH [VERSION]" >&2
	exit 2
fi

app_name=$1
prefix=$2
icns_path=$3
version=${4:-1.0}

script_dir=$(cd "$(dirname "$0")" && pwd)
bundle_script=${COSMOE_BUNDLE_SCRIPT:-$script_dir/macos-bundle.sh}
codesign_identity=${COSMOE_CODESIGN_IDENTITY:--}
codesign_options=${COSMOE_CODESIGN_OPTIONS:-}
destdir=${DESTDIR:-}

staging_path=$destdir$prefix
executable_path=$staging_path/bin/$app_name
bundle_path=$staging_path/Applications/$app_name.app
contents_path=$bundle_path/Contents
macos_path=$contents_path/MacOS
frameworks_path=$contents_path/Frameworks
resources_path=$contents_path/Resources

if [ ! -f "$bundle_script" ]; then
	echo "Cannot bundle $app_name: $bundle_script not found" >&2
	exit 1
fi

if [ ! -f "$executable_path" ]; then
	echo "Cannot bundle $app_name: $executable_path not found" >&2
	exit 1
fi

# Cosmoe's own libraries come from the (possibly staged) install prefix, the
# rest from Homebrew.
COSMOE_BUNDLE_LIBRARY_PATHS=${COSMOE_BUNDLE_LIBRARY_PATHS:-$prefix/lib:/opt/homebrew/lib:/usr/local/lib}
COSMOE_BUNDLE_DESTDIR=$destdir
export COSMOE_BUNDLE_LIBRARY_PATHS COSMOE_BUNDLE_DESTDIR

sh "$bundle_script" "$bundle_path" "$executable_path"

# The executable lives inside the bundle from here on; drop the bare copy so it
# is not installed twice.
rm -f "$executable_path"

mkdir -p "$resources_path"

if [ -n "$icns_path" ] && [ -f "$icns_path" ]; then
	cp "$icns_path" "$resources_path/$app_name.icns"
fi

plist_path=$contents_path/Info.plist
{
	printf '%s\n' '<?xml version="1.0" encoding="UTF-8"?>'
	printf '%s\n' '<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">'
	printf '%s\n' '<plist version="1.0">' '<dict>'
	printf '%s\n' '    <key>CFBundleExecutable</key>' "    <string>$app_name</string>"
	printf '%s\n' '    <key>CFBundleIdentifier</key>' "    <string>org.cosmoe.$app_name</string>"
	printf '%s\n' '    <key>CFBundleName</key>' "    <string>$app_name</string>"
	printf '%s\n' '    <key>CFBundlePackageType</key>' '    <string>APPL</string>'
	if [ -f "$resources_path/$app_name.icns" ]; then
		printf '%s\n' '    <key>CFBundleIconFile</key>' "    <string>$app_name.icns</string>"
	fi
	printf '%s\n' '    <key>CFBundleShortVersionString</key>' "    <string>$version</string>"
	printf '%s\n' '    <key>CFBundleVersion</key>' "    <string>$version</string>"
	printf '%s\n' '    <key>NSHighResolutionCapable</key>' '    <true/>' '</dict>' '</plist>'
} > "$plist_path"

# Signing happens after every Mach-O inside the bundle has been modified: on
# Apple silicon an executable or dylib with an invalidated signature refuses to
# run. Bundled dylibs are sealed by the bundle signature, so sign them first.
if [ -d "$frameworks_path" ]; then
	find "$frameworks_path" -type f -exec \
		codesign --force --sign "$codesign_identity" $codesign_options {} \;
fi
codesign --force --sign "$codesign_identity" $codesign_options "$bundle_path"

bundled_count=$(find "$frameworks_path" -type f 2>/dev/null | wc -l | tr -d ' ')
echo "Created macOS bundle: $bundle_path ($bundled_count bundled libraries)"
