#!/bin/sh

set -eu

source_dir="$1"
rc_path="$2"
mime_db_builder="$3"
build_root="$4"
install_root_arg="${5:-}"

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
# Runtime lookup resolves under B_SYSTEM_DATA_DIRECTORY/mime_db.
install_prefix="${MESON_INSTALL_DESTDIR_PREFIX:-${MESON_INSTALL_PREFIX:-/usr/local}}"
if [ -n "$install_root_arg" ]; then
    if [ -n "${MESON_INSTALL_PREFIX:-}" ] \
        && [ "${install_root_arg#${MESON_INSTALL_PREFIX}}" != "$install_root_arg" ]; then
        mime_db_root="$install_prefix${install_root_arg#${MESON_INSTALL_PREFIX}}"
    else
        mime_db_root="$install_root_arg"
    fi
else
    mime_db_root="$install_prefix/share/cosmoe/mime_db"
fi
tmpdir="$(mktemp -d "${TMPDIR:-/tmp}/cosmoe-mime-db.XXXXXX")"
trap 'rm -rf "$tmpdir"' EXIT INT TERM

mkdir -p "$mime_db_root"
chmod 755 "$mime_db_root"

find "$source_dir" -type d | while IFS= read -r source_path; do
    rel_path=${source_path#"$source_dir"}
    mkdir -p "$mime_db_root$rel_path"
    chmod 755 "$mime_db_root$rel_path"
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
        chmod 755 "$dest_path"
    else
        run_be_tool "$mime_db_builder" "$compiled_path" "$dest_path"
        chmod 644 "$dest_path"
    fi
done
