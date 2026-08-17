#!/usr/bin/env python3
"""Publish a verified immutable firmware candidate through the RAD control plane."""

from __future__ import annotations

import argparse
import base64
from datetime import datetime, timezone
import hashlib
import json
import os
import re
import subprocess
import sys
import time
import urllib.error
import urllib.request
from dataclasses import dataclass
from pathlib import Path
from typing import Any

CONTROL_PLANE = "https://dashboard.researchanddesire.com"
PROJECT_REFS = {
    "main": "acjajruwevyyatztbkdf",
    "staging": "meuaxbjzqrszxdvmacug",
}
VALID_ROLES = {
    "application",
    "filesystem",
    "bootloader",
    "partitions",
    "web-installer",
}
PROVENANCE_KEYS = {
    "staging": ("rd-fw-staging-2026-08-01", "35fb9c22de8d69e3a1bb999c69f110b53b4879cd884af10b92cc137013b1c2cc"),
    "main": ("rd-fw-production-2026-08-01", "13f51408cd35f7925d9405e1b04d4e6ebdb57d2149f0eae522caaf3fc4d3aed6"),
}


@dataclass(frozen=True)
class Artifact:
    role: str
    path: Path
    install_order: int
    installable: bool

    @property
    def filename(self) -> str:
        return self.path.name

    @property
    def content(self) -> bytes:
        return self.path.read_bytes()

    @property
    def sha256(self) -> str:
        return hashlib.sha256(self.content).hexdigest()

    @property
    def size_bytes(self) -> int:
        return self.path.stat().st_size


def parse_artifact(value: str) -> Artifact:
    try:
        role, path, order, installable = value.split(":", 3)
    except ValueError as error:
        raise argparse.ArgumentTypeError(
            "artifact must be ROLE:PATH:ORDER:INSTALLABLE"
        ) from error
    if role not in VALID_ROLES:
        raise argparse.ArgumentTypeError(f"unsupported artifact role: {role}")
    artifact = Artifact(role, Path(path), int(order), installable.lower() == "true")
    if not artifact.path.is_file() or artifact.size_bytes == 0:
        raise argparse.ArgumentTypeError(f"artifact is missing or empty: {artifact.path}")
    return artifact


def read_version(path: Path) -> str:
    match = re.search(r'^#define VERSION "(\d+\.\d+\.\d+)"$', path.read_text(), re.M)
    if not match:
        raise ValueError(f"unable to read semantic version from {path}")
    return match.group(1)


def positive_int(value: str) -> int:
    parsed = int(value)
    if parsed <= 0:
        raise argparse.ArgumentTypeError("value must be a positive integer")
    return parsed


def compatibility_rules(min_flash_size_bytes: int | None) -> list[dict[str, int]]:
    return (
        [{"minFlashSizeBytes": min_flash_size_bytes}]
        if min_flash_size_bytes is not None
        else []
    )


def json_bytes(value: Any) -> bytes:
    return (json.dumps(value, sort_keys=True, separators=(",", ":")) + "\n").encode()


def base64url(value: bytes) -> str:
    return base64.urlsafe_b64encode(value).rstrip(b"=").decode("ascii")


def read_der_length(value: bytes, offset: int) -> tuple[int, int]:
    length = value[offset]
    if length < 0x80:
        return length, offset + 1
    count = length & 0x7F
    if count == 0 or count > 2:
        raise RuntimeError("unsupported ECDSA DER length")
    return int.from_bytes(value[offset + 1 : offset + 1 + count], "big"), offset + 1 + count


def der_ecdsa_to_p1363(signature: bytes) -> bytes:
    if not signature or signature[0] != 0x30:
        raise RuntimeError("invalid ECDSA signature encoding")
    sequence_length, offset = read_der_length(signature, 1)
    if offset + sequence_length != len(signature):
        raise RuntimeError("invalid ECDSA signature length")
    values: list[bytes] = []
    for _ in range(2):
        if offset >= len(signature) or signature[offset] != 0x02:
            raise RuntimeError("invalid ECDSA signature integer")
        integer_length, offset = read_der_length(signature, offset + 1)
        integer = signature[offset : offset + integer_length]
        offset += integer_length
        integer = integer.lstrip(b"\x00")
        if len(integer) > 32:
            raise RuntimeError("ECDSA signature integer is too large")
        values.append(integer.rjust(32, b"\x00"))
    return b"".join(values)


def sign_provenance(track: str, claims: dict[str, Any]) -> str:
    private_key = os.environ.get("FIRMWARE_PROVENANCE_SIGNING_KEY_PEM", "").strip()
    if not private_key:
        raise RuntimeError("FIRMWARE_PROVENANCE_SIGNING_KEY_PEM is required")
    key_id, expected_fingerprint = PROVENANCE_KEYS[track]
    public_der = subprocess.run(
        ["openssl", "pkey", "-pubout", "-outform", "DER"],
        input=private_key.encode(),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=True,
    ).stdout
    if hashlib.sha256(public_der).hexdigest() != expected_fingerprint:
        raise RuntimeError("firmware provenance signing key does not match the selected track")
    header = {"alg": "ES256", "kid": key_id, "typ": "rad-fw-prov+jws"}
    signing_input = (
        f"{base64url(json.dumps(header, sort_keys=True, separators=(',', ':')).encode())}."
        f"{base64url(json.dumps(claims, sort_keys=True, separators=(',', ':')).encode())}"
    )
    import tempfile

    with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", prefix="rad-fw-key-", delete=True) as key_file:
        os.chmod(key_file.name, 0o600)
        key_file.write(private_key)
        key_file.flush()
        signature = subprocess.run(
            ["openssl", "dgst", "-sha256", "-sign", key_file.name],
            input=signing_input.encode(),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=True,
        ).stdout
    return f"{signing_input}.{base64url(der_ecdsa_to_p1363(signature))}"


def runtime_image_sha256(application: Artifact) -> str:
    content = application.content
    if len(content) < 56 or content[0] != 0xE9:
        raise RuntimeError("application is not an ESP app image")
    if content[23] != 1:
        return hashlib.sha256(content).hexdigest()
    expected = content[-32:]
    if hashlib.sha256(content[:-32]).digest() != expected:
        raise RuntimeError("application has an invalid appended image digest")
    return expected.hex()


def create_provenance(args: argparse.Namespace, version: str, manifest: Artifact, application: Artifact, path: Path, order: int) -> tuple[Artifact, str]:
    claims = {
        "schema": "rad.firmware.provenance.v1",
        "issuer": "research-and-desire",
        "track": args.track,
        "deviceType": args.device_type,
        "hardwareVariant": getattr(args, "hardware_variant", "default"),
        "kind": args.kind,
        "version": version,
        "buildSha": args.build_sha,
        "manifestSha256": manifest.sha256,
        "applicationSha256": application.sha256,
        "runtimeImageSha256": runtime_image_sha256(application),
        "applicationSizeBytes": application.size_bytes,
        "issuedAt": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
    }
    token = sign_provenance(args.track, claims)
    path.write_bytes(json_bytes({"provenance": token}))
    return Artifact("provenance", path, order, False), token


def request_json(url: str, token: str, payload: dict[str, Any]) -> dict[str, Any]:
    request = urllib.request.Request(
        url,
        data=json_bytes(payload),
        headers={
            "Authorization": f"Bearer {token}",
            "Content-Type": "application/json",
            "User-Agent": "rad-firmware-publisher/1",
        },
        method="POST",
    )
    try:
        with urllib.request.urlopen(request, timeout=120) as response:
            return json.load(response)
    except urllib.error.HTTPError as error:
        detail = error.read().decode(errors="replace")
        raise RuntimeError(f"control plane returned HTTP {error.code}: {detail}") from error


def upload_file(url: str, content: bytes, content_type: str) -> None:
    request = urllib.request.Request(
        url,
        data=content,
        headers={"Content-Type": content_type, "x-upsert": "false"},
        method="PUT",
    )
    with urllib.request.urlopen(request, timeout=120) as response:
        if response.status not in (200, 201):
            raise RuntimeError(f"signed upload returned HTTP {response.status}")


def verify_public_object(url: str, sha256: str, size_bytes: int) -> None:
    last_error: Exception | None = None
    for attempt in range(5):
        try:
            with urllib.request.urlopen(url, timeout=120) as response:
                content = response.read()
            if len(content) != size_bytes:
                raise RuntimeError(f"size mismatch for {url}")
            if hashlib.sha256(content).hexdigest() != sha256:
                raise RuntimeError(f"SHA-256 mismatch for {url}")
            return
        except (OSError, RuntimeError, urllib.error.URLError) as error:
            last_error = error
            time.sleep(2**attempt)
    raise RuntimeError(f"public object verification failed: {last_error}")


def validation_payload(release_id: str, layer: str, status: str, args: argparse.Namespace) -> dict[str, Any]:
    repository = os.environ.get("GITHUB_REPOSITORY", "")
    run_id = os.environ.get("GITHUB_RUN_ID", "")
    workflow_url = f"https://github.com/{repository}/actions/runs/{run_id}"
    payload: dict[str, Any] = {
        "releaseId": release_id,
        "layer": layer,
        "deviceType": args.device_type,
        "repositorySha": args.build_sha,
        "status": status,
        "runner": os.environ.get("RUNNER_NAME", "github-actions"),
    }
    if repository and run_id:
        payload["workflowRunUrl"] = workflow_url
    return payload


def select_release_artifacts(
    artifacts: list[Artifact],
) -> tuple[list[Artifact], list[Artifact]]:
    if len({artifact.filename for artifact in artifacts}) != len(artifacts):
        raise RuntimeError("artifact filenames must be unique")
    if len({artifact.install_order for artifact in artifacts}) != len(artifacts):
        raise RuntimeError("artifact install orders must be unique")
    installable = sorted(
        (artifact for artifact in artifacts if artifact.installable),
        key=lambda artifact: artifact.install_order,
    )
    applications = [
        artifact for artifact in installable if artifact.role == "application"
    ]
    if len(applications) != 1:
        raise RuntimeError("exactly one installable application artifact is required")
    web_installers = [
        artifact for artifact in artifacts if artifact.role == "web-installer"
    ]
    if len(web_installers) != 1 or any(
        artifact.installable for artifact in web_installers
    ):
        raise RuntimeError(
            "exactly one non-installable web-installer artifact is required"
        )
    if any(
        artifact.role not in {"application", "filesystem"}
        for artifact in installable
    ):
        raise RuntimeError("only application and filesystem may be installable")
    return installable, sorted(
        [*installable, *web_installers], key=lambda artifact: artifact.install_order
    )


def upload_request_payload(
    args: argparse.Namespace, version: str, artifacts: list[Artifact]
) -> dict[str, Any]:
    return {
        "track": args.track,
        "deviceType": args.device_type,
        "version": version,
        "buildSha": args.build_sha,
        "kind": args.kind,
        "storageProjectRef": PROJECT_REFS[args.track],
        "bucketId": f"{args.device_type}-firmware",
        "artifacts": [
            {
                "role": artifact.role,
                "filename": artifact.filename,
                "sha256": artifact.sha256,
                "sizeBytes": artifact.size_bytes,
                "installOrder": artifact.install_order,
                "contentType": "application/json"
                if artifact.path.suffix == ".json"
                else "application/octet-stream",
            }
            for artifact in artifacts
        ],
    }


def publish(args: argparse.Namespace) -> str:
    token = os.environ.get("FIRMWARE_PUBLISH_TOKEN", "").strip()
    if not token:
        raise RuntimeError("FIRMWARE_PUBLISH_TOKEN is required")
    if args.track not in PROJECT_REFS:
        raise RuntimeError("track must be main or staging")
    if not re.fullmatch(r"[0-9a-fA-F]{7,64}", args.build_sha):
        raise RuntimeError("build SHA must contain 7 to 64 hexadecimal characters")
    version = read_version(args.version_file)
    installable, release_artifacts = select_release_artifacts(args.artifact)

    metadata = [
        {
            "role": artifact.role,
            "filename": artifact.filename,
            "sha256": artifact.sha256,
            "sizeBytes": artifact.size_bytes,
            "installOrder": artifact.install_order,
            "installable": artifact.installable,
        }
        for artifact in sorted(args.artifact, key=lambda item: item.install_order)
    ]
    generated_dir = Path(os.environ.get("RUNNER_TEMP", ".pio")) / "firmware-release"
    generated_dir.mkdir(parents=True, exist_ok=True)
    manifest_path = generated_dir / "manifest.json"
    release_path = generated_dir / "release.json"
    manifest_path.write_bytes(
        json_bytes(
            {
                "protocolVersion": 1,
                "deviceType": args.device_type,
                "track": args.track,
                "version": version,
                "buildSha": args.build_sha,
                "artifacts": metadata,
            }
        )
    )
    release_path.write_bytes(
        json_bytes(
            {
                "protocolVersion": 1,
                "deviceType": args.device_type,
                "track": args.track,
                "version": version,
                "buildSha": args.build_sha,
                "kind": args.kind,
                "manifest": "manifest.json",
            }
        )
    )
    generated_order = max(item.install_order for item in args.artifact)
    manifest_artifact = Artifact("manifest", manifest_path, generated_order + 1, False)
    release_artifact = Artifact("release", release_path, generated_order + 2, False)
    application = next(item for item in installable if item.role == "application")
    provenance_artifact, provenance_token = create_provenance(
        args, version, manifest_artifact, application,
        generated_dir / "provenance.json", generated_order + 3,
    )
    generated = [manifest_artifact, release_artifact, provenance_artifact]
    release_artifacts = [*release_artifacts, provenance_artifact]
    all_artifacts = [*args.artifact, *generated]
    upload_request = upload_request_payload(args, version, all_artifacts)
    base_url = os.environ.get("FIRMWARE_CONTROL_PLANE_BASE_URL", CONTROL_PLANE).rstrip("/")
    upload_base_url = os.environ.get(
        "FIRMWARE_UPLOAD_BASE_URL",
        "https://staging.researchanddesire.com" if args.track == "staging" else base_url,
    ).rstrip("/")
    signed = request_json(f"{upload_base_url}/api/internal/firmware/v1/uploads", token, upload_request)
    by_name = {artifact.filename: artifact for artifact in all_artifacts}
    for upload in signed["uploads"]:
        artifact = by_name[upload["filename"]]
        content_type = "application/json" if artifact.path.suffix == ".json" else "application/octet-stream"
        upload_file(upload["signedUrl"], artifact.content, content_type)
        verify_public_object(upload["publicUrl"], artifact.sha256, artifact.size_bytes)

    uploads_by_name = {upload["filename"]: upload for upload in signed["uploads"]}
    release_request = {
        "track": args.track,
        "deviceType": args.device_type,
        "version": version,
        "buildSha": args.build_sha,
        "kind": args.kind,
        "storageProjectRef": PROJECT_REFS[args.track],
        "bucketId": f"{args.device_type}-firmware",
        "objectPrefix": signed["objectPrefix"],
        "compatibilityRules": compatibility_rules(args.min_flash_size_bytes),
        "provenance": provenance_token,
        "artifacts": [
            {
                "role": artifact.role,
                "objectPath": uploads_by_name[artifact.filename]["objectPath"],
                "publicUrl": uploads_by_name[artifact.filename]["publicUrl"],
                "sha256": artifact.sha256,
                "sizeBytes": artifact.size_bytes,
                "required": artifact.installable,
                "installOrder": artifact.install_order,
            }
            for artifact in release_artifacts
        ],
    }
    published = request_json(f"{base_url}/api/internal/firmware/v1/releases", token, release_request)
    release_id = published["releaseId"]
    try:
        for layer in ("build", "unit", "integration"):
            request_json(
                f"{base_url}/api/internal/firmware/v1/validations",
                token,
                validation_payload(release_id, layer, "success", args),
            )
    except Exception:
        request_json(
            f"{base_url}/api/internal/firmware/v1/validations",
            token,
            validation_payload(release_id, "integration", "failed", args),
        )
        raise
    output = os.environ.get("GITHUB_OUTPUT")
    if output:
        with open(output, "a", encoding="utf-8") as handle:
            handle.write(f"release_id={release_id}\n")
    return release_id


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--device-type", required=True, choices=("dtt", "lkbx", "ossm", "radr"))
    parser.add_argument("--track", required=True, choices=("main", "staging"))
    parser.add_argument("--build-sha", required=True)
    parser.add_argument("--version-file", required=True, type=Path)
    parser.add_argument("--kind", default="firmware", choices=("firmware", "migration"))
    parser.add_argument("--min-flash-size-bytes", type=positive_int)
    parser.add_argument("--artifact", action="append", required=True, type=parse_artifact)
    args = parser.parse_args()
    try:
        release_id = publish(args)
        print(f"Published validating release {release_id}")
        return 0
    except Exception as error:
        print(f"Publication failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
