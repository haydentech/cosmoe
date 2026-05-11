#!/bin/bash

# -----------------------------------------------------------------------------
# Start a Weston instance, then nest labwc inside it.
#
# This is useful for developing/testing labwc without logging out
# of your normal desktop session.
#
# Requirements:
#   sudo apt install weston labwc xwayland
#
# Usage:
#   chmod +x run-labwc-nested.sh
#   ./run-labwc-nested.sh
# -----------------------------------------------------------------------------

set -e

WESTON_SOCKET="wayland-labwc-dev"

echo "Starting Weston nested compositor..."
weston \
    --socket="$WESTON_SOCKET" \
    --width=1600 \
    --height=900 \
    &
WESTON_PID=$!

# Give Weston a moment to initialize
sleep 2

echo "Starting labwc inside Weston..."

WAYLAND_DISPLAY="$WESTON_SOCKET" \
WLR_BACKENDS=wayland \
WLR_RENDERER=pixman \
WLR_WL_OUTPUTS=1 \
labwc -s labwc-cosmoe-socket &
LABWC_PID=$!

LABWC_SOCKET=wayland-0

echo "labwc socket: $LABWC_SOCKET"

# --------------------------------------------------------------------
# OPTIONAL:
# Start your shell/taskbar/desktop application inside the labwc session.
#
# Uncomment and modify the line below later:
#
# WAYLAND_DISPLAY=wayland-1 /path/to/your-shell
#
# Notes:
#   - wayland-1 is usually the display created by labwc inside Weston
#   - You can confirm with:
#         ls /run/user/$(id -u)/
#
# Example:
#
# WAYLAND_DISPLAY=wayland-1 ~/src/mytaskbar/build/mytaskbar
# --------------------------------------------------------------------

echo
echo "Weston PID : $WESTON_PID"
echo "labwc PID  : $LABWC_PID"
echo
echo "Nested labwc environment is running."
echo "Press Ctrl+C to stop."

WAYLAND_DISPLAY="$LABWC_SOCKET" Deskbar &
WAYLAND_DISPLAY="$LABWC_SOCKET" Showcase

trap 'kill $LABWC_PID $WESTON_PID 2>/dev/null' EXIT

wait
