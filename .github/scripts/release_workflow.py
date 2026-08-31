#!/usr/bin/env python3
"""Small, unit-tested helpers shared by release workflows."""

from __future__ import annotations

import argparse
import os
import re
from pathlib import Path

PROJECT_REFS = {
    "main": "acjajruwevyyatztbkdf",
    "staging": "meuaxbjzqrszxdvmacug",
}


def branch_configuration(branch: str) -> dict[str, str]:
    if branch == "main":
        return {
            "TRACK": "main",
            "PIO_ENV": "production",
            "STORAGE_PROJECT_REF": PROJECT_REFS["main"],
        }
    if branch == "staging":
        return {
            "TRACK": "staging",
            "PIO_ENV": "staging",
            "STORAGE_PROJECT_REF": PROJECT_REFS["staging"],
        }
    raise ValueError("firmware publication is restricted to main or staging")


def tag_action(existing_sha: str | None, candidate_sha: str) -> str:
    candidate = candidate_sha.strip().lower()
    existing = (existing_sha or "").strip().lower()
    if not re.fullmatch(r"[0-9a-f]{7,64}", candidate):
        raise ValueError("candidate SHA is invalid")
    if not existing:
        return "create"
    if not re.fullmatch(r"[0-9a-f]{7,64}", existing):
        raise ValueError("existing tag SHA is invalid")
    if existing != candidate:
        raise ValueError("tag already targets a different commit")
    return "exists"


def append_github_env(values: dict[str, str]) -> None:
    path = os.environ.get("GITHUB_ENV")
    if not path:
        raise RuntimeError("GITHUB_ENV is required")
    with Path(path).open("a", encoding="utf-8") as handle:
        for key, value in values.items():
            handle.write(f"{key}={value}\n")


def main() -> int:
    parser = argparse.ArgumentParser()
    commands = parser.add_subparsers(dest="command", required=True)
    configure = commands.add_parser("configure")
    configure.add_argument("--branch", required=True)
    tag = commands.add_parser("tag-action")
    tag.add_argument("--existing-sha", default="")
    tag.add_argument("--candidate-sha", required=True)
    args = parser.parse_args()
    if args.command == "configure":
        values = branch_configuration(args.branch)
        append_github_env(values)
        return 0
    print(tag_action(args.existing_sha, args.candidate_sha))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
