#!/bin/sh

set -eu

source_dir="$1"
rc_path="$2"
mime_db_builder="$3"
build_root="$4"

run_be_tool() {
    case "$(uname -s)" in
        Darwin)
            DYLD_LIBRARY_PATH="${build_root}${DYLD_LIBRARY_PATH:+:${DYLD_LIBRARY_PATH}}" "$@"
            ;;
        *)
            LD_LIBRARY_PATH="${build_root}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}" "$@"
            ;;
    esac
}

destdir="${DESTDIR:-}"
mime_db_root="${destdir}/usr/local/etc/cosmoe/mime_db"
tmpdir="$(mktemp -d "${TMPDIR:-/tmp}/cosmoe-mime-db.XXXXXX")"
trap 'rm -rf "$tmpdir"' EXIT INT TERM

mkdir -p "$mime_db_root"

find "$source_dir" -type d | while IFS= read -r source_path; do
    rel_path=${source_path#"$source_dir"}
    mkdir -p "$mime_db_root$rel_path"
done

find "$source_dir" -type f | sort | while IFS= read -r source_path; do
    rel_path=${source_path#"$source_dir"/}
    build_args=
    if [ "${rel_path%.super}" != "$rel_path" ]; then
        node_rel_path=${rel_path%.super}
        compiled_path="$tmpdir/$node_rel_path.rsrc"
        dest_path="$mime_db_root/$node_rel_path"
        build_args="--directory"
    else
        node_rel_path=$rel_path
        compiled_path="$tmpdir/$node_rel_path.rsrc"
        dest_path="$mime_db_root/$node_rel_path"
    fi

    mkdir -p "$(dirname "$compiled_path")" "$(dirname "$dest_path")"
    run_be_tool "$rc_path" "$source_path" -o "$compiled_path" -I "$(dirname "$source_path")"
    if [ -n "$build_args" ]; then
        run_be_tool "$mime_db_builder" --directory "$compiled_path" "$dest_path"
    else
        run_be_tool "$mime_db_builder" "$compiled_path" "$dest_path"
    fi
done
