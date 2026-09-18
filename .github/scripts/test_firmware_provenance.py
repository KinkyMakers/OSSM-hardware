import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import download_verified_release as downloader
import publish_immutable_firmware as publisher


FIXTURE = json.loads(
    (Path(__file__).with_name("firmware_provenance_test_vector.json")).read_text()
)


class FirmwareProvenanceTests(unittest.TestCase):
    def test_shared_vector_verifies(self):
        claims = downloader.verify_provenance(FIXTURE["compactJws"], "staging")
        self.assertEqual(claims, FIXTURE["claims"])

    def test_staging_vector_is_not_valid_for_production(self):
        with self.assertRaisesRegex(RuntimeError, "does not match the track"):
            downloader.verify_provenance(FIXTURE["compactJws"], "main")

    def test_malformed_and_truncated_signatures_fail(self):
        with self.assertRaises(RuntimeError):
            downloader.verify_provenance("!" + FIXTURE["compactJws"], "staging")
        with self.assertRaises(RuntimeError):
            downloader.verify_provenance(FIXTURE["compactJws"][:-20], "staging")

    def test_signing_secret_is_required(self):
        with mock.patch.dict(os.environ, {}, clear=True):
            with self.assertRaisesRegex(RuntimeError, "is required"):
                publisher.sign_provenance("staging", FIXTURE["claims"])

    def test_wrong_signing_key_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            key_path = Path(directory) / "wrong.pem"
            subprocess.run(
                [
                    "openssl",
                    "genpkey",
                    "-algorithm",
                    "EC",
                    "-pkeyopt",
                    "ec_paramgen_curve:P-256",
                    "-out",
                    str(key_path),
                ],
                check=True,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            with mock.patch.dict(
                os.environ,
                {"FIRMWARE_PROVENANCE_SIGNING_KEY_PEM": key_path.read_text()},
                clear=True,
            ):
                with self.assertRaisesRegex(RuntimeError, "does not match"):
                    publisher.sign_provenance("staging", FIXTURE["claims"])

    def test_runtime_hash_uses_validated_appended_digest(self):
        image = bytearray(64)
        image[0] = 0xE9
        image[23] = 1
        import hashlib

        image.extend(hashlib.sha256(image).digest())
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "firmware.bin"
            path.write_bytes(image)
            artifact = publisher.Artifact("application", path, 0, True)
            self.assertEqual(
                publisher.runtime_image_sha256(artifact),
                image[-32:].hex(),
            )
            image[-1] ^= 1
            path.write_bytes(image)
            with self.assertRaisesRegex(RuntimeError, "invalid appended"):
                publisher.runtime_image_sha256(artifact)

    def test_hardware_root_of_trust_features_remain_disabled(self):
        repository = Path(__file__).resolve().parents[2]
        configuration_paths = list(repository.rglob("platformio.ini"))
        configuration_paths.extend(repository.rglob("sdkconfig*"))
        configuration_paths = [
            path
            for path in configuration_paths
            if ".pio" not in path.parts and path.is_file()
        ]
        combined = "\n".join(
            path.read_text(errors="replace") for path in configuration_paths
        )
        forbidden = (
            "CONFIG_SECURE_BOOT=y",
            "CONFIG_SECURE_SIGNED_APPS_NO_SECURE_BOOT=y",
            "CONFIG_FLASH_ENCRYPTION_ENABLED=y",
        )
        for setting in forbidden:
            self.assertNotIn(setting, combined)


if __name__ == "__main__":
    unittest.main()
