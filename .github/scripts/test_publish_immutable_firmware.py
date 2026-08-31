import argparse
import tempfile
import unittest
from pathlib import Path

from publish_immutable_firmware import (
    Artifact,
    ControlPlaneHttpError,
    complete_release_artifacts,
    compatibility_rules,
    parse_artifact,
    positive_int,
    read_version,
    requires_legacy_production_envelope,
    select_release_artifacts,
    upload_request_payload,
)


class PublisherTests(unittest.TestCase):
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
