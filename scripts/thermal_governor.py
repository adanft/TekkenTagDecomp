#!/usr/bin/env python3
"""Keeps the CPU under a temperature ceiling by adjusting the CPU quota of the `ttt.slice` systemd user slice.

`scripts/cool.sh` starts this in the background and runs heavy commands inside `ttt.slice`. Every second the governor
reads the CPU package temperature and moves the slice's CPUQuota between --min and all threads: down fast when the
temperature passes the target, up slowly when there is headroom. It exits once the slice has been empty for --idle
seconds. Only one instance runs at a time (a lock in $XDG_RUNTIME_DIR).

    scripts/thermal_governor.py [--target 75] [--ceiling 78] [--min 200] [--idle 60]
"""

import argparse
import fcntl
import os
import subprocess
import sys
import time
from pathlib import Path

SLICE = "ttt.slice"
RUNTIME = Path(os.environ.get("XDG_RUNTIME_DIR", "/tmp"))


def find_sensor():
    """The package temperature input: Intel coretemp "Package id 0", AMD k10temp "Tctl", else the first temp1."""
    fallback = None
    for hw in sorted(Path("/sys/class/hwmon").glob("hwmon*")):
        name = (hw / "name").read_text().strip() if (hw / "name").exists() else ""
        if name not in ("coretemp", "k10temp", "zenpower"):
            continue
        for label in sorted(hw.glob("temp*_label")):
            if label.read_text().strip() in ("Package id 0", "Tctl", "Tdie"):
                return Path(str(label).replace("_label", "_input"))
        if fallback is None and (hw / "temp1_input").exists():
            fallback = hw / "temp1_input"
    return fallback


def slice_dir():
    return Path(f"/sys/fs/cgroup/user.slice/user-{os.getuid()}.slice/user@{os.getuid()}.service/{SLICE}")


def slice_tasks():
    pids = slice_dir() / "pids.current"
    try:
        return int(pids.read_text())
    except (OSError, ValueError):
        return 0


def set_quota(percent):
    subprocess.run(["systemctl", "--user", "set-property", "--runtime", SLICE, f"CPUQuota={percent}%"],
                   check=False, capture_output=True)


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument("--target", type=float, default=75.0, help="temperature to hold, °C (default 75)")
    p.add_argument("--ceiling", type=float, default=78.0, help="cut the quota hard above this, °C (default 78)")
    p.add_argument("--min", type=int, default=200, help="lowest quota in percent of one thread (default 200)")
    p.add_argument("--idle", type=float, default=60.0, help="exit after the slice is empty this long, s (default 60)")
    args = p.parse_args(argv)

    lock = open(RUNTIME / "ttt-thermal.lock", "w")
    try:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        return 0  # already running
    sensor = find_sensor()
    if sensor is None:
        print("thermal_governor: no CPU temperature sensor found", file=sys.stderr)
        return 1

    full = 100 * (os.cpu_count() or 1)
    quota = full // 2  # start halfway and let the loop find the level
    set_quota(quota)
    log = open(RUNTIME / "ttt-thermal.log", "a", buffering=1)
    empty_since = None
    while True:
        temp = int(sensor.read_text()) / 1000
        if temp >= args.ceiling:
            new = max(args.min, int(quota * 0.6))
        elif temp > args.target:
            new = max(args.min, quota - 100)
        elif temp < args.target - 5:
            new = min(full, quota + 200)
        else:
            new = quota
        new -= new % 100
        if new != quota:
            quota = new
            set_quota(quota)
            log.write(f"{time.strftime('%H:%M:%S')} {temp:.0f}C quota={quota}%\n")
        if slice_tasks() == 0:
            empty_since = empty_since or time.monotonic()
            if time.monotonic() - empty_since > args.idle:
                set_quota(full // 2)  # the next run starts from the middle again
                return 0
        else:
            empty_since = None
        time.sleep(1)


if __name__ == "__main__":
    sys.exit(main())
