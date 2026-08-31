import copy
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from release_workflow import branch_configuration, main, tag_action, validate_release_pair


class ReleaseWorkflowTests(unittest.TestCase):
    def test_publisher_uses_scoped_deploy_key_for_git_writes(self):
        workflow = (
            Path(__file__).resolve().parents[1] / "workflows" / "publish_firmware.yml"
        ).read_text()
        self.assertIn(
            "ssh-key: ${{ secrets.RAD_VERSION_CONTROL_DEPLOY_KEY }}", workflow
        )
        self.assertNotRegex(
            workflow, r"(?m)^\s+token:\s+\$\{\{ github\.token \}\}$"
        )
        self.assertIn("github-token: ${{ github.token }}", workflow)

    def test_branch_configuration_maps_tracks_and_projects(self):
        self.assertEqual(branch_configuration("main")["PIO_ENV"], "production")
        self.assertEqual(branch_configuration("main")["PIO_ENV_4MB"], "production_4mb")
        self.assertEqual(branch_configuration("staging")["PIO_ENV"], "staging")
        self.assertEqual(branch_configuration("staging")["PIO_ENV_4MB"], "staging_4mb")
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
            self.assertIn("PIO_ENV_4MB=staging_4mb", env_file.read_text())

    @staticmethod
    def release(variant):
        return {
            "track": "main", "deviceType": "ossm", "hardwareVariant": variant,
            "lifecycle": "ready", "paused": True, "rolloutPercentage": 0,
            "version": "1.2.3", "buildSha": "a" * 40,
            "manifestUrl": f"https://example.invalid/{variant}/manifest.json",
            "validations": [
                {"layer": layer, "status": "success", "repository_sha": "a" * 40}
                for layer in ("build", "unit", "integration", "artifact", "hardware")
            ],
        }

    def test_finalization_accepts_one_version_with_independently_validated_variants(self):
        result = validate_release_pair(self.release("v1"), self.release("v2"))
        self.assertEqual(result["version"], "1.2.3")
        self.assertEqual(result["build_sha"], "a" * 40)
        self.assertNotEqual(result["manifest_url_4mb"], result["manifest_url_16mb"])

    def test_finalization_cannot_reuse_the_other_variants_hardware_gate(self):
        for missing_variant in ("v1", "v2"):
            for invalid_gate in ("missing", "failed", "wrong_commit"):
                with self.subTest(variant=missing_variant, invalid_gate=invalid_gate):
                    releases = {variant: self.release(variant) for variant in ("v1", "v2")}
                    rows = releases[missing_variant]["validations"]
                    if invalid_gate == "missing":
                        rows.pop()
                    elif invalid_gate == "failed":
                        rows[-1]["status"] = "failed"
                    else:
                        rows[-1]["repository_sha"] = "b" * 40
                    with self.assertRaisesRegex(ValueError, "persisted gates"):
                        validate_release_pair(releases["v1"], releases["v2"])

    def test_finalization_rejects_mixed_identity_or_unready_candidates(self):
        for key, value in (("version", "1.2.4"), ("buildSha", "b" * 40),
                           ("hardwareVariant", "v1"), ("track", "staging"),
                           ("deviceType", "dtt"), ("lifecycle", "validating"),
                           ("paused", False), ("rolloutPercentage", 1)):
            with self.subTest(key=key):
                release = self.release("v2")
                release[key] = value
                with self.assertRaises(ValueError):
                    validate_release_pair(self.release("v1"), release)

    def test_finalization_rejects_shared_or_unsafe_manifests(self):
        for manifest in (self.release("v1")["manifestUrl"], "http://example.invalid/manifest.json",
                         "https://example.invalid/manifest.json\nother=value"):
            with self.subTest(manifest=manifest):
                release = self.release("v2")
                release["manifestUrl"] = manifest
                with self.assertRaises(ValueError):
                    validate_release_pair(self.release("v1"), release)

    def test_validate_pair_outputs_only_after_both_pass(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            files = [root / "4mb.json", root / "16mb.json"]
            for path, variant in zip(files, ("v1", "v2")):
                path.write_text(json.dumps(self.release(variant)))
            output = root / "outputs"
            argv = ["release_workflow.py", "validate-pair", "--release-4mb", str(files[0]),
                    "--release-16mb", str(files[1])]
            with patch.dict("os.environ", {"GITHUB_OUTPUT": str(output)}), patch("sys.argv", argv):
                self.assertEqual(main(), 0)
                previous = output.read_text()
                invalid = copy.deepcopy(self.release("v1"))
                invalid["validations"] = []
                files[0].write_text(json.dumps(invalid))
                with self.assertRaises(ValueError):
                    main()
                self.assertEqual(output.read_text(), previous)

    def test_current_source_matrix_keeps_all_native_tests(self):
        # These contracts protect against silently restoring the frozen legacy build
        # or narrowing the only native test gate when another suite is added.
        workflows = Path(__file__).resolve().parents[1] / "workflows"
        ci = (workflows / "pull_request_action.yml").read_text()
        for environment in ("production", "staging", "production_4mb", "staging_4mb"):
            self.assertIn(f"- environment: {environment}\n", ci)
        self.assertNotIn("legacy-4mb", ci)
        for name in ("pull_request_action.yml", "publish_firmware.yml"):
            workflow = (workflows / name).read_text()
            self.assertRegex(workflow, r"(?m)^\s*(?:run: )?pio test -e test$")
            self.assertIn("-t buildprog", workflow)


if __name__ == "__main__":
    unittest.main()
