#!/usr/bin/env python3

import json
import tempfile
import unittest
from pathlib import Path

from release_version import bump_type, is_auto_commit, read_version, write_version

HEADER = """#ifndef VERSION_H
#define VERSION_H
#define VERSION "1.2.3"
#define MAJOR_VERSION 1
#define MINOR_VERSION 2
#define PATCH_VERSION 3
#endif
"""


class ReleaseVersionTests(unittest.TestCase):
    def test_title_classification(self):
        self.assertEqual(bump_type("Fix a minor display issue"), "minor")
        self.assertEqual(bump_type("[MAJOR] protocol redesign"), "major")
        self.assertEqual(bump_type("Routine bug fix"), "patch")
        self.assertEqual(bump_type("minority report"), "patch")
        with self.assertRaisesRegex(ValueError, "both major and minor"):
            bump_type("Major and minor")

    def test_auto_commit_is_track_specific(self):
        message = "AUTO: Version bump staging 1.2.4"
        self.assertTrue(is_auto_commit(message, "staging"))
        self.assertFalse(is_auto_commit(message, "main"))
        self.assertFalse(is_auto_commit(f"{message}\nnot generated", "staging"))

    def test_write_updates_header_and_json(self):
        with tempfile.TemporaryDirectory() as directory:
            header = Path(directory) / "Version.h"
            metadata = Path(directory) / "version.json"
            header.write_text(HEADER)
            write_version(header, "2.0.0", metadata)
            self.assertEqual(read_version(header), "2.0.0")
            self.assertEqual(
                json.loads(metadata.read_text()),
                {"version": "2.0.0", "major": 2, "minor": 0, "patch": 0},
            )


if __name__ == "__main__":
    unittest.main()
