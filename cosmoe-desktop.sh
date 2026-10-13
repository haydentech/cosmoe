#!/bin/bash

set -euo pipefail

# Essential environment
export XDG_SESSION_TYPE=wayland
export XDG_SESSION_DESKTOP=cosmoe
export XDG_CURRENT_DESKTOP=cosmoe
export MOZ_ENABLE_WAYLAND=1
export QT_QPA_PLATFORM=wayland
export XCURSOR_THEME="${XCURSOR_THEME:-Adwaita}"
export XCURSOR_SIZE="${XCURSOR_SIZE:-24}"

# A display manager normally creates this session. Re-exec under one when the
# launcher is started directly from a terminal.
if [ -z "${DBUS_SESSION_BUS_ADDRESS:-}" ]; then
    exec dbus-run-session -- "$0" "$@"
fi

bootstrap_mime_database() {
    config_home="${XDG_CONFIG_HOME:-$HOME/.config}"
    mime_db="$config_home/cosmoe/mime_db"
    marker="$mime_db/.cosmoe-package-version"

    if command -v dpkg-query >/dev/null 2>&1; then
        package_version="$(dpkg-query -W -f='${Version}' cosmoe 2>/dev/null || true)"
        package_files() {
            dpkg-query -L cosmoe cosmoe-apps cosmoe-desktop 2>/dev/null
        }
    elif command -v rpm >/dev/null 2>&1; then
        package_version="$(rpm -q --qf '%{VERSION}-%{RELEASE}' cosmoe 2>/dev/null || true)"
        package_files() {
            rpm -ql cosmoe cosmoe-apps cosmoe-desktop 2>/dev/null
        }
    else
        return 0
    fi

    [ -n "$package_version" ] || return 0
    [ -r "$marker" ] && [ "$(cat "$marker")" = "$package_version" ] && return 0
    command -v mimeset >/dev/null 2>&1 || return 0

    mkdir -p "$mime_db" || return 0
    if ! package_files \
        | while IFS= read -r path; do
            [ -x "$path" ] && printf '%s\n' "$path"
        done \
        | mimeset -a --mimedb "$mime_db" @; then
        return 0
    fi
    printf '%s\n' "$package_version" >"$marker"
}

# Package archives do not retain the extended attributes used by Cosmoe's MIME
# database. Recreate application metadata in the user's writable database.
bootstrap_mime_database

# VirtualBox's VMSVGA driver can accept pointer events while failing to display
# wlroots hardware cursors. Use software cursors and pixmap renderer to avoid this.
export WLR_NO_HARDWARE_CURSORS=1
export WLR_RENDERER=pixman

# Usually started via systemd --user, but ensure they are available.
systemctl --user start pipewire wireplumber 2>/dev/null || true

# labwc executes -s only after it has created WAYLAND_DISPLAY. Starting
# graphical clients before then makes them fail to connect to the compositor.
startup_script="$(mktemp "${XDG_RUNTIME_DIR:-/tmp}/cosmoe-labwc-startup.XXXXXX")"
cat >"$startup_script" <<'EOF'
#!/bin/sh

if [ -x /usr/lib/policykit-1-gnome/polkit-gnome-authentication-agent-1 ]; then
    /usr/lib/policykit-1-gnome/polkit-gnome-authentication-agent-1 &
fi

xdg-desktop-portal -r &
xdg-desktop-portal-wlr -r &

if ! grep -qF "Tracker" "$HOME/.config/labwc/autostart" 2>/dev/null; then
    Tracker &
fi

if ! grep -qF "Deskbar" "$HOME/.config/labwc/autostart" 2>/dev/null; then
    Deskbar &
fi

if ! grep -qF "Terminal" "$HOME/.config/labwc/autostart" 2>/dev/null; then
    Terminal &
fi

mako &
EOF
chmod 700 "$startup_script"

exec labwc -s "$startup_script"