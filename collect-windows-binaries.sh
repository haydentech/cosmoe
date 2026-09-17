#!/bin/bash

set -euo pipefail

# Resolve the repository root as the directory where this script lives.
COSMOE_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$COSMOE_DIR/builddir-windows"
WIN_TEST_DIR="$COSMOE_DIR/win-test"
CROSS_FILE="$COSMOE_DIR/cross-mxe.ini"

if [[ ! -d "$BUILD_DIR" ]]; then
	echo "Windows build directory not found: $BUILD_DIR" >&2
	exit 1
fi

if [[ ! -f "$CROSS_FILE" ]]; then
	echo "MXE cross file not found: $CROSS_FILE" >&2
	exit 1
fi

MXE_SYSROOT="$(sed -n "s/^[[:space:]]*sys_root[[:space:]]*=[[:space:]]*'\(.*\)'[[:space:]]*$/\1/p" "$CROSS_FILE")"
if [[ -z "$MXE_SYSROOT" || ! -d "$MXE_SYSROOT" ]]; then
	echo "Unable to find the MXE sysroot from $CROSS_FILE" >&2
	exit 1
fi

OBJDUMP="${MXE_OBJDUMP:-$MXE_SYSROOT/../bin/x86_64-w64-mingw32.shared-objdump}"
if [[ ! -x "$OBJDUMP" ]]; then
	echo "MXE objdump not found: $OBJDUMP" >&2
	exit 1
fi

# Start from a clean directory so stale files cannot conceal dependencies.
rm -rf "$WIN_TEST_DIR"
mkdir -p "$WIN_TEST_DIR"

while IFS= read -r -d '' file; do
	cp "$file" "$WIN_TEST_DIR/"
done < <(find "$BUILD_DIR" -type f \( -name '*.exe' -o -name '*.dll' \) \
	! -name 'libbe-bootstrap.dll' -print0)

shopt -s nullglob
queue=("$WIN_TEST_DIR"/*.exe "$WIN_TEST_DIR"/*.dll)
declare -A scanned=()
declare -A copied=()

while ((${#queue[@]})); do
	file="${queue[0]}"
	queue=("${queue[@]:1}")
	filename="$(basename "$file")"

	[[ -n "${scanned[$filename]:-}" ]] && continue
	scanned["$filename"]=1

	while IFS= read -r dll; do
		case "${dll^^}" in
			API-MS-WIN-*|KERNEL32.DLL|MSVCRT.DLL|NTDLL.DLL|USER32.DLL|GDI32.DLL|\
			ADVAPI32.DLL|SHELL32.DLL|OLE32.DLL|OLEAUT32.DLL|COMDLG32.DLL|\
			COMCTL32.DLL|DNSAPI.DLL|IPHLPAPI.DLL|WS2_32.DLL|WINMM.DLL|VERSION.DLL|SHLWAPI.DLL|\
			IMM32.DLL|MSIMG32.DLL|UCOMCTL32.DLL|DWMAPI.DLL|UXTHEME.DLL|DWRITE.DLL)
				continue
				;;
		esac

		if [[ -z "${copied[$dll]:-}" && ! -e "$WIN_TEST_DIR/$dll" ]]; then
			runtime_dll="$MXE_SYSROOT/bin/$dll"
			if [[ ! -f "$runtime_dll" ]]; then
				echo "Missing MXE runtime DLL: $dll" >&2
				echo "Expected: $runtime_dll" >&2
				exit 1
			fi
			cp "$runtime_dll" "$WIN_TEST_DIR/"
			copied["$dll"]=1
		fi

		if [[ -f "$WIN_TEST_DIR/$dll" ]]; then
			queue+=("$WIN_TEST_DIR/$dll")
		fi
	done < <("$OBJDUMP" -p "$file" | awk '/DLL Name:/ { print $3 }')
done

echo "Collected runnable Windows environment in $WIN_TEST_DIR"
