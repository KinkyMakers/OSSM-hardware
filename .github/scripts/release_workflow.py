#!/usr/bin/env python3
"""Small, unit-tested helpers shared by release workflows."""

from __future__ import annotations

import argparse
import json
import os
import re
from pathlib import Path
from typing import Any
from urllib.parse import urlsplit

PROJECT_REFS = {
    "main": "acjajruwevyyatztbkdf",
    "staging": "meuaxbjzqrszxdvmacug",
}
REQUIRED_GATES = {"build", "unit", "integration", "artifact", "hardware"}


def branch_configuration(branch: str) -> dict[str, str]:
    if branch == "main":
        return {
            "TRACK": "main",
            "PIO_ENV": "production",
            "PIO_ENV_4MB": "production_4mb",
            "STORAGE_PROJECT_REF": PROJECT_REFS["main"],
        }
    if branch == "staging":
        return {
            "TRACK": "staging",
            "PIO_ENV": "staging",
            "PIO_ENV_4MB": "staging_4mb",
            "STORAGE_PROJECT_REF": PROJECT_REFS["staging"],
        }
    raise ValueError("firmware publication is restricted to main or staging")


def validate_release_pair(
    release_4mb: dict[str, Any], release_16mb: dict[str, Any]
) -> dict[str, str]:
    """Require independent persisted gates before tagging one dual-variant version."""
    for variant, release in (("v1", release_4mb), ("v2", release_16mb)):
        if (
            release.get("track") != "main"
            or release.get("deviceType") != "ossm"
            or release.get("hardwareVariant") != variant
        ):
            raise ValueError(f"{variant} release identity does not match")
        if (
            release.get("lifecycle") != "ready"
            or release.get("paused") is not True
            or release.get("rolloutPercentage") != 0
        ):
            raise ValueError(f"{variant} release is not ready, paused, and held at zero rollout")
        sha = release.get("buildSha", "")
        if not re.fullmatch(r"[0-9a-f]{40}", sha):
            raise ValueError(f"{variant} release must identify an exact commit SHA")
        if not re.fullmatch(r"\d+\.\d+\.\d+", release.get("version", "")):
            raise ValueError(f"{variant} release version is invalid")
        passed = {
            result.get("layer")
            for result in release.get("validations", [])
            if result.get("status") == "success" and result.get("repository_sha") == sha
        }
        if not REQUIRED_GATES.issubset(passed):
            raise ValueError(f"{variant} release is missing successful persisted gates for its commit")
        manifest = release.get("manifestUrl", "")
        url = urlsplit(manifest)
        if url.scheme != "https" or not url.netloc or any(c in manifest for c in "\r\n"):
            raise ValueError(f"{variant} release manifest URL is invalid")
    if any(release_4mb[key] != release_16mb[key] for key in ("version", "buildSha")):
        raise ValueError("both variants must share the same version and commit SHA")
    if release_4mb["manifestUrl"] == release_16mb["manifestUrl"]:
        raise ValueError("hardware variants must have distinct immutable manifests")
    return {
        "version": release_16mb["version"],
        "build_sha": release_16mb["buildSha"],
        "manifest_url_4mb": release_4mb["manifestUrl"],
        "manifest_url_16mb": release_16mb["manifestUrl"],
    }


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
    pair = commands.add_parser("validate-pair")
    pair.add_argument("--release-4mb", required=True, type=Path)
    pair.add_argument("--release-16mb", required=True, type=Path)
    tag = commands.add_parser("tag-action")
    tag.add_argument("--existing-sha", default="")
    tag.add_argument("--candidate-sha", required=True)
    args = parser.parse_args()
    if args.command == "configure":
        values = branch_configuration(args.branch)
        append_github_env(values)
        return 0
    if args.command == "validate-pair":
        values = validate_release_pair(
            json.loads(args.release_4mb.read_text()),
            json.loads(args.release_16mb.read_text()),
        )
        output = os.environ.get("GITHUB_OUTPUT")
        if output:
            with Path(output).open("a", encoding="utf-8") as handle:
                for key, value in values.items():
                    handle.write(f"{key}={value}\n")
        print(json.dumps(values, sort_keys=True))
        return 0
    print(tag_action(args.existing_sha, args.candidate_sha))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
