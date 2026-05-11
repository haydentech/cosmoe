#!/bin/sh

set -eu

prefix="$1"
desktop_id="$2"
app_name="$3"
exec_name="$4"
icon_name="$5"
categories="$6"

destdir="${DESTDIR:-}"
applications_dir="${destdir}${prefix}/share/applications"
desktop_file="${applications_dir}/${desktop_id}.desktop"

mkdir -p "${applications_dir}"

cat > "${desktop_file}" <<EOF
[Desktop Entry]
Type=Application
Version=1.0
Name=${app_name}
Exec=${exec_name}
Icon=${icon_name}
Terminal=false
Categories=${categories}
StartupNotify=true
StartupWMClass=${desktop_id}
EOF

if command -v update-desktop-database >/dev/null 2>&1; then
	update-desktop-database "${applications_dir}" 2>/dev/null || true
fi