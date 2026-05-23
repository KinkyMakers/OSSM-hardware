#!/usr/bin/env python3
"""Plot SplineCtrl telemetry.

Two input formats are supported:

1. Serial-monitor log captured from PlatformIO. X-axis is the device firmware
   timestamp ([millis] prefix on each log line), converted to seconds since
   the first sample. If the device drops a frame the gap will be visible on
   the x-axis.

2. CSV produced by the `test_spline_patterns` unit test. Columns:
      millis_ms,t,position,velocity,acceleration[,jerk][,speed]
   The test drives SplinePattern::evaluate(1.0) (and evaluateFeasible(1.0))
   across 3x totalDuration using a faked millis() clock. Position / velocity /
   acceleration come straight from the evaluator, so units are normalized per
   second / per second^2 / per second^3.

   When a raw `spline_samples_<id>.csv` is plotted, the script also looks for
   a sibling `spline_samples_<id>_feasible.csv` (output of evaluateFeasible)
   and overlays it on the same panels. Raw curves are drawn faded/dashed and
   the feasible curves are drawn solid so the time-stretching introduced by
   MAX_SPEED / MAX_ACCEL / MAX_JERK feasibility is visible at a glance. A
   jerk panel is added when the feasible CSV is present.

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
    """Parse unit-test CSV.

    Returns a dict of numpy arrays. The `jerk` and `speed` columns are
    optional — only present in `_feasible.csv` and the speed-change variants
    respectively.
    """
    data = np.genfromtxt(path, delimiter=",", names=True)
    names = data.dtype.names or ()
    return {
        "millis": data["millis_ms"].astype(np.int64),
        "t": data["t"].astype(np.float64),
        "pos": data["position"].astype(np.float64),
        "vel": data["velocity"].astype(np.float64),
        "acc": data["acceleration"].astype(np.float64),
        "jerk": (
            data["jerk"].astype(np.float64) if "jerk" in names else None
        ),
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


# Channel colors: raw uses the soft pastels; feasible uses a darker shade so
# the overlay reads cleanly without picking a clashing hue.
RAW_COLORS = {
    "t":    "#ab47bc",
    "pos":  "#4fc3f7",
    "vel":  "#ff8a65",
    "acc":  "#81c784",
    "jerk": "#ffd54f",
}
FEASIBLE_COLORS = {
    "t":    "#6a1b9a",
    "pos":  "#0277bd",
    "vel":  "#d84315",
    "acc":  "#2e7d32",
    "jerk": "#f57c00",
}


def _detect_speed_changes(wall_s, speed):
    changes = []
    if speed is None or speed.size <= 1:
        return changes
    change_idx = np.where(np.diff(speed) != 0)[0] + 1
    for i in change_idx:
        changes.append((wall_s[i], float(speed[i - 1]), float(speed[i])))
    return changes


def plot(raw, feasible=None, *, out_path: str, title_suffix: str = ""):
    """Plot a raw `evaluate()` CSV, optionally overlaid with an
    `evaluateFeasible()` CSV sampled across the same wall-clock window.

    raw / feasible are dicts as returned by parse_csv (or None for feasible
    if no sibling file exists). Feasible may carry the extra `jerk` column;
    when present a 5th panel is added.
    """
    millis = raw["millis"]
    wall_s = (millis - millis[0]) / 1000.0
    report_cadence(wall_s)

    has_feasible = feasible is not None and feasible["millis"].size > 0
    has_jerk = has_feasible and feasible["jerk"] is not None
    panel_keys = ["t", "pos", "vel", "acc"] + (["jerk"] if has_jerk else [])
    panel_labels = {
        "t": "Spline t",
        "pos": "Position",
        "vel": "Velocity",
        "acc": "Acceleration",
        "jerk": "Jerk",
    }
    raw_key_of = {"t": "t", "pos": "pos", "vel": "vel", "acc": "acc",
                  "jerk": "jerk"}

    fig, axes = plt.subplots(
        len(panel_keys), 1, figsize=(14, 2.4 * len(panel_keys)), sharex=True
    )
    if len(panel_keys) == 1:
        axes = [axes]

    # Raw curve is drawn faded/dashed so the feasible overlay reads as the
    # primary signal when both are present. When raw is plotted alone it
    # falls back to a solid line (no overlay to disambiguate against).
    raw_alone = not has_feasible
    raw_line_kw = dict(
        linewidth=0.9 if raw_alone else 0.8,
        alpha=0.7 if raw_alone else 0.45,
        linestyle="-" if raw_alone else "--",
    )
    raw_dot_kw = dict(s=6, alpha=0.6 if has_feasible else 0.8)
    feasible_line_kw = dict(linewidth=1.4, alpha=0.95)
    feasible_dot_kw = dict(s=8)

    # Wall-clock x for the feasible CSV. Both sweeps were driven by the same
    # faked millis() clock so they share an origin; we still rebase against
    # raw's first millis to keep one consistent x-axis.
    if has_feasible:
        feasible_wall_s = (feasible["millis"] - millis[0]) / 1000.0

    speed_changes = _detect_speed_changes(wall_s, raw["speed"])

    for ax, key in zip(axes, panel_keys):
        raw_y = raw.get(raw_key_of[key])
        if raw_y is not None and key != "jerk":
            color = RAW_COLORS[key]
            label = "evaluate()" if has_feasible else None
            ax.plot(wall_s, raw_y, color=color, label=label, **raw_line_kw)
            ax.scatter(wall_s, raw_y, color=color, **raw_dot_kw)
        if has_feasible:
            f_y = feasible.get(raw_key_of[key])
            if f_y is not None:
                color = FEASIBLE_COLORS[key]
                ax.plot(feasible_wall_s, f_y, color=color,
                        label="evaluateFeasible()", **feasible_line_kw)
                ax.scatter(feasible_wall_s, f_y, color=color,
                           **feasible_dot_kw)
        ax.set_ylabel(panel_labels[key])
        ax.grid(True, alpha=0.3)

    if has_feasible:
        axes[0].legend(loc="upper right", fontsize=9, framealpha=0.85)

    axes[0].set_title(
        f"SplineCtrl  ({len(millis)} raw samples"
        + (f" + {feasible['millis'].size} feasible" if has_feasible else "")
        + f", {wall_s[-1]:.2f}s wall-clock span){title_suffix}"
    )

    # Secondary x-axis on the top panel maps device time -> spline t for the
    # raw stream (kept consistent with previous behaviour).
    def wall_to_t(x):
        return np.interp(x, wall_s, raw["t"])

    def t_to_wall(x):
        return np.interp(x, raw["t"], wall_s)

    secax = axes[0].secondary_xaxis("top", functions=(wall_to_t, t_to_wall))
    secax.set_xlabel("Raw spline t (from t= field)")

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


# Maps Analysis/spline_samples_<id>.csv -> Analysis/spline_samples_<id>_feasible.csv.
# Run only on the raw file; the script auto-discovers the sibling and overlays
# it. Speed-change CSVs (no _feasible sibling) plot the raw curve standalone.
def _feasible_sibling_path(raw_path: str):
    if raw_path.endswith("_feasible.csv"):
        return None
    base, ext = os.path.splitext(raw_path)
    if ext.lower() != ".csv":
        return None
    candidate = f"{base}_feasible.csv"
    return candidate if os.path.exists(candidate) else None


def main(path: str):
    is_csv = path.lower().endswith(".csv")
    if is_csv:
        raw = parse_csv(path)
        if raw["millis"].size == 0:
            print(f"No rows in {path}.")
            sys.exit(1)

        feasible = None
        sibling = _feasible_sibling_path(path)
        if sibling is not None:
            feasible = parse_csv(sibling)
            print(f"Overlaying feasible CSV: {sibling}")
        elif path.endswith("_feasible.csv"):
            # User pointed us straight at a feasible CSV. Plot it as the
            # `feasible` overlay against an empty raw spec so the jerk panel
            # appears and the feasible curve renders in its standard color.
            feasible = raw
            raw = {
                "millis": feasible["millis"],
                "t": feasible["t"],
                "pos": np.full_like(feasible["pos"], np.nan),
                "vel": np.full_like(feasible["vel"], np.nan),
                "acc": np.full_like(feasible["acc"], np.nan),
                "jerk": None,
                "speed": feasible["speed"],
            }

        base = os.path.splitext(os.path.basename(path))[0]
        out = os.path.join(SCRIPT_DIR, f"spline_plot_{base}.png")
        plot(
            raw,
            feasible=feasible,
            out_path=out,
            title_suffix=f"  —  {base}",
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
