#!/usr/bin/env python3
"""Download a validation-gated immutable release for an explicit legacy alias."""

from __future__ import annotations

import argparse
import base64
import hashlib
import json
import os
import re
import subprocess
import tempfile
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any

CONTROL_PLANE = "https://dashboard.researchanddesire.com"
REQUIRED_GATES = {"build", "unit", "integration", "artifact", "hardware"}
PROVENANCE_KEYS = {
    "rd-fw-staging-2026-08-01": ("staging", """-----BEGIN PUBLIC KEY-----
MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEYb/VGPjyRATcddhxhnNs6IZYPSVM
7GKJFcCpnZgNEYj4z7N897PnOo/KMfyhE5xny9Kl5nJDxEDw0P12xkj75w==
-----END PUBLIC KEY-----
"""),
    "rd-fw-production-2026-08-01": ("main", """-----BEGIN PUBLIC KEY-----
MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEaXWHINx1VmUw0eMf5xCX5T5w24IZ
9K31OGTOy/s37oE7lbPyJanjrVq2u4DOqGLWc6YgSTwekFKO8VXq+GW9Cw==
-----END PUBLIC KEY-----
"""),
}


def decode_base64url(value: str) -> bytes:
    if not re.fullmatch(r"[A-Za-z0-9_-]+", value):
        raise RuntimeError("provenance contains malformed base64url")
    return base64.urlsafe_b64decode(value + "=" * (-len(value) % 4))


def p1363_to_der(signature: bytes) -> bytes:
    if len(signature) != 64:
        raise RuntimeError("provenance signature must be 64 bytes")
    encoded = []
    for integer in (signature[:32], signature[32:]):
        integer = integer.lstrip(b"\0") or b"\0"
        if integer[0] & 0x80:
            integer = b"\0" + integer
        encoded.append(b"\x02" + bytes([len(integer)]) + integer)
    body = b"".join(encoded)
    return b"\x30" + bytes([len(body)]) + body


def verify_provenance(token: str, track: str) -> dict[str, Any]:
    parts = token.split(".")
    if len(parts) != 3 or not all(parts):
        raise RuntimeError("release provenance is not a compact JWS")
    header = json.loads(decode_base64url(parts[0]))
    claims = json.loads(decode_base64url(parts[1]))
    key = PROVENANCE_KEYS.get(header.get("kid"))
    if (header.get("alg"), header.get("typ")) != ("ES256", "rad-fw-prov+jws"):
        raise RuntimeError("release provenance has an unsupported header")
    if key is None or key[0] != track or claims.get("track") != track:
        raise RuntimeError("release provenance key does not match the track")
    with tempfile.TemporaryDirectory(prefix="rad-fw-verify-") as directory:
        directory_path = Path(directory)
        public_path = directory_path / "public.pem"
        signature_path = directory_path / "signature.der"
        public_path.write_text(key[1], encoding="utf-8")
        signature_path.write_bytes(p1363_to_der(decode_base64url(parts[2])))
        result = subprocess.run(
            ["openssl", "dgst", "-sha256", "-verify", str(public_path),
             "-signature", str(signature_path)],
            input=f"{parts[0]}.{parts[1]}".encode(),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
    if result.returncode != 0:
        raise RuntimeError("release provenance signature is invalid")
    return claims


def runtime_image_sha256(content: bytes) -> str:
    if len(content) < 56 or content[0] != 0xE9:
        raise RuntimeError("application is not an ESP app image")
    if content[23] != 1:
        return hashlib.sha256(content).hexdigest()
    expected = content[-32:]
    if hashlib.sha256(content[:-32]).digest() != expected:
        raise RuntimeError("application has an invalid appended image digest")
    return expected.hex()


def legacy_version_document(
    version: str, build_sha: str, release_id: str
) -> dict[str, Any]:
    """Build the metadata shape consumed by every legacy OSSM updater."""
    match = re.fullmatch(r"(\d+)\.(\d+)\.(\d+)", version)
    if match is None:
        raise RuntimeError(
            f"release version is not numeric semantic version: {version}"
        )
    major, minor, patch = (int(part) for part in match.groups())
    return {
        "version": version,
        "major": major,
        "minor": minor,
        "patch": patch,
        "buildSha": build_sha,
        "releaseId": release_id,
    }


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
    provenance = status.get("provenance") or {}
    provenance_token = provenance.get("compact_jws", "")
    claims = verify_provenance(provenance_token, track)
    if (
        claims.get("schema") != "rad.firmware.provenance.v1"
        or claims.get("issuer") != "research-and-desire"
        or claims.get("deviceType") != device
        or claims.get("version") != status.get("version")
        or claims.get("buildSha") != build_sha
        or claims.get("manifestSha256") != hashlib.sha256(manifest_bytes).hexdigest()
    ):
        raise RuntimeError("provenance claims do not match the release")
    output.mkdir(parents=True, exist_ok=True)
    (output / "manifest.json").write_bytes(manifest_bytes)
    object_base = manifest_url.rsplit("/", 1)[0]
    saw_application = False
    for artifact in manifest.get("artifacts", []):
        filename = artifact.get("filename", "")
        if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]*", filename):
            raise RuntimeError("manifest contains an unsafe filename")
        content = fetch(f"{object_base}/{filename}")
        if len(content) != artifact.get("sizeBytes"):
            raise RuntimeError(f"size mismatch for {filename}")
        if hashlib.sha256(content).hexdigest() != artifact.get("sha256"):
            raise RuntimeError(f"SHA-256 mismatch for {filename}")
        if artifact.get("role") == "application":
            saw_application = True
            if (
                claims.get("applicationSha256") != artifact.get("sha256")
                or claims.get("applicationSizeBytes") != len(content)
                or claims.get("runtimeImageSha256") != runtime_image_sha256(content)
            ):
                raise RuntimeError("application does not match its provenance claims")
        (output / filename).write_bytes(content)
    if not saw_application:
        raise RuntimeError("manifest has no application artifact")
    (output / "provenance.json").write_text(
        json.dumps({"provenance": provenance_token}, separators=(",", ":")) + "\n"
    )
    (output / "version.json").write_text(
        json.dumps(
            legacy_version_document(status["version"], build_sha, release_id),
            indent=2,
        )
        + "\n"
    )
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
