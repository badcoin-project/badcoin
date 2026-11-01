#!/usr/bin/env python3
"""Verify that a release staging package exactly matches its manifest."""

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def fail(message):
    raise ValueError(message)


def relative_file(value):
    path = Path(value)
    if path.is_absolute() or ".." in path.parts or not value:
        fail("invalid manifest artifact path: %s" % value)
    return path


def verify(staging_dir, expected_tag=None, expected_manifest_sha=None):
    manifest_path = staging_dir / "release-manifest.json"
    if not manifest_path.is_file():
        fail("missing release manifest")
    manifest_sha = sha256(manifest_path)
    if expected_manifest_sha and manifest_sha != expected_manifest_sha:
        fail("release manifest checksum mismatch")
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as error:
        fail("invalid release manifest: %s" % error)
    if manifest.get("schema") != "badcoin.release.staging.v1":
        fail("unexpected release manifest schema")
    if not re.fullmatch(r"[0-9a-f]{40}", manifest.get("candidate_commit", "")):
        fail("invalid manifest candidate commit")
    if not re.fullmatch(r"v[0-9]+(?:\.[0-9]+){3}-rc[0-9]+", manifest.get("release_tag", "")):
        fail("invalid manifest release tag")
    if expected_tag and manifest["release_tag"] != expected_tag:
        fail("release manifest tag mismatch")
    if manifest.get("unsigned") is not True:
        fail("unexpected signing declaration")
    package_dir = staging_dir / "packages"
    if not package_dir.is_dir():
        fail("missing package directory")
    entries = manifest.get("artifacts")
    if not isinstance(entries, list) or not entries:
        fail("missing manifest artifacts")
    expected = {}
    for entry in entries:
        if not isinstance(entry, dict) or not isinstance(entry.get("filename"), str) or not re.fullmatch(r"[0-9a-f]{64}", entry.get("sha256", "")):
            fail("invalid manifest artifact entry")
        filename = relative_file(entry["filename"])
        if filename in expected:
            fail("duplicate manifest artifact: %s" % filename)
        expected[filename] = entry["sha256"]
    actual = {path.relative_to(package_dir) for path in package_dir.rglob("*") if path.is_file()}
    if actual != set(expected):
        fail("package inventory does not exactly match manifest")
    for filename, expected_sha in expected.items():
        if sha256(package_dir / filename) != expected_sha:
            fail("staged artifact checksum mismatch: %s" % filename)
    return manifest_sha


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--staging-dir", required=True, type=Path)
    parser.add_argument("--expected-tag")
    parser.add_argument("--expected-manifest-sha")
    args = parser.parse_args()
    try:
        manifest_sha = verify(args.staging_dir, args.expected_tag, args.expected_manifest_sha)
    except (OSError, ValueError) as error:
        print("release manifest validation failed: %s" % error, file=sys.stderr)
        return 1
    print("release manifest validation passed: %s" % manifest_sha)
    return 0


if __name__ == "__main__":
    sys.exit(main())
