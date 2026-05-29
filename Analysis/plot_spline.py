#!/usr/bin/env python3
"""Plot SplineCtrl telemetry.

Two input formats are supported:

1. Serial-monitor log captured from PlatformIO. X-axis is the device firmware
   timestamp ([millis] prefix on each log line), converted to seconds since
   the first sample. If the device drops a frame the gap will be visible on
   the x-axis.

2. CSV produced by the `test_spline_patterns` unit test. Columns:
      millis_ms,t,position,handleY,velocity,acceleration[,speed]
   The test drives SplinePattern::evaluate(1.0) across 3x totalDuration using
   a faked millis() clock. Position / velocity / acceleration come straight
   from the evaluator, so units are normalized per second / per second^2.

Usage:
    python plot_spline.py
        Plot every spline_samples_*.csv in this directory.

    python plot_spline.py <log_or_csv>
        Plot a single log or CSV file.
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
    rf"t=({FLOAT})\s+pos=({FLOAT})\s+handleY=({FLOAT})\s+\({FLOAT}%\)\s+"
    rf"vel=({FLOAT})\s+naive=({FLOAT})\s+"
    rf"acc=({FLOAT}),\s*"
    rf"speed=({FLOAT}),\s*timeOffset=({FLOAT})"
)
EVAL_PATTERN_LEGACY = re.compile(
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
                    (
                        float(me.group(1)),
                        float(me.group(2)),
                        float(me.group(3)),
                        float(me.group(4)),
                        float(me.group(5)),
                        float(me.group(6)),
                        float(me.group(7)),
                        np.nan,
                        float(me.group(8)),
                        float(me.group(9)),
                    )
                )
                continue
            me = EVAL_PATTERN_LEGACY.search(line)
            if me:
                eval_rows.append(
                    (
                        float(me.group(1)),
                        float(me.group(2)),
                        float(me.group(3)),
                        float(me.group(3)),
                        float(me.group(4)),
                        float(me.group(5)),
                        float(me.group(6)),
                        np.nan,
                        float(me.group(7)),
                        float(me.group(8)),
                    )
                )

    if not eval_rows:
        empty_i = np.array([], dtype=np.int64)
        empty_f = np.array([], dtype=np.float64)
        return {
            "millis": empty_i, "t": empty_f, "pos": empty_f,
            "handleY": empty_f, "vel": empty_f, "naive": empty_f,
            "acc": empty_f, "jerk": empty_f, "speed": empty_f,
            "timeOffset": empty_f,
        }

    e = np.array(eval_rows, dtype=np.float64)
    return {
        "millis": e[:, 0].astype(np.int64),
        "t": e[:, 1],
        "pos": e[:, 2],
        "handleY": e[:, 3],
        "vel": e[:, 4],
        "naive": e[:, 5],
        "acc": e[:, 6],
        "jerk": e[:, 7],
        "speed": e[:, 8],
        "timeOffset": e[:, 9],
    }


def parse_csv(path: str):
    """Parse unit-test CSV.

    Returns a dict of numpy arrays. The `handleY` and `speed` columns are
    optional — older CSVs omit `handleY`; speed-change variants add `speed`.
    """
    data = np.genfromtxt(path, delimiter=",", names=True)
    names = data.dtype.names or ()
    pos = data["position"].astype(np.float64)
    return {
        "millis": data["millis_ms"].astype(np.int64),
        "t": data["t"].astype(np.float64),
        "pos": pos,
        "handleY": (
            data["handleY"].astype(np.float64)
            if "handleY" in names
            else np.clip(pos, 0.0, 1.0)
        ),
        "vel": data["velocity"].astype(np.float64),
        "acc": data["acceleration"].astype(np.float64),
        "speed": (
            data["speed"].astype(np.float64) if "speed" in names else None
        ),
    }


def report_cadence(wall_s: np.ndarray) -> None:
    if wall_s.size < 2:
        return
    dt_ms = np.diff(wall_s) * 1000.0
    print(
        f"Sample Δt (ms): min={dt_ms.min():.1f}  median={np.median(dt_ms):.1f}  "
        f"max={dt_ms.max():.1f}  std={dt_ms.std():.1f}"
    )


COLORS = {
    "t": "#ab47bc",
    "pos": "#4fc3f7",
    "vel": "#ff8a65",
    "acc": "#81c784",
}


def _detect_speed_changes(wall_s, speed):
    changes = []
    if speed is None or speed.size <= 1:
        return changes
    change_idx = np.where(np.diff(speed) != 0)[0] + 1
    for i in change_idx:
        changes.append((wall_s[i], float(speed[i - 1]), float(speed[i])))
    return changes


def plot(data, *, out_path: str, title_suffix: str = ""):
    """Plot a unit-test CSV from SplinePattern::evaluate()."""
    millis = data["millis"]
    wall_s = (millis - millis[0]) / 1000.0
    report_cadence(wall_s)

    panel_keys = ["t", "pos", "vel", "acc"]
    panel_labels = {
        "t": "Spline t",
        "pos": "Position",
        "vel": "Velocity",
        "acc": "Acceleration",
    }
    key_of = {"t": "t", "pos": "pos", "vel": "vel", "acc": "acc"}

    fig, axes = plt.subplots(
        len(panel_keys), 1, figsize=(14, 2.4 * len(panel_keys)), sharex=True
    )
    if len(panel_keys) == 1:
        axes = [axes]

    line_kw = dict(linewidth=0.9, alpha=0.7)
    dot_kw = dict(s=6, alpha=0.8)

    speed_changes = _detect_speed_changes(wall_s, data["speed"])

    for ax, key in zip(axes, panel_keys):
        y = data.get(key_of[key])
        if y is not None:
            color = COLORS[key]
            ax.plot(wall_s, y, color=color, **line_kw)
            ax.scatter(wall_s, y, color=color, **dot_kw)
            if key == "pos":
                handle_y = data.get("handleY")
                if handle_y is not None and not np.allclose(handle_y, y):
                    ax.plot(
                        wall_s, handle_y, color="#1565c0", linewidth=1.2,
                        alpha=0.95, label="handleY [0,1]",
                    )
        ax.set_ylabel(panel_labels[key])
        ax.grid(True, alpha=0.3)

    if axes[0].get_legend_handles_labels()[0]:
        axes[0].legend(loc="upper right", fontsize=9, framealpha=0.85)

    axes[0].set_title(
        f"SplineCtrl  ({len(millis)} samples, {wall_s[-1]:.2f}s wall-clock span)"
        f"{title_suffix}"
    )

    def wall_to_t(x):
        return np.interp(x, wall_s, data["t"])

    def t_to_wall(x):
        return np.interp(x, data["t"], wall_s)

    secax = axes[0].secondary_xaxis("top", functions=(wall_to_t, t_to_wall))
    secax.set_xlabel("Spline t (from t= field)")

    axes[-1].set_xlabel(f"Device time (s, t0 = {millis[0]} ms)")

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


def discover_csv_targets(analysis_dir: str | None = None) -> list[str]:
    """Return plot targets: all spline_samples_*.csv files."""
    root = analysis_dir or SCRIPT_DIR
    targets = []
    for name in sorted(os.listdir(root)):
        if not name.startswith("spline_samples_") or not name.endswith(".csv"):
            continue
        if name.endswith("_feasible.csv"):
            continue
        targets.append(os.path.join(root, name))
    return targets


def main(path: str) -> int:
    is_csv = path.lower().endswith(".csv")
    if is_csv:
        data = parse_csv(path)
        if data["millis"].size == 0:
            print(f"No rows in {path}.")
            return 1

        base = os.path.splitext(os.path.basename(path))[0]
        out = os.path.join(SCRIPT_DIR, f"spline_plot_{base}.png")
        plot(data, out_path=out, title_suffix=f"  —  {base}")
        return 0

    rows = parse_log(path)
    if rows["millis"].size == 0:
        print("No SplineCtrl data found in file.")
        return 1
    dt = (rows["millis"][-1] - rows["millis"][0]) / 1000.0
    print(f"Parsed {rows['millis'].size} samples over {dt:.1f}s")
    out = os.path.join(SCRIPT_DIR, "spline_plot.png")
    plot_log(rows, out_path=out)
    return 0


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
    if len(sys.argv) > 1:
        sys.exit(main(sys.argv[1]))

    targets = discover_csv_targets()
    if not targets:
        print(f"No spline_samples_*.csv files found in {SCRIPT_DIR}")
        sys.exit(1)

    failed = []
    for path in targets:
        print(f"\n--- {os.path.basename(path)} ---")
        if main(path) != 0:
            failed.append(path)

    if failed:
        print(f"\nFailed to plot {len(failed)}/{len(targets)} file(s).")
        sys.exit(1)

    print(f"\nPlotted {len(targets)} CSV(s).")
