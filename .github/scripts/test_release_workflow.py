import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from release_workflow import branch_configuration, main, tag_action


class ReleaseWorkflowTests(unittest.TestCase):
    def test_branch_configuration_maps_tracks_and_projects(self):
        self.assertEqual(branch_configuration("main")["PIO_ENV"], "production")
        self.assertEqual(branch_configuration("staging")["PIO_ENV"], "staging")
        self.assertNotEqual(
            branch_configuration("main")["STORAGE_PROJECT_REF"],
            branch_configuration("staging")["STORAGE_PROJECT_REF"],
        )
        with self.assertRaises(ValueError):
            branch_configuration("feature")

    def test_tag_action_is_idempotent_and_fails_on_collision(self):
        sha = "a" * 40
        self.assertEqual(tag_action("", sha), "create")
        self.assertEqual(tag_action(sha, sha), "exists")
        with self.assertRaisesRegex(ValueError, "different commit"):
            tag_action("b" * 40, sha)

    def test_configure_writes_github_environment(self):
        with tempfile.TemporaryDirectory() as directory:
            env_file = Path(directory) / "env"
            with patch.dict("os.environ", {"GITHUB_ENV": str(env_file)}), patch(
                "sys.argv", ["release_workflow.py", "configure", "--branch", "staging"]
            ):
                self.assertEqual(main(), 0)
            self.assertIn("TRACK=staging", env_file.read_text())


if __name__ == "__main__":
    unittest.main()
