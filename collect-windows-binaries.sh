#!/bin/bash

set -euo pipefail

# Collect the runtime environment for a Windows cross build: every .exe/.dll
# produced by the build, plus the third-party DLLs they import.
#
# Usage: collect-windows-binaries.sh [BUILD_DIR] [CROSS_FILE] [OUTPUT_DIR]
#
# Both layouts are supported: MXE (cross-mxe.ini, whose sys_root points at the
# MXE target tree) and a distribution-provided MinGW-w64 such as Fedora's
# mingw64-* packages (cross-mingw.ini, whose pkg_config_libdir sits inside the
# sysroot). MINGW_SYSROOT and MINGW_OBJDUMP override the detected paths.

# Resolve the repository root as the directory where this script lives.
COSMOE_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${1:-$COSMOE_DIR/builddir-windows}"
CROSS_FILE="${2:-$COSMOE_DIR/cross-mxe.ini}"
WIN_TEST_DIR="${3:-$COSMOE_DIR/win-test}"

if [[ ! -d "$BUILD_DIR" ]]; then
	echo "Windows build directory not found: $BUILD_DIR" >&2
	exit 1
fi

if [[ ! -f "$CROSS_FILE" ]]; then
	echo "Cross file not found: $CROSS_FILE" >&2
	exit 1
fi

# Read the first value of KEY from the cross file, which stores one value per line.
cross_value() {
	sed -n "s/^[[:space:]]*$2[[:space:]]*=[[:space:]]*'\\(.*\\)'[[:space:]]*\$/\\1/p" "$1" | head -n 1
}

CROSS_CC="$(cross_value "$CROSS_FILE" c)"
CROSS_SYS_ROOT="$(cross_value "$CROSS_FILE" sys_root)"
CROSS_PKG_CONFIG_LIBDIR="$(cross_value "$CROSS_FILE" pkg_config_libdir)"

# The sysroot holds the third-party runtime DLLs in its bin/ directory.
SYSROOT="${MINGW_SYSROOT:-}"
if [[ -z "$SYSROOT" && -n "$CROSS_SYS_ROOT" ]]; then
	# MXE names the whole target tree as the sysroot.
	SYSROOT="$CROSS_SYS_ROOT"
fi
if [[ -z "$SYSROOT" && "$CROSS_PKG_CONFIG_LIBDIR" == */lib/pkgconfig ]]; then
	# Distribution toolchains put pkg-config files at <sysroot>/lib/pkgconfig.
	SYSROOT="${CROSS_PKG_CONFIG_LIBDIR%/lib/pkgconfig}"
fi
if [[ -z "$SYSROOT" && -n "$CROSS_CC" && -x "$CROSS_CC" ]]; then
	SYSROOT="$("$CROSS_CC" -print-sysroot)"
fi
if [[ -z "$SYSROOT" || ! -d "$SYSROOT" ]]; then
	echo "Unable to determine the MinGW sysroot from $CROSS_FILE" >&2
	echo "Set MINGW_SYSROOT to the directory containing bin/*.dll." >&2
	exit 1
fi

# The objdump sits next to the cross compiler, named for the same target triplet.
OBJDUMP="${MINGW_OBJDUMP:-${MXE_OBJDUMP:-}}"
if [[ -z "$OBJDUMP" && -n "$CROSS_CC" ]]; then
	OBJDUMP="${CROSS_CC%gcc}objdump"
fi
if [[ ! -x "$OBJDUMP" ]] && command -v x86_64-w64-mingw32-objdump >/dev/null 2>&1; then
	OBJDUMP="$(command -v x86_64-w64-mingw32-objdump)"
fi
if [[ ! -x "$OBJDUMP" ]]; then
	echo "MinGW objdump not found: ${OBJDUMP:-<none>}" >&2
	echo "Set MINGW_OBJDUMP to the objdump for this cross toolchain." >&2
	exit 1
fi

# Start from a clean directory so stale files cannot conceal dependencies.
rm -rf "$WIN_TEST_DIR"
mkdir -p "$WIN_TEST_DIR"

while IFS= read -r -d '' file; do
	cp "$file" "$WIN_TEST_DIR/"
done < <(find "$BUILD_DIR" -type f \( -name '*.exe' -o -name '*.dll' \) -print0)

shopt -s nullglob
queue=("$WIN_TEST_DIR"/*.exe "$WIN_TEST_DIR"/*.dll)
declare -A scanned=()
declare -A copied=()

while ((${#queue[@]})); do
	file="${queue[0]}"
	queue=("${queue[@]:1}")
	filename="$(basename "$file")"

	[[ -n "${scanned[$filename]:-}" ]] && continue
	scanned[$filename]=1

	while IFS= read -r dll; do
		case "${dll^^}" in
			API-MS-WIN-*|KERNEL32.DLL|MSVCRT.DLL|NTDLL.DLL|USER32.DLL|GDI32.DLL|\
			ADVAPI32.DLL|SHELL32.DLL|OLE32.DLL|OLEAUT32.DLL|COMDLG32.DLL|\
			COMCTL32.DLL|DNSAPI.DLL|IPHLPAPI.DLL|WS2_32.DLL|WINMM.DLL|VERSION.DLL|SHLWAPI.DLL|\
			IMM32.DLL|MSIMG32.DLL|UCOMCTL32.DLL|DWMAPI.DLL|UXTHEME.DLL|DWRITE.DLL|\
			BCRYPT.DLL|CRYPT32.DLL|NCRYPT.DLL|SECUR32.DLL|WLDAP32.DLL|WININET.DLL|WINHTTP.DLL|\
			USERENV.DLL|PSAPI.DLL|SETUPAPI.DLL)
				continue
				;;
		esac

		if [[ -z "${copied[$dll]:-}" && ! -e "$WIN_TEST_DIR/$dll" ]]; then
			runtime_dll="$SYSROOT/bin/$dll"
			if [[ ! -f "$runtime_dll" ]]; then
				echo "Missing runtime DLL: $dll" >&2
				echo "Expected: $runtime_dll" >&2
				exit 1
			fi
			cp "$runtime_dll" "$WIN_TEST_DIR/"
			copied[$dll]=1
		fi

		if [[ -f "$WIN_TEST_DIR/$dll" ]]; then
			queue+=("$WIN_TEST_DIR/$dll")
		fi
	done < <("$OBJDUMP" -p "$file" | awk '/DLL Name:/ { print $3 }')
done

echo "Collected runnable Windows environment in $WIN_TEST_DIR"
