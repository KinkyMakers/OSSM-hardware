#!/usr/bin/env python3
"""Download a validation-gated immutable release for an explicit legacy alias."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any

CONTROL_PLANE = "https://dashboard.researchanddesire.com"
REQUIRED_GATES = {"build", "unit", "integration", "artifact", "hardware"}


def fetch(url: str, token: str | None = None) -> bytes:
    headers = {"User-Agent": "rad-firmware-alias/1"}
    if token:
        headers["Authorization"] = f"Bearer {token}"
    request = urllib.request.Request(url, headers=headers)
    try:
        with urllib.request.urlopen(request, timeout=120) as response:
            return response.read()
    except urllib.error.HTTPError as error:
        detail = error.read().decode(errors="replace")
        raise RuntimeError(f"download returned HTTP {error.code}: {detail}") from error


def download_release(release_id: str, track: str, device: str, output: Path) -> str:
    token = os.environ.get("FIRMWARE_PUBLISH_TOKEN", "").strip()
    if not token:
        raise RuntimeError("FIRMWARE_PUBLISH_TOKEN is required")
    base = os.environ.get("FIRMWARE_CONTROL_PLANE_BASE_URL", CONTROL_PLANE).rstrip("/")
    status = json.loads(fetch(f"{base}/api/internal/firmware/v1/releases/{release_id}?track={track}", token))
    if status.get("track") != track or status.get("deviceType") != device:
        raise RuntimeError("release track or device family does not match")
    allowed = {"active"} if track == "staging" else {"ready", "active"}
    if status.get("lifecycle") not in allowed:
        raise RuntimeError("release has not passed the required lifecycle gate")
    build_sha = status.get("buildSha")
    passed = {
        result["layer"]
        for result in status.get("validations", [])
        if result.get("status") == "success" and result.get("repository_sha") == build_sha
    }
    if not REQUIRED_GATES.issubset(passed):
        raise RuntimeError("release is missing a successful persisted validation gate")
    manifest_url = status["manifestUrl"]
    manifest_bytes = fetch(manifest_url)
    manifest = json.loads(manifest_bytes)
    if manifest.get("track") != track or manifest.get("deviceType") != device or manifest.get("buildSha") != build_sha:
        raise RuntimeError("manifest identity does not match the release")
    output.mkdir(parents=True, exist_ok=True)
    (output / "manifest.json").write_bytes(manifest_bytes)
    object_base = manifest_url.rsplit("/", 1)[0]
    for artifact in manifest.get("artifacts", []):
        filename = artifact.get("filename", "")
        if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]*", filename):
            raise RuntimeError("manifest contains an unsafe filename")
        content = fetch(f"{object_base}/{filename}")
        if len(content) != artifact.get("sizeBytes"):
            raise RuntimeError(f"size mismatch for {filename}")
        if hashlib.sha256(content).hexdigest() != artifact.get("sha256"):
            raise RuntimeError(f"SHA-256 mismatch for {filename}")
        (output / filename).write_bytes(content)
    (output / "version.json").write_text(json.dumps({"version": status["version"], "buildSha": build_sha, "releaseId": release_id}, indent=2) + "\n")
    return status["version"]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--release-id", required=True)
    parser.add_argument("--track", required=True, choices=("main", "staging"))
    parser.add_argument("--device", required=True, choices=("dtt", "lkbx", "ossm", "radr"))
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    version = download_release(args.release_id, args.track, args.device, args.output)
    github_output = os.environ.get("GITHUB_OUTPUT")
    if github_output:
        with open(github_output, "a", encoding="utf-8") as handle:
            handle.write(f"version={version}\n")
    print(version)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
