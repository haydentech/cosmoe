#!/usr/bin/env sh
set -eu

if [ "$#" -lt 2 ]; then
    echo "usage: $0 <catkeys-root> <linkcatkeys-binary> [install-root]" >&2
    exit 2
fi

catkeys_root="$1"
linkcatkeys_bin="$2"
install_root_arg="${3:-}"

if [ ! -d "$catkeys_root" ]; then
    echo "catalog source directory not found: $catkeys_root" >&2
    exit 1
fi

if [ ! -x "$linkcatkeys_bin" ]; then
    echo "linkcatkeys binary not executable: $linkcatkeys_bin" >&2
    exit 1
fi

start_time="$(date +%s)"

# Runtime lookup for *.catalog resolves under B_SYSTEM_DATA_DIRECTORY/locale/catalogs.
# Meson provides DESTDIR-aware prefix in MESON_INSTALL_DESTDIR_PREFIX.
install_prefix="${MESON_INSTALL_DESTDIR_PREFIX:-${MESON_INSTALL_PREFIX:-/usr/local}}"
if [ -n "$install_root_arg" ]; then
    if [ -n "${MESON_INSTALL_PREFIX:-}" ] \
        && [ "${install_root_arg#${MESON_INSTALL_PREFIX}}" != "$install_root_arg" ]; then
        install_root="$install_prefix${install_root_arg#${MESON_INSTALL_PREFIX}}"
    else
        install_root="$install_root_arg"
    fi
else
    install_root="$install_prefix/share/cosmoe/locale/catalogs"
fi

mkdir -p "$install_root"

tmp_root="${TMPDIR:-/tmp}/cosmoe-catalog-install.$$"
trap 'rm -rf "$tmp_root"' EXIT HUP INT TERM
mkdir -p "$tmp_root"

count=0
up_to_date=0
skipped_no_signature=0
skipped_examples=""
list_file="$tmp_root/catkeys.list"
task_file="$tmp_root/tasks.nul"
status_file="$tmp_root/status.log"
no_alias_marker="__COSMOE_NO_ALIAS__"
find "$catkeys_root" -type f -name '*.catkeys' -print > "$list_file"
: > "$task_file"
: > "$status_file"

while IFS= read -r catkeys_file; do
    lang="$(basename "$catkeys_file" .catkeys)"

    # Extract app signature from the first line of the .catkeys file.
    # Accept both x-vnd.* and legacy x.vnd.* prefixes.
    signature="$(awk 'NR==1 { for (i = 1; i <= NF; i++) if ($i ~ /^x[-.]vnd\./) { print $i; exit } }' "$catkeys_file")"
    alias_signature=""

    # Normalize legacy OpenBeOS prefix to the modern Haiku form.
    case "$signature" in
        x.vnd.*)
            alias_signature="$signature"
            signature="x-vnd.${signature#x.vnd.}"
            ;;
    esac

    if [ -z "$signature" ]; then
        skipped_no_signature=$((skipped_no_signature + 1))
        if [ "$skipped_no_signature" -le 8 ]; then
            skipped_examples="$skipped_examples\n$catkeys_file"
        fi
        continue
    fi

    # Queue work as NUL-delimited fields to safely support xargs -0.
    # xargs may drop empty fields; emit a non-empty marker for "no alias"
    # so each task always has exactly 4 arguments.
    emit_alias="$alias_signature"
    if [ -z "$emit_alias" ]; then
        emit_alias="$no_alias_marker"
    fi
    printf '%s\0%s\0%s\0%s\0' \
        "$catkeys_file" "$lang" "$signature" "$emit_alias" >> "$task_file"
done < "$list_file"

job_count="${INSTALL_CATALOGS_JOBS:-}"
if [ -z "$job_count" ]; then
    if command -v nproc >/dev/null 2>&1; then
        job_count="$(nproc)"
    elif command -v getconf >/dev/null 2>&1; then
        job_count="$(getconf _NPROCESSORS_ONLN 2>/dev/null || true)"
    fi
fi
case "$job_count" in
    ''|*[!0-9]*|0)
        job_count=1
        ;;
esac

if [ -s "$task_file" ]; then
    xargs -0 -n 4 -P "$job_count" sh -c '
        set -eu
        install_root="$1"
        linkcatkeys_bin="$2"
        tmp_root="$3"
        status_file="$4"
        catkeys_file="$5"
        lang="$6"
        signature="$7"
        alias_signature="$8"

        if [ "$alias_signature" = "__COSMOE_NO_ALIAS__" ]; then
            alias_signature=""
        fi

        out_dir="$install_root/$signature"
        out_file="$out_dir/$lang.catalog"
        mkdir -p "$out_dir"

        alias_dir=""
        alias_file=""
        if [ -n "$alias_signature" ] && [ "$alias_signature" != "$signature" ]; then
            alias_dir="$install_root/$alias_signature"
            alias_file="$alias_dir/$lang.catalog"
        fi

        # Incremental fast-path: skip conversion if outputs are newer than sources.
        if [ -f "$out_file" ] && [ "$out_file" -nt "$catkeys_file" ] && [ "$out_file" -nt "$linkcatkeys_bin" ]; then
            if [ -z "$alias_file" ] || { [ -f "$alias_file" ] && [ "$alias_file" -nt "$catkeys_file" ] && [ "$alias_file" -nt "$linkcatkeys_bin" ]; }; then
                printf "U\n" >> "$status_file"
                exit 0
            fi
        fi

        # Use a per-process temp output to avoid partial files if conversion fails.
        tmp_out="$tmp_root/$lang.$$.catalog"
        "$linkcatkeys_bin" -s "$signature" -l "$lang" -o "$tmp_out" "$catkeys_file" >/dev/null
        install -m 0644 "$tmp_out" "$out_file"

        # Backward-compat path for legacy OpenBeOS signatures.
        if [ -n "$alias_file" ]; then
            mkdir -p "$alias_dir"
            install -m 0644 "$tmp_out" "$alias_file"
        fi

        rm -f "$tmp_out"
        printf "L\n" >> "$status_file"
    ' _ "$install_root" "$linkcatkeys_bin" "$tmp_root" "$status_file" < "$task_file"
fi

count="$(grep -c '^L$' "$status_file" || true)"
up_to_date="$(grep -c '^U$' "$status_file" || true)"

end_time="$(date +%s)"
run_time=$((end_time - start_time))
echo "Linked and installed $count catalogs into $install_root in $run_time seconds."
if [ "$up_to_date" -gt 0 ]; then
    echo "Skipped $up_to_date catalogs that were already up to date."
fi
if [ "$skipped_no_signature" -gt 0 ]; then
    echo "Skipped $skipped_no_signature .catkeys files without embedded signature." >&2
    printf '%b\n' "$skipped_examples" | sed '/^$/d' | sed 's/^/  - /' >&2
    if [ "$skipped_no_signature" -gt 8 ]; then
        echo "  - ..." >&2
    fi
fi
