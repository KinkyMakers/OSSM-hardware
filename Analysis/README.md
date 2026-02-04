# OSSM Video Analysis Tool

A Python application for analyzing OSSM motion from video recordings or live camera feeds using OpenCV.

## Features

- Load video files and extract frames
- **Live camera tracking** via Jupyter notebook
- Calibrate pixel-to-mm conversion using a reference line
- Track a selected point/circle frame by frame
- Multi-point tracking with position averaging
- Export position-time data to CSV

## Setup

This project uses [uv](https://github.com/astral-sh/uv) for fast Python package management.

```bash
# Install uv (if not already installed)
curl -LsSf https://astral.sh/uv/install.sh | sh

# Sync dependencies (creates .venv automatically)
uv sync
```

## Usage

### Video File Analysis

```bash
uv run ossm_tracker.py path/to/video.mp4
```

Or run without arguments to be prompted for a video file:

```bash
uv run ossm_tracker.py
```

### Live Camera Tracking (Jupyter Notebook)

For real-time tracking from a live camera feed:

```bash
uv sync
uv run jupyter lab ossm_live_tracker.ipynb
```

Run the first cell to display the tracker UI with:
- Camera selection dropdown
- Calibration line drawing
- Tracking point placement
- Real-time position display
- CSV export

The second cell (optional) shows a live-updating position chart.

This is only a quick start. The real docs are [here](../Documentation/ossm/Analysis/getting-started/introduction.mdx)