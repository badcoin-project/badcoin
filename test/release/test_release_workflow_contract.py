#!/usr/bin/env python3

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class ReleaseWorkflowContractTest(unittest.TestCase):
    def setUp(self):
        self.preparation = (ROOT / ".github/workflows/release-preparation.yml").read_text(encoding="utf-8")
        self.publication = (ROOT / ".github/workflows/release-publication.yml").read_text(encoding="utf-8")

    def test_preparation_rejects_mismatched_source_runs_and_collisions(self):
        self.assertIn("run.head_sha !== candidate", self.preparation)
        self.assertIn("run.conclusion !== 'success'", self.preparation)
        self.assertIn("trustedBranches.has(run.head_branch)", self.preparation)
        self.assertIn("trustedEvents.has(run.event)", self.preparation)
        self.assertIn("source workflow run IDs must be distinct", self.preparation)
        self.assertIn("candidate tag or branch already exists", self.preparation)
        self.assertIn("release already exists", self.preparation)

    def test_publication_requires_verified_reviewer_protection_and_prerelease(self):
        self.assertIn("environment: release-publication", self.publication)
        self.assertIn("getEnvironment", self.publication)
        self.assertIn("required_reviewers", self.publication)
        self.assertNotIn("approval:", self.publication)
        self.assertIn("contents: write", self.publication)
        self.assertIn("manifest_sha256", self.publication)
        self.assertIn("tag or branch already exists", self.publication)
        self.assertIn("--prerelease", self.publication)
        self.assertIn("verify_release_manifest.py", self.publication)

    def test_ordinary_workflows_cannot_write_release_content(self):
        for workflow in ("linux-artifacts.yml", "windows-artifacts.yml", "macos-artifacts.yml"):
            text = (ROOT / ".github/workflows" / workflow).read_text(encoding="utf-8")
            self.assertIn("contents: read", text)
            self.assertNotIn("contents: write", text)


if __name__ == "__main__":
    unittest.main()
