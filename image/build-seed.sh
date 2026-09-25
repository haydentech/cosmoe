#!/usr/bin/env bash

# Build a NoCloud seed ISO containing the Cosmoe runtime packages and the
# Autoinstall configuration. This is intentionally not a VirtualBox/OVA builder.

set -euo pipefail

readonly script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly repo_dir="$(cd -- "${script_dir}/.." && pwd)"
readonly package_dir="${repo_dir}/debian/packages"
readonly output_dir="${script_dir}/out"
readonly staging_dir="${output_dir}/seed"
readonly output_iso="${output_dir}/cosmoe-autoinstall.iso"

password_hash="${COSMOE_PASSWORD_HASH:-}"
if [[ -z "${password_hash}" ]]; then
    cat >&2 <<'EOF'
COSMOE_PASSWORD_HASH must contain an encrypted password for the cosmoe user.
For this development image, for example:
  COSMOE_PASSWORD_HASH="$(openssl passwd -6 cosmoe)" make image
EOF
    exit 1
fi

if [[ "${password_hash}" == *$'\n'* || "${password_hash}" == *$'\r'* ]]; then
    echo "COSMOE_PASSWORD_HASH must be a single-line password hash." >&2
    exit 1
fi

shopt -s nullglob
runtime_packages=("${package_dir}"/cosmoe_[0-9]*.deb)
apps_packages=("${package_dir}"/cosmoe-apps_[0-9]*.deb)
shopt -u nullglob

if (( ${#runtime_packages[@]} != 1 || ${#apps_packages[@]} != 1 )); then
    cat >&2 <<EOF
Expected exactly one runtime package and one applications package in:
  ${package_dir}
Run 'make deb' before building the seed ISO.
EOF
    exit 1
fi

if command -v xorriso >/dev/null 2>&1; then
    iso_command=(xorriso -as mkisofs)
elif command -v genisoimage >/dev/null 2>&1; then
    iso_command=(genisoimage)
elif command -v mkisofs >/dev/null 2>&1; then
    iso_command=(mkisofs)
else
    echo "Install xorriso (preferred) or genisoimage to build the seed ISO." >&2
    exit 1
fi

rm -rf "${staging_dir}"
mkdir -p "${staging_dir}/packages"

# NoCloud expects these conventional names at the ISO root. Keep interpolation
# out of sed replacement text so characters such as '$' in a crypt hash remain
# literal.
awk -v hash="${password_hash}" '
    /password: "__COSMOE_PASSWORD_HASH__"/ {
        print "    password: \"" hash "\""
        next
    }
    { print }
' "${script_dir}/autoinstall.yaml" >"${staging_dir}/user-data"
: >"${staging_dir}/meta-data"

cp -- "${runtime_packages[0]}" "${apps_packages[0]}" "${staging_dir}/packages/"
cp -a -- "${script_dir}/files" "${staging_dir}/"

"${iso_command[@]}" \
    -quiet \
    -V CIDATA \
    -o "${output_iso}" \
    "${staging_dir}"

echo "Created ${output_iso}"
