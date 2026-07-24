#!/bin/sh

set -eu

mimeset_path="$1"
build_root="$2"
install_dir="$3"
mime_db_dir="$4"
app_name="$5"

destdir="${DESTDIR:-}"
dest_path="${destdir}${install_dir}/${app_name}"
mime_db_path="${destdir}${mime_db_dir}"
tmpdir=

cleanup() {
    if [ -n "$tmpdir" ] && [ -d "$tmpdir" ]; then
        rm -rf "$tmpdir"
    fi
}

trap cleanup EXIT INT TERM

if [ ! -e "$dest_path" ]; then
    echo "install_with_mimeset: skipping $app_name, target not found: $dest_path" >&2
    exit 0
fi

mime_db_parent="$(dirname "$mime_db_path")"
if [ ! -d "$mime_db_path" ]; then
    mkdir -p "$mime_db_path" 2>/dev/null || true
fi

if [ ! -d "$mime_db_path" ] || [ ! -w "$mime_db_path" ]; then
    if [ ! -w "$mime_db_parent" ]; then
        echo "install_with_mimeset: skipping system MIME registration for $app_name (no write access to $mime_db_path)" >&2
        exit 0
    fi
fi

case "$(uname -s)" in
    Darwin)
        DYLD_LIBRARY_PATH="${build_root}${DYLD_LIBRARY_PATH:+:${DYLD_LIBRARY_PATH}}" \
            "$mimeset_path" -f "$dest_path"
        ;;
    *)
        LD_LIBRARY_PATH="${build_root}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
            "$mimeset_path" -f "$dest_path"
        ;;
esac

tmpdir="$(mktemp -d "${TMPDIR:-/tmp}/cosmoe-mimeset.XXXXXX")"

case "$(uname -s)" in
    Darwin)
        DYLD_LIBRARY_PATH="${build_root}${DYLD_LIBRARY_PATH:+:${DYLD_LIBRARY_PATH}}" \
            "$mimeset_path" -a --mimedb "$tmpdir" "$dest_path"
        ;;
    *)
        LD_LIBRARY_PATH="${build_root}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" \
            "$mimeset_path" -a --mimedb "$tmpdir" "$dest_path"
        ;;
esac

mkdir -p "$mime_db_path"
cp -a "$tmpdir/." "$mime_db_path/"
find "$mime_db_path" -type d -exec chmod 755 {} \;
find "$mime_db_path" -type f -exec chmod 644 {} \;
