#!/usr/bin/env python3
"""Read and update the canonical firmware version used by release workflows."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

SEMVER = re.compile(r"^(\d+)\.(\d+)\.(\d+)$")
AUTO_COMMIT = re.compile(
    r"^AUTO: Version bump (?P<track>main|staging) \d+\.\d+\.\d+\n?\Z"
)


def read_version(header: Path) -> str:
    matches = re.findall(r'^#define VERSION "(\d+\.\d+\.\d+)"$', header.read_text(), re.M)
    if len(matches) != 1:
        raise ValueError(f"expected exactly one semantic VERSION in {header}")
    return matches[0]


def bump_type(title: str) -> str:
    has_major = re.search(r"\bmajor\b", title, re.I) is not None
    has_minor = re.search(r"\bminor\b", title, re.I) is not None
    if has_major and has_minor:
        raise ValueError("PR title cannot request both major and minor bumps")
    if has_major:
        return "major"
    if has_minor:
        return "minor"
    return "patch"


def is_auto_commit(message: str, track: str) -> bool:
    match = AUTO_COMMIT.match(message)
    return match is not None and match.group("track") == track


def replace_once(content: str, pattern: str, replacement: str, label: str) -> str:
    updated, count = re.subn(pattern, replacement, content, flags=re.M)
    if count != 1:
        raise ValueError(f"expected exactly one {label} definition")
    return updated


def write_version(header: Path, version: str, version_json: Path | None) -> None:
    match = SEMVER.fullmatch(version)
    if not match:
        raise ValueError("version must use X.Y.Z")
    major, minor, patch = match.groups()
    content = header.read_text()
    content = replace_once(content, r'^#define VERSION ".*"$', f'#define VERSION "{version}"', "VERSION")
    content = replace_once(content, r"^#define MAJOR_VERSION \d+$", f"#define MAJOR_VERSION {major}", "MAJOR_VERSION")
    content = replace_once(content, r"^#define MINOR_VERSION \d+$", f"#define MINOR_VERSION {minor}", "MINOR_VERSION")
    content = replace_once(content, r"^#define PATCH_VERSION \d+$", f"#define PATCH_VERSION {patch}", "PATCH_VERSION")
    header.write_text(content)
    if version_json is not None:
        version_json.write_text(
            json.dumps(
                {
                    "version": version,
                    "major": int(major),
                    "minor": int(minor),
                    "patch": int(patch),
                },
                indent=2,
            )
            + "\n"
        )


def main() -> int:
    parser = argparse.ArgumentParser()
    commands = parser.add_subparsers(dest="command", required=True)

    read = commands.add_parser("read")
    read.add_argument("--header", required=True, type=Path)

    classify = commands.add_parser("bump-type")
    classify.add_argument("--title", default="")

    automatic = commands.add_parser("is-auto-commit")
    automatic.add_argument("--message", required=True)
    automatic.add_argument("--track", required=True, choices=("main", "staging"))

    write = commands.add_parser("write")
    write.add_argument("--header", required=True, type=Path)
    write.add_argument("--version", required=True)
    write.add_argument("--version-json", type=Path)

    args = parser.parse_args()
    if args.command == "read":
        print(read_version(args.header))
        return 0
    if args.command == "bump-type":
        print(bump_type(args.title))
        return 0
    if args.command == "is-auto-commit":
        return 0 if is_auto_commit(args.message, args.track) else 1
    write_version(args.header, args.version, args.version_json)
    print(args.version)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
