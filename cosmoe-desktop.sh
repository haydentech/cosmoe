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

# VirtualBox's VMSVGA driver can accept pointer events while failing to display
# wlroots hardware cursors. Use software cursors and pixmap renderer to avoid this.
export WLR_NO_HARDWARE_CURSORS=1
export WLR_RENDERER=pixman

# D-Bus user session (critical for file dialogs and notifications).
if [ -z "$DBUS_SESSION_BUS_ADDRESS" ]; then
    eval "$(dbus-launch --sh-syntax --exit-with-session)"
fi

# Usually started via systemd --user, but ensure they are available.
systemctl --user start pipewire wireplumber 2>/dev/null || true

# labwc executes -s only after it has created WAYLAND_DISPLAY. Starting
# graphical clients before then makes them fail to connect to the compositor.
startup_script="$(mktemp "${XDG_RUNTIME_DIR:-/tmp}/cosmoe-labwc-startup.XXXXXX")"
cat >"$startup_script" <<'EOF'
#!/bin/sh

/usr/lib/policykit-1-gnome/polkit-gnome-authentication-agent-1 &
/usr/libexec/xdg-desktop-portal -r &
/usr/libexec/xdg-desktop-portal-wlr -r &

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