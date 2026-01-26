# OSSM Video Analysis Tool

A Python application for analyzing OSSM motion from video recordings using OpenCV.

## Features

- Load video files and extract frames
- Calibrate pixel-to-mm conversion using a reference line
- Track a selected point/circle frame by frame
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

```bash
uv run ossm_tracker.py path/to/video.mp4
```

Or run without arguments to be prompted for a video file:

```bash
uv run ossm_tracker.py
```


This is only a quick start. The real docs are [here](../Documentation/ossm/Analysis/getting-started/introduction.mdx)