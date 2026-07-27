import argparse
import tempfile
import unittest
from pathlib import Path

from publish_immutable_firmware import (
    compatibility_rules,
    parse_artifact,
    positive_int,
    read_version,
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


if __name__ == "__main__":
    unittest.main()
