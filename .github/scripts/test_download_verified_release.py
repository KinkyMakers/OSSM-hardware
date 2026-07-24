import unittest

from download_verified_release import legacy_version_document


class LegacyVersionDocumentTests(unittest.TestCase):
    def test_includes_fields_required_by_legacy_updaters(self):
        document = legacy_version_document("1.2.34", "a" * 40, "release-id")

        self.assertEqual(
            document,
            {
                "version": "1.2.34",
                "major": 1,
                "minor": 2,
                "patch": 34,
                "buildSha": "a" * 40,
                "releaseId": "release-id",
            },
        )

    def test_rejects_non_numeric_semantic_version(self):
        for version in ("1.2", "v1.2.3", "1.2.3-beta"):
            with self.subTest(version=version), self.assertRaises(RuntimeError):
                legacy_version_document(version, "a" * 40, "release-id")


if __name__ == "__main__":
    unittest.main()
