#!/usr/bin/env python3

import hashlib
import importlib.util
import json
import subprocess
import tarfile
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[2] / "contrib" / "release" / "prepare_release_package.py"
CANDIDATE = "a" * 40
TAG = "v1.0.0.0-rc99"
PLATFORMS = {
    "linux-x64-core": ["badcoind", "badcoin-cli", "badcoin-tx"],
    "windows-x64-core": ["badcoind.exe", "badcoin-cli.exe", "badcoin-tx.exe"],
    "windows-x64-qt": ["badcoind.exe", "badcoin-cli.exe", "badcoin-tx.exe", "badcoin-qt.exe"],
    "macos-arm64-core": ["badcoind", "badcoin-cli", "badcoin-tx"],
    "macos-arm64-qt": ["badcoind", "badcoin-cli", "badcoin-tx", "badcoin-qt"],
}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


class ReleasePackageTest(unittest.TestCase):
    def create_input(self, root, candidate=CANDIDATE):
        for platform, binaries in PLATFORMS.items():
            destination = root / platform
            destination.mkdir(parents=True)
            source = destination
            if platform.startswith("macos"):
                unpacked = root / (platform + "-unpacked")
                unpacked.mkdir()
                source = unpacked
            for binary in binaries:
                (source / binary).write_bytes((platform + binary).encode("ascii"))
            checksums = "".join("%s  %s\n" % (digest(source / binary), binary) for binary in binaries)
            (source / "SHA256SUMS.txt").write_text(checksums, encoding="utf-8")
            if platform.startswith("macos"):
                archive_name = "badcoin-%s.tar.gz" % platform
                archive = destination / archive_name
                with tarfile.open(archive, "w:gz") as archive_file:
                    for item in source.iterdir():
                        archive_file.add(item, arcname=item.name)
                (destination / (archive_name + ".SHA256SUMS.txt")).write_text(
                    "%s  %s\n" % (digest(archive), archive_name), encoding="utf-8")
            (destination / "provenance.json").write_text(json.dumps({"commit": candidate, "platform": platform}), encoding="utf-8")

    def run_prepare(self, input_dir, output_dir, candidate=CANDIDATE, tag=TAG):
        return subprocess.run([
            "python3", str(SCRIPT), "--input-dir", str(input_dir), "--output-dir", str(output_dir),
            "--candidate-sha", candidate, "--release-tag", tag,
        ], text=True, capture_output=True)

    def test_prepares_complete_unsigned_package(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.create_input(root / "input")
            result = self.run_prepare(root / "input", root / "output")
            self.assertEqual(result.returncode, 0, result.stderr)
            manifest = json.loads((root / "output/release-manifest.json").read_text(encoding="utf-8"))
            self.assertEqual(manifest["candidate_commit"], CANDIDATE)
            self.assertEqual(manifest["release_tag"], TAG)
            self.assertTrue(manifest["unsigned"])
            self.assertEqual({entry["platform"] for entry in manifest["artifacts"]}, set(PLATFORMS))

    def test_rejects_candidate_mismatch(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.create_input(root / "input", candidate="b" * 40)
            result = self.run_prepare(root / "input", root / "output")
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("candidate identity mismatch", result.stderr)

    def test_rejects_missing_platform(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.create_input(root / "input")
            (root / "input/windows-x64-qt/provenance.json").unlink()
            result = self.run_prepare(root / "input", root / "output")
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("missing provenance", result.stderr)

    def test_rejects_invalid_checksum(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.create_input(root / "input")
            (root / "input/linux-x64-core/badcoind").write_text("tampered", encoding="utf-8")
            result = self.run_prepare(root / "input", root / "output")
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("checksum mismatch", result.stderr)

    def test_rejects_invalid_tag(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.create_input(root / "input")
            result = self.run_prepare(root / "input", root / "output", tag="v1.0.0.0")
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("release tag", result.stderr)


if __name__ == "__main__":
    unittest.main()
