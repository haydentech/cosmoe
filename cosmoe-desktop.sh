#!/bin/bash

# 1. Essential environment
export XDG_SESSION_TYPE=wayland
export XDG_SESSION_DESKTOP=cosmoe-desktop
export XDG_CURRENT_DESKTOP=cosmoe-desktop  # For xdg-desktop-portal compatibility
export MOZ_ENABLE_WAYLAND=1               # Firefox/Thunderbird native Wayland
export QT_QPA_PLATFORM=wayland            # Qt apps

# 2. D-Bus user session (critical for file dialogs, notifications)
if [ -z "$DBUS_SESSION_BUS_ADDRESS" ]; then
    eval "$(dbus-launch --sh-syntax --exit-with-session)"
fi

# 3. PipeWire/WirePlumber (Ubuntu 24.04 uses these for audio/camera)
# Usually started via systemd --user, but ensure they’re up:
systemctl --user start pipewire wireplumber 2>/dev/null || true

# 4. Polkit agent (so GUI apps can request root permissions)
/usr/lib/policykit-1-gnome/polkit-gnome-authentication-agent-1 &
# Alternative if you prefer a lighter agent: /usr/libexec/lxqt-policykit-agent &

# 5. Portals (file open/save, screen sharing)
# You need xdg-desktop-portal and a backend (wlr or gtk)
/usr/libexec/xdg-desktop-portal -r &
/usr/libexec/xdg-desktop-portal-wlr -r &   # or -gtk if you prefer GTK file picker

# 6. Your components (adjust paths/names to your actual binaries)
if ! grep -qF "Tracker" ~/.config/labwc/autostart 2>/dev/null; then
    Tracker &
    echo "$!" >>"$LABWC_APP_PID_FILE"
fi

if ! grep -qF "Deskbar" ~/.config/labwc/autostart 2>/dev/null; then
    Deskbar &
    echo "$!" >>"$LABWC_APP_PID_FILE"
fi

mako &  # or dunstify, fnott, etc. for notifications

# 7. Finally, the compositor
# -d disables the default autostart (so we control the startup)
exec labwc