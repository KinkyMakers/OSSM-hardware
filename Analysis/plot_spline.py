#!/usr/bin/env python3
"""Plot SplineCtrl telemetry.

Two input formats are supported:

1. Serial-monitor log captured from PlatformIO. X-axis is the device firmware
   timestamp ([millis] prefix on each log line), converted to seconds since
   the first sample. If the device drops a frame the gap will be visible on
   the x-axis.

2. CSV produced by the `test_spline_patterns` unit test. Columns:
      millis_ms,t,position,velocity,acceleration
   The test drives SplinePattern::evaluate(1.0) across 3x totalDuration
   using a faked millis() clock. Position/velocity/acceleration come straight
   from `evaluate()`, so velocity and acceleration are in normalized units
   per second / per second^2.

Usage:
    python plot_spline.py <log_or_csv>
    python plot_spline.py Analysis/spline_samples_abc_ABC-000.csv
"""

import os
import re
import sys

import numpy as np

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

FLOAT = r"-?\d+(?:\.\d+)?"

# Newer PlatformIO/idf-monitor captures embed raw ANSI color codes
# (ESC[37m, ESC[0m, ...) directly in the saved log. They get sprinkled inside
# words and even between digits of numeric fields, so we strip them before
# trying to match anything.
ANSI_ESCAPE_RE = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")

# Per-iteration "evaluate" log line (stroke_engine.cpp). Holds the rich
# spline state: normalized position/velocity, the naive velocity (steps/sec)
# fed to setSpeedInHz, acceleration, the user-driven speed setting, and the
# current spline timeOffset.
EVAL_PATTERN = re.compile(
    rf"\[\s*(\d+)\].*\[SplineCtrl\]\s+"
    rf"t=({FLOAT})\s+pos=({FLOAT})\s*(?:\([^)]*\))?\s+"
    rf"vel=({FLOAT})\s+naive=({FLOAT})\s+acc=({FLOAT}),\s*"
    rf"speed=({FLOAT}),\s*timeOffset=({FLOAT})"
)


def parse_log(path: str):
    """Parse serial-monitor output.

    Returns a dict of numpy arrays. Each firmware iteration emits a single
    log line carrying every field we care about, so we just collect them in
    order.
    """
    eval_rows = []
    with open(path) as f:
        for line in f:
            line = ANSI_ESCAPE_RE.sub("", line)
            me = EVAL_PATTERN.search(line)
            if me:
                eval_rows.append(
                    tuple(float(me.group(i)) for i in range(1, 9))
                )

    if not eval_rows:
        empty_i = np.array([], dtype=np.int64)
        empty_f = np.array([], dtype=np.float64)
        return {
            "millis": empty_i, "t": empty_f, "pos": empty_f, "vel": empty_f,
            "naive": empty_f, "acc": empty_f, "speed": empty_f,
            "timeOffset": empty_f,
        }

    e = np.array(eval_rows, dtype=np.float64)
    return {
        "millis": e[:, 0].astype(np.int64),
        "t": e[:, 1],
        "pos": e[:, 2],
        "vel": e[:, 3],
        "naive": e[:, 4],
        "acc": e[:, 5],
        "speed": e[:, 6],
        "timeOffset": e[:, 7],
    }


def parse_csv(path: str):
    """Parse unit-test CSV; returns arrays + accel + optional speed column."""
    data = np.genfromtxt(path, delimiter=",", names=True)
    millis = data["millis_ms"].astype(np.int64)
    t_param = data["t"].astype(np.float64)
    pos = data["position"].astype(np.float64)
    vel = data["velocity"].astype(np.float64)
    accel = data["acceleration"].astype(np.float64)
    speed = (
        data["speed"].astype(np.float64)
        if "speed" in (data.dtype.names or ())
        else None
    )
    return millis, t_param, pos, vel, accel, speed


def report_cadence(wall_s: np.ndarray) -> None:
    if wall_s.size < 2:
        return
    dt_ms = np.diff(wall_s) * 1000.0
    print(
        f"Sample Δt (ms): min={dt_ms.min():.1f}  median={np.median(dt_ms):.1f}  "
        f"max={dt_ms.max():.1f}  std={dt_ms.std():.1f}"
    )


def plot(
    millis,
    t_param,
    pos,
    vel,
    row4,
    *,
    row4_label,
    out_path,
    title_suffix="",
    speed=None,
):
    wall_s = (millis - millis[0]) / 1000.0
    report_cadence(wall_s)

    fig, axes = plt.subplots(4, 1, figsize=(14, 10), sharex=True)
    line_kw = dict(linewidth=0.8, alpha=0.6)
    dot_kw = dict(s=8)

    # Detect speed-change boundaries (test_sweep_all_patterns_with_speed_change
    # produces a CSV with a `speed` column). Each change is drawn as a dashed
    # vertical line on every subplot so position / velocity / accel kinks line
    # up with the speed event that caused them.
    speed_changes = []
    if speed is not None and speed.size > 1:
        change_idx = np.where(np.diff(speed) != 0)[0] + 1
        for i in change_idx:
            speed_changes.append(
                (wall_s[i], float(speed[i - 1]), float(speed[i]))
            )

    axes[0].plot(wall_s, t_param, color="#ab47bc", **line_kw)
    axes[0].scatter(wall_s, t_param, color="#ab47bc", **dot_kw)
    axes[0].set_ylabel("Spline t")
    axes[0].set_title(
        f"SplineCtrl  ({len(millis)} samples, "
        f"{wall_s[-1]:.2f}s wall-clock span){title_suffix}"
    )
    axes[0].grid(True, alpha=0.3)

    # Secondary x-axis on the top panel that maps device time -> spline t.
    # Uses the observed (wall_s, t_param) pairs via linear interpolation so the
    # mapping is correct even when the spline velocity changes mid-run.
    def wall_to_t(x):
        return np.interp(x, wall_s, t_param)

    def t_to_wall(x):
        return np.interp(x, t_param, wall_s)

    secax = axes[0].secondary_xaxis("top", functions=(wall_to_t, t_to_wall))
    secax.set_xlabel("Spline t (from t= field)")

    axes[1].plot(wall_s, pos, color="#4fc3f7", **line_kw)
    axes[1].scatter(wall_s, pos, color="#4fc3f7", **dot_kw)
    axes[1].set_ylabel("Position")
    axes[1].grid(True, alpha=0.3)

    axes[2].plot(wall_s, vel, color="#ff8a65", **line_kw)
    axes[2].scatter(wall_s, vel, color="#ff8a65", **dot_kw)
    axes[2].set_ylabel("Velocity")
    axes[2].grid(True, alpha=0.3)

    axes[3].plot(wall_s, row4, color="#81c784", **line_kw)
    axes[3].scatter(wall_s, row4, color="#81c784", **dot_kw)
    axes[3].set_ylabel(row4_label)
    axes[3].set_xlabel(
        f"Device time (s, t0 = {millis[0]} ms)"
    )
    axes[3].grid(True, alpha=0.3)

    for x, before, after in speed_changes:
        for ax in axes:
            ax.axvline(x, color="#e53935", linestyle="--", linewidth=1.0,
                       alpha=0.7)
        axes[0].annotate(
            f"speed {before * 100:.0f}% → {after * 100:.0f}%",
            xy=(x, axes[0].get_ylim()[1]),
            xytext=(4, -10),
            textcoords="offset points",
            color="#e53935",
            fontsize=9,
        )

    fig.tight_layout()
    plt.savefig(out_path, dpi=150)
    print(f"Saved {out_path}")


def main(path: str):
    is_csv = path.lower().endswith(".csv")
    if is_csv:
        millis, t_param, pos, vel, accel, speed = parse_csv(path)
        if millis.size == 0:
            print(f"No rows in {path}.")
            sys.exit(1)
        base = os.path.splitext(os.path.basename(path))[0]
        out = os.path.join(SCRIPT_DIR, f"spline_plot_{base}.png")
        plot(
            millis,
            t_param,
            pos,
            vel,
            accel,
            row4_label="Acceleration",
            out_path=out,
            title_suffix=f"  —  {base}",
            speed=speed,
        )
    else:
        rows = parse_log(path)
        if rows["millis"].size == 0:
            print("No SplineCtrl data found in file.")
            sys.exit(1)
        dt = (rows["millis"][-1] - rows["millis"][0]) / 1000.0
        print(f"Parsed {rows['millis'].size} samples over {dt:.1f}s")
        out = os.path.join(SCRIPT_DIR, "spline_plot.png")
        plot_log(rows, out_path=out)


def plot_log(rows, *, out_path: str) -> None:
    """Plot serial-log telemetry with the new acc/speed/timeOffset columns."""
    millis = rows["millis"]
    wall_s = (millis - millis[0]) / 1000.0
    report_cadence(wall_s)

    speed = rows["speed"]
    speed_changes = []
    if speed.size > 1:
        change_idx = np.where(np.diff(speed) != 0)[0] + 1
        for i in change_idx:
            speed_changes.append(
                (wall_s[i], float(speed[i - 1]), float(speed[i]))
            )

    panels = [
        ("Spline t",         rows["t"],          "#ab47bc"),
        ("Position",         rows["pos"],        "#4fc3f7"),
        ("Velocity",         rows["vel"],        "#ff8a65"),
        ("Acceleration",     rows["acc"],        "#81c784"),
        ("Target speed (%)", rows["speed"],      "#ffb74d"),
        ("Time offset",      rows["timeOffset"], "#ba68c8"),
    ]

    fig, axes = plt.subplots(
        len(panels), 1, figsize=(14, 2.0 * len(panels)), sharex=True
    )
    line_kw = dict(linewidth=0.8, alpha=0.6)
    dot_kw = dict(s=8)

    for ax, (label, ydata, color) in zip(axes, panels):
        ax.plot(wall_s, ydata, color=color, **line_kw)
        ax.scatter(wall_s, ydata, color=color, **dot_kw)
        ax.set_ylabel(label)
        ax.grid(True, alpha=0.3)

    # Velocity panel overlay: spline `vel` is normalized units/sec, while
    # `naive` is the steps/sec value actually fed to setSpeedInHz. Put naive
    # on a twin y-axis so both can share the same time base.
    vel_ax = axes[2]
    naive_ax = vel_ax.twinx()
    naive_color = "#7e57c2"
    naive_ax.plot(
        wall_s, rows["naive"], color=naive_color, linewidth=0.8, alpha=0.6,
        label="naive velocity",
    )
    naive_ax.scatter(wall_s, rows["naive"], color=naive_color, s=8)
    naive_ax.set_ylabel("Naive velocity (steps/s)", color=naive_color)
    naive_ax.tick_params(axis="y", labelcolor=naive_color)
    naive_ax.axhline(0, color=naive_color, linestyle=":", linewidth=0.6,
                     alpha=0.4)

    # Tag the spline-velocity line so users know which axis it belongs to.
    vel_ax.set_ylabel("Velocity (units/s)", color="#ff8a65")
    vel_ax.tick_params(axis="y", labelcolor="#ff8a65")

    axes[0].set_title(
        f"SplineCtrl  ({millis.size} samples, "
        f"{wall_s[-1]:.2f}s wall-clock span, "
        f"{len(speed_changes)} speed changes)"
    )

    # Secondary x-axis is only meaningful when spline t is monotonic across
    # the whole capture. In live logs t resets at the start of each pattern
    # iteration, so adding it would produce nonsense tick labels.
    t_param = rows["t"]
    if t_param.size >= 2 and np.all(np.diff(t_param) >= 0):
        def wall_to_t(x):
            return np.interp(x, wall_s, t_param)

        def t_to_wall(x):
            return np.interp(x, t_param, wall_s)

        secax = axes[0].secondary_xaxis(
            "top", functions=(wall_to_t, t_to_wall)
        )
        secax.set_xlabel("Spline t (from t= field)")

    # Annotate every speed change when there aren't too many; otherwise just
    # draw the vertical guide lines so the chart stays readable.
    annotate_changes = len(speed_changes) <= 8
    for x, before, after in speed_changes:
        for ax in axes:
            ax.axvline(
                x, color="#e53935", linestyle="--", linewidth=0.8, alpha=0.4
            )
        if annotate_changes:
            axes[0].annotate(
                f"{before:.0f}→{after:.0f}%",
                xy=(x, axes[0].get_ylim()[1]),
                xytext=(4, -10),
                textcoords="offset points",
                color="#e53935",
                fontsize=8,
            )

    axes[-1].set_xlabel(f"Device time (s, t0 = {millis[0]} ms)")

    fig.tight_layout()
    plt.savefig(out_path, dpi=150)
    print(f"Saved {out_path}")


if __name__ == "__main__":
    path = sys.argv[1] if len(sys.argv) > 1 else None
    if not path:
        print("Usage: python plot_spline.py <log_or_csv>")
        print("       python plot_spline.py Analysis/spline_samples_abc_ABC-000.csv")
        sys.exit(1)
    main(path)
