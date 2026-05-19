#!/bin/sh

set -eu

mimeset_path="$1"
build_root="$2"
install_dir="$3"
app_name="$4"

destdir="${DESTDIR:-}"
dest_path="${destdir}${install_dir}/${app_name}"

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
