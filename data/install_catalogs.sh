#!/usr/bin/env sh
set -eu

if [ "$#" -lt 2 ]; then
    echo "usage: $0 <catkeys-root> <linkcatkeys-binary>" >&2
    exit 2
fi

catkeys_root="$1"
linkcatkeys_bin="$2"

if [ ! -d "$catkeys_root" ]; then
    echo "catalog source directory not found: $catkeys_root" >&2
    exit 1
fi

if [ ! -x "$linkcatkeys_bin" ]; then
    echo "linkcatkeys binary not executable: $linkcatkeys_bin" >&2
    exit 1
fi

# Runtime lookup for *.catalog currently resolves under /usr/local/etc/cosmoe/locale/catalogs.
# Meson provides DESTDIR-aware prefix in MESON_INSTALL_DESTDIR_PREFIX.
install_prefix="${MESON_INSTALL_DESTDIR_PREFIX:-${MESON_INSTALL_PREFIX:-/usr/local}}"
install_root="$install_prefix/etc/cosmoe/locale/catalogs"

mkdir -p "$install_root"

tmp_root="${TMPDIR:-/tmp}/cosmoe-catalog-install.$$"
trap 'rm -rf "$tmp_root"' EXIT HUP INT TERM
mkdir -p "$tmp_root"

count=0
skipped_no_signature=0
skipped_examples=""
list_file="$tmp_root/catkeys.list"
find "$catkeys_root" -type f -name '*.catkeys' -print > "$list_file"

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

    out_dir="$install_root/$signature"
    out_file="$out_dir/$lang.catalog"
    mkdir -p "$out_dir"

    # Use a temp output to avoid partial files if conversion fails.
    tmp_out="$tmp_root/$lang.catalog"
    "$linkcatkeys_bin" -s "$signature" -l "$lang" -o "$tmp_out" "$catkeys_file" >/dev/null
    install -m 0644 "$tmp_out" "$out_file"

    # Backward-compat path for legacy OpenBeOS signatures.
    if [ -n "$alias_signature" ] && [ "$alias_signature" != "$signature" ]; then
        alias_dir="$install_root/$alias_signature"
        alias_file="$alias_dir/$lang.catalog"
        mkdir -p "$alias_dir"
        install -m 0644 "$tmp_out" "$alias_file"
    fi

    count=$((count + 1))
done < "$list_file"

echo "Installed $count catalogs into $install_root"
if [ "$skipped_no_signature" -gt 0 ]; then
    echo "Skipped $skipped_no_signature .catkeys files without embedded signature." >&2
    printf '%b\n' "$skipped_examples" | sed '/^$/d' | sed 's/^/  - /' >&2
    if [ "$skipped_no_signature" -gt 8 ]; then
        echo "  - ..." >&2
    fi
fi
