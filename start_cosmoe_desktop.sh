#!/bin/bash

# -----------------------------------------------------------------------------
# Start a Cosmoe desktop in a labwc instance
#
# This is useful for developing/testing Cosmoe in a full Desktop setup without
# logging out of your normal desktop session.
#
# Requirements:
#   labwc
# -----------------------------------------------------------------------------

set -euo pipefail

if ! command -v labwc >/dev/null 2>&1; then
    install_hint="Install labwc using your distribution's package manager"

    if [[ -r /etc/os-release ]]; then
        . /etc/os-release
        distro_ids="${ID:-} ${ID_LIKE:-}"
        if [[ "$distro_ids" == *ubuntu* || "$distro_ids" == *debian* ]]; then
            install_hint+=":\nsudo apt install labwc"
        elif [[ "$distro_ids" == *fedora* || "$distro_ids" == *rhel* ]]; then
            install_hint+=":\nsudo dnf install labwc"
        elif [[ "$distro_ids" == *arch* ]]; then
            install_hint+=":\nsudo pacman -S labwc"
        fi
    fi

    echo "labwc is required, but was not found." >&2
    echo >&2
    echo -e "$install_hint" >&2
    exit 1
fi

SOCKET_FILE="$(mktemp "${XDG_RUNTIME_DIR:-/tmp}/labwc-socket.XXXXXX")"
STARTUP_SCRIPT="$(mktemp "${XDG_RUNTIME_DIR:-/tmp}/labwc-startup.XXXXXX")"

cleanup() {
    rm -f "$SOCKET_FILE" "$STARTUP_SCRIPT"
    if [[ -n "${labwc_pid:-}" ]]; then
        kill "$labwc_pid" 2>/dev/null || true
    fi
}

trap cleanup EXIT

cat >"$STARTUP_SCRIPT" <<'EOF'
#!/bin/bash

set -euo pipefail

printf '%s\n' "${WAYLAND_DISPLAY:-}" >"$LABWC_SOCKET_FILE"

if ! grep -qF "MiniTracker" ~/.config/labwc/autostart 2>/dev/null; then
    MiniTracker &
fi

if ! grep -qF "Terminal" ~/.config/labwc/autostart 2>/dev/null; then
    Terminal &
fi

if ! grep -qF "Pulse" ~/.config/labwc/autostart 2>/dev/null; then
    Pulse &
fi

if ! grep -qF "Deskbar" ~/.config/labwc/autostart 2>/dev/null; then
    Deskbar &
fi
EOF

chmod +x "$STARTUP_SCRIPT"

echo "Starting labwc..."

LABWC_SOCKET_FILE="$SOCKET_FILE" labwc -s "$STARTUP_SCRIPT" &
labwc_pid=$!

for _ in {1..50}; do
    if [[ -s "$SOCKET_FILE" ]]; then
        break
    fi
    if ! kill -0 "$labwc_pid" 2>/dev/null; then
        echo "labwc exited before reporting WAYLAND_DISPLAY" >&2
        wait "$labwc_pid"
    fi
    sleep 0.1
done

if [[ ! -s "$SOCKET_FILE" ]]; then
    echo "Timed out waiting for labwc to report WAYLAND_DISPLAY" >&2
    exit 1
fi


echo
echo
echo "labwc environment is now running as pid $labwc_pid."
echo "Press Ctrl+C to terminate."

wait "$labwc_pid"
