#!/usr/bin/env python3
"""Prepare a reviewed semantic-version PR without building release artifacts."""

from __future__ import annotations

import argparse
import json
import os
import re
from pathlib import Path


def bump_version(current: str, bump: str) -> str:
    major, minor, patch = (int(part) for part in current.split("."))
    if bump == "major":
        major, minor, patch = major + 1, 0, 0
    elif bump == "minor":
        minor, patch = minor + 1, 0
    else:
        patch += 1
    return f"{major}.{minor}.{patch}"


def prepare(header: Path, changelog: Path, bump: str, version_json: Path | None) -> str:
    content = header.read_text()
    match = re.search(r'^#define VERSION "(\d+\.\d+\.\d+)"$', content, re.M)
    if not match:
        raise ValueError(f"unable to read semantic version from {header}")
    version = bump_version(match.group(1), bump)
    major, minor, patch = version.split(".")
    content = re.sub(r'^#define VERSION ".*"$', f'#define VERSION "{version}"', content, flags=re.M)
    content = re.sub(r'^#define MAJOR_VERSION \d+$', f'#define MAJOR_VERSION {major}', content, flags=re.M)
    content = re.sub(r'^#define MINOR_VERSION \d+$', f'#define MINOR_VERSION {minor}', content, flags=re.M)
    content = re.sub(r'^#define PATCH_VERSION \d+$', f'#define PATCH_VERSION {patch}', content, flags=re.M)
    header.write_text(content)
    if version_json:
        version_json.write_text(json.dumps({"version": version, "major": int(major), "minor": int(minor), "patch": int(patch)}, indent=2) + "\n")
    notes = changelog.read_text()
    marker = re.search(r'<Update[^>]*>\n', notes)
    if not marker:
        raise ValueError(f"unable to find first Update block in {changelog}")
    if f"### v{version}" not in notes:
        draft = f'\n### v{version}\n\n- **Release notes draft** — Replace this line with reviewed release notes before promotion.\n'
        notes = notes[: marker.end()] + draft + notes[marker.end() :]
        changelog.write_text(notes)
    return version


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--header", required=True, type=Path)
    parser.add_argument("--changelog", required=True, type=Path)
    parser.add_argument("--bump", required=True, choices=("patch", "minor", "major"))
    parser.add_argument("--version-json", type=Path)
    args = parser.parse_args()
    version = prepare(args.header, args.changelog, args.bump, args.version_json)
    output = os.environ.get("GITHUB_OUTPUT")
    if output:
        with open(output, "a", encoding="utf-8") as handle:
            handle.write(f"version={version}\n")
    print(version)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
