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
APP_PID_FILE="$(mktemp "${XDG_RUNTIME_DIR:-/tmp}/labwc-app-pids.XXXXXX")"

kill_pid_if_alive() {
    local pid="$1"
    local signal="${2:-TERM}"

    if [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null; then
        kill "-$signal" "$pid" 2>/dev/null || true
    fi
}

kill_wayland_processes() {
    local target_display="$1"
    local pid

    [[ -n "$target_display" ]] || return 0

    for env_file in /proc/[0-9]*/environ; do
        [[ -r "$env_file" ]] || continue

        if ! cat "$env_file" 2>/dev/null | tr '\0' '\n' | grep -qx "WAYLAND_DISPLAY=$target_display"; then
            continue
        fi

        pid="${env_file#/proc/}"
        pid="${pid%/environ}"

        # Skip our own shell process and avoid noisy failures.
        [[ "$pid" == "$$" ]] && continue
        kill_pid_if_alive "$pid" "TERM"
    done

    sleep 0.2

    for env_file in /proc/[0-9]*/environ; do
        [[ -r "$env_file" ]] || continue

        if ! cat "$env_file" 2>/dev/null | tr '\0' '\n' | grep -qx "WAYLAND_DISPLAY=$target_display"; then
            continue
        fi

        pid="${env_file#/proc/}"
        pid="${pid%/environ}"
        [[ "$pid" == "$$" ]] && continue
        kill_pid_if_alive "$pid" "KILL"
    done
}

cleanup() {
    local wayland_display=""

    if [[ -s "$SOCKET_FILE" ]]; then
        wayland_display="$(head -n 1 "$SOCKET_FILE" || true)"
    fi

    if [[ -s "$APP_PID_FILE" ]]; then
        while IFS= read -r pid; do
            [[ "$pid" =~ ^[0-9]+$ ]] || continue
            kill_pid_if_alive "$pid" "TERM"
        done <"$APP_PID_FILE"
    fi

    if [[ -n "${labwc_pid:-}" ]]; then
        kill "$labwc_pid" 2>/dev/null || true

        for _ in {1..20}; do
            if ! kill -0 "$labwc_pid" 2>/dev/null; then
                break
            fi
            sleep 0.1
        done

        kill -KILL "$labwc_pid" 2>/dev/null || true
    fi

    if [[ -s "$APP_PID_FILE" ]]; then
        while IFS= read -r pid; do
            [[ "$pid" =~ ^[0-9]+$ ]] || continue
            kill_pid_if_alive "$pid" "KILL"
        done <"$APP_PID_FILE"
    fi

    kill_wayland_processes "$wayland_display"
    rm -f "$SOCKET_FILE" "$STARTUP_SCRIPT" "$APP_PID_FILE"
}

trap cleanup EXIT

cat >"$STARTUP_SCRIPT" <<'EOF'
#!/bin/bash

set -euo pipefail

printf '%s\n' "${WAYLAND_DISPLAY:-}" >"$LABWC_SOCKET_FILE"

touch "$LABWC_APP_PID_FILE"

if ! grep -qF "Tracker" ~/.config/labwc/autostart 2>/dev/null; then
    Tracker &
    echo "$!" >>"$LABWC_APP_PID_FILE"
fi

if ! grep -qF "Deskbar" ~/.config/labwc/autostart 2>/dev/null; then
    Deskbar &
    echo "$!" >>"$LABWC_APP_PID_FILE"
fi

if ! grep -qF "Terminal" ~/.config/labwc/autostart 2>/dev/null; then
    Terminal &
    echo "$!" >>"$LABWC_APP_PID_FILE"
fi

EOF

chmod +x "$STARTUP_SCRIPT"

echo "Starting labwc..."

LABWC_SOCKET_FILE="$SOCKET_FILE" LABWC_APP_PID_FILE="$APP_PID_FILE" labwc -s "$STARTUP_SCRIPT" &
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
