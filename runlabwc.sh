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

trap 'kill $LABWC_PID $WESTON_PID 2>/dev/null' EXIT

echo "labwc socket: $LABWC_SOCKET"
echo
echo "Weston PID  : $WESTON_PID"
echo "labwc PID   : $LABWC_PID"
echo
echo "Nested labwc environment is running."
echo "Press Ctrl+C to terminate."

WAYLAND_DISPLAY="$LABWC_SOCKET" MiniTracker &
WAYLAND_DISPLAY="$LABWC_SOCKET" Terminal &
WAYLAND_DISPLAY="$LABWC_SOCKET" Deskbar

wait
