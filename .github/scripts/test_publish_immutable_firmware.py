import argparse
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from publish_immutable_firmware import (
    Artifact,
    ControlPlaneHttpError,
    complete_release_artifacts,
    compatibility_rules,
    parse_artifact,
    positive_int,
    publish,
    read_version,
    requires_legacy_production_envelope,
    select_release_artifacts,
    upload_request_payload,
)


class PublisherTests(unittest.TestCase):
    def test_same_version_variants_keep_distinct_artifacts_provenance_and_gates(self):
        requests = []
        provenance_claims = []

        def control_plane(url, token, payload):
            requests.append((url, payload))
            if url.endswith("/uploads"):
                prefix = f"releases/{payload['hardwareVariant']}/{payload['version']}/{payload['buildSha']}"
                return {
                    "objectPrefix": prefix,
                    "uploads": [
                        {"filename": artifact["filename"],
                         "signedUrl": f"https://example.invalid/upload/{prefix}/{artifact['filename']}",
                         "publicUrl": f"https://example.invalid/{prefix}/{artifact['filename']}",
                         "objectPath": f"{prefix}/{artifact['filename']}"}
                        for artifact in payload["artifacts"]
                    ],
                }
            if url.endswith("/releases"):
                return {"releaseId": f"release-{payload['hardwareVariant']}"}
            return {}

        def sign(track, claims):
            provenance_claims.append(claims)
            return f"test-provenance-{claims['hardwareVariant']}"

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            header = root / "Version.h"
            header.write_text('#define VERSION "1.2.3"\n')
            with patch.dict("os.environ", {"RUNNER_TEMP": directory,
                            "FIRMWARE_PUBLISH_TOKEN": "test-only", "GITHUB_OUTPUT": str(root / "outputs")}), \
                    patch("publish_immutable_firmware.request_json", side_effect=control_plane), \
                    patch("publish_immutable_firmware.sign_provenance", side_effect=sign), \
                    patch("publish_immutable_firmware.upload_file"), \
                    patch("publish_immutable_firmware.verify_public_object"):
                for variant, capacity in (("v1", 4 * 1024 * 1024), ("v2", 16 * 1024 * 1024)):
                    inputs = root / variant
                    inputs.mkdir()
                    application = inputs / "firmware.bin"
                    application.write_bytes(b"\xe9" + b"\0" * 55 + variant.encode())
                    installer = inputs / "web-installer.bin"
                    installer.write_bytes(b"installer-" + variant.encode())
                    args = argparse.Namespace(
                        track="main", device_type="ossm", hardware_variant=variant,
                        build_sha="a" * 40, kind="firmware", version_file=header,
                        min_flash_size_bytes=capacity,
                        artifact=[Artifact("application", application, 1, True),
                                  Artifact("web-installer", installer, 4, False)],
                    )
                    self.assertEqual(publish(args), f"release-{variant}")
            manifests = list((root / "firmware-release").rglob("manifest.json"))
            provenances = list((root / "firmware-release").rglob("provenance.json"))
            self.assertEqual(len(manifests), 2)
            self.assertEqual(len(provenances), 2)
            self.assertEqual({json.loads(path.read_text())["hardwareVariant"] for path in manifests}, {"v1", "v2"})

        releases = [payload for url, payload in requests if url.endswith("/releases")]
        self.assertEqual({release["version"] for release in releases}, {"1.2.3"})
        self.assertEqual({release["buildSha"] for release in releases}, {"a" * 40})
        self.assertNotEqual(releases[0]["objectPrefix"], releases[1]["objectPrefix"])
        self.assertEqual([r["compatibilityRules"] for r in releases],
                         [[{"minFlashSizeBytes": 4194304}], [{"minFlashSizeBytes": 16777216}]])
        self.assertEqual({claims["hardwareVariant"] for claims in provenance_claims}, {"v1", "v2"})
        self.assertNotEqual(provenance_claims[0]["applicationSha256"], provenance_claims[1]["applicationSha256"])
        self.assertNotEqual(provenance_claims[0]["manifestSha256"], provenance_claims[1]["manifestSha256"])
        validations = [payload for url, payload in requests if url.endswith("/validations")]
        for variant in ("v1", "v2"):
            self.assertEqual({v["layer"] for v in validations if v["releaseId"] == f"release-{variant}"},
                             {"build", "unit", "integration"})
        self.assertNotIn("hardware", {v["layer"] for v in validations})

    def test_reads_numeric_semantic_version(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "Version.h"
            path.write_text('#define VERSION "1.2.3"\n')
            self.assertEqual(read_version(path), "1.2.3")

    def test_rejects_missing_artifact(self):
        with self.assertRaises(argparse.ArgumentTypeError):
            parse_artifact("application:missing.bin:1:true")

    def test_parses_installable_artifact(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "firmware.bin"
            path.write_bytes(b"firmware")
            artifact = parse_artifact(f"application:{path}:2:true")
            self.assertTrue(artifact.installable)
            self.assertEqual(artifact.install_order, 2)
            self.assertEqual(artifact.sha256, "c3bf47ea1f4a4a605470313cacb3a44f4a461f68c6faeab07e737610cb5ac835")

    def test_adds_physical_flash_compatibility_rule(self):
        self.assertEqual(
            compatibility_rules(positive_int("16777216")),
            [{"minFlashSizeBytes": 16_777_216}],
        )
        self.assertEqual(compatibility_rules(None), [])

    def test_rejects_non_positive_flash_size(self):
        with self.assertRaises(argparse.ArgumentTypeError):
            positive_int("0")

    def test_upload_request_includes_release_kind(self):
        with tempfile.TemporaryDirectory() as directory:
            firmware = Path(directory) / "firmware.bin"
            firmware.write_bytes(b"firmware")
            payload = upload_request_payload(
                argparse.Namespace(
                    track="staging",
                    device_type="ossm",
                    hardware_variant="v2",
                    build_sha="abcdef0",
                    kind="firmware",
                ),
                "1.2.3",
                [Artifact("application", firmware, 1, True)],
            )
        self.assertEqual(payload["kind"], "firmware")
        self.assertEqual(payload["hardwareVariant"], "v2")

    def test_web_installer_is_published_but_not_installable(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            firmware = root / "firmware.bin"
            installer = root / "web-installer.bin"
            firmware.write_bytes(b"firmware")
            installer.write_bytes(b"installer")
            installable, published = select_release_artifacts(
                [
                    Artifact("application", firmware, 1, True),
                    Artifact("web-installer", installer, 4, False),
                ]
            )
        self.assertEqual([artifact.role for artifact in installable], ["application"])
        self.assertEqual(
            [artifact.role for artifact in published],
            ["application", "web-installer"],
        )

    def test_rejects_installable_web_installer(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            firmware = root / "firmware.bin"
            installer = root / "web-installer.bin"
            firmware.write_bytes(b"firmware")
            installer.write_bytes(b"installer")
            with self.assertRaisesRegex(RuntimeError, "non-installable"):
                select_release_artifacts(
                    [
                        Artifact("application", firmware, 1, True),
                        Artifact("web-installer", installer, 4, True),
                    ]
                )

    def test_requires_one_web_installer(self):
        with tempfile.TemporaryDirectory() as directory:
            firmware = Path(directory) / "firmware.bin"
            firmware.write_bytes(b"firmware")
            with self.assertRaisesRegex(RuntimeError, "exactly one"):
                select_release_artifacts(
                    [Artifact("application", firmware, 1, True)]
                )

    def test_release_includes_manifest_and_provenance(self):
        application = Artifact("application", Path("firmware.bin"), 1, True)
        manifest = Artifact("manifest", Path("manifest.json"), 5, False)
        provenance = Artifact("provenance", Path("provenance.json"), 7, False)
        self.assertEqual(
            [
                artifact.role
                for artifact in complete_release_artifacts(
                    [application], manifest, provenance
                )
            ],
            ["application", "manifest", "provenance"],
        )

    def test_legacy_envelope_is_limited_to_old_production_schema(self):
        old_schema = ControlPlaneHttpError(
            400,
            '{"issues":[{"code":"invalid_value","values":'
            '["application","manifest","release"],"path":'
            '["artifacts",6,"role"]}]}',
        )
        self.assertTrue(requires_legacy_production_envelope("main", old_schema))
        self.assertFalse(requires_legacy_production_envelope("staging", old_schema))
        self.assertFalse(
            requires_legacy_production_envelope(
                "main",
                ControlPlaneHttpError(
                    400,
                    '{"issues":[{"code":"invalid_value","values":'
                    '["application","provenance"],"path":'
                    '["artifacts",6,"role"]}]}',
                ),
            )
        )


if __name__ == "__main__":
    unittest.main()
