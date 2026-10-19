#!/bin/sh
# Runs a command inside the `ttt.slice` systemd user slice, whose CPU quota scripts/thermal_governor.py adjusts so the
# CPU package stays under about 78 °C. The command may use every thread; the governor trims it only when hot.
#
#   scripts/cool.sh ninja
#   scripts/cool.sh .venv/bin/python scripts/permute.py src/battle_flow.c Fighter_StepMove
#
# COOL_JOBS (default: all threads) is exported for tools that take a job count. The command runs at nice 5.
set -eu
here="$(cd "$(dirname "$0")" && pwd)"
export COOL_JOBS="${COOL_JOBS:-$(nproc)}"
if command -v systemd-run >/dev/null 2>&1 && systemctl --user show-environment >/dev/null 2>&1; then
    # The governor holds a lock, so starting it again while one runs is a no-op.
    setsid python3 "$here/thermal_governor.py" </dev/null >/dev/null 2>&1 &
    exec systemd-run --user --scope -q --slice=ttt.slice -- nice -n 5 "$@"
fi
echo "cool.sh: no systemd user session; running without the temperature governor" >&2
exec nice -n 5 "$@"
