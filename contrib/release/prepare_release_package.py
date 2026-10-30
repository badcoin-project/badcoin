#!/usr/bin/env python3
"""Build a deterministic, non-publishing Badcoin release staging package."""

import argparse
import hashlib
import json
import re
import shutil
import sys
import tarfile
from pathlib import Path


EXPECTED = {
    "linux-x64-core": ("badcoind", "badcoin-cli", "badcoin-tx"),
    "windows-x64-core": ("badcoind.exe", "badcoin-cli.exe", "badcoin-tx.exe"),
    "windows-x64-qt": ("badcoind.exe", "badcoin-cli.exe", "badcoin-tx.exe", "badcoin-qt.exe"),
    "macos-arm64-core": ("badcoind", "badcoin-cli", "badcoin-tx"),
    "macos-arm64-qt": ("badcoind", "badcoin-cli", "badcoin-tx", "badcoin-qt"),
}
ARCHIVES = {
    "macos-arm64-core": "badcoin-macos-arm64-core.tar.gz",
    "macos-arm64-qt": "badcoin-macos-arm64-qt.tar.gz",
}


def fail(message):
    raise ValueError(message)


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def checked_relative_path(value, label):
    path = Path(value)
    if path.is_absolute() or ".." in path.parts or len(path.parts) != 1:
        fail("invalid %s path: %s" % (label, value))
    return path


def verify_checksum_file(directory, expected):
    checksum_file = directory / "SHA256SUMS.txt"
    if not checksum_file.is_file():
        fail("missing checksum file in %s" % directory)
    recorded = {}
    for line in checksum_file.read_text(encoding="utf-8").splitlines():
        match = re.fullmatch(r"([0-9a-fA-F]{64}) [ *](.+)", line)
        if not match:
            fail("invalid checksum line in %s: %s" % (checksum_file, line))
        filename = checked_relative_path(match.group(2), "checksum")
        if filename.name in recorded:
            fail("duplicate checksum entry: %s" % filename)
        recorded[filename.name] = match.group(1).lower()
    if set(recorded) != set(expected):
        fail("unexpected checksum inventory in %s" % directory)
    for filename in expected:
        artifact = directory / filename
        if not artifact.is_file():
            fail("missing expected executable: %s" % artifact)
        if sha256(artifact) != recorded[filename]:
            fail("checksum mismatch: %s" % artifact)


def verify_provenance(directory, candidate_sha, platform):
    provenance = directory / "provenance.json"
    if not provenance.is_file():
        provenance = directory / (platform + ".provenance.json")
    if not provenance.is_file():
        fail("missing provenance for %s" % platform)
    try:
        data = json.loads(provenance.read_text(encoding="utf-8"))
    except json.JSONDecodeError as error:
        fail("invalid provenance for %s: %s" % (platform, error))
    if data.get("commit") != candidate_sha or data.get("platform") != platform:
        fail("candidate identity mismatch for %s" % platform)
    return data


def copy_tree(source, destination):
    if destination.exists():
        fail("duplicate staging destination: %s" % destination)
    shutil.copytree(source, destination)


def prepare(input_dir, output_dir, candidate_sha, release_tag):
    if not re.fullmatch(r"[0-9a-f]{40}", candidate_sha):
        fail("candidate SHA must be a lowercase 40-character commit SHA")
    if not re.fullmatch(r"v[0-9]+(?:\.[0-9]+){3}-rc[0-9]+", release_tag):
        fail("release tag must be an immutable vX.Y.Z.W-rcN designation")
    if output_dir.exists():
        fail("output directory already exists: %s" % output_dir)
    output_dir.mkdir(parents=True)
    package_dir = output_dir / "packages"
    package_dir.mkdir()
    artifacts = []
    provenance = {}
    for platform, expected in EXPECTED.items():
        source = input_dir / platform
        if not source.is_dir():
            fail("missing platform artifact directory: %s" % platform)
        provenance[platform] = verify_provenance(source, candidate_sha, platform)
        if platform in ARCHIVES:
            archive = source / ARCHIVES[platform]
            outer_checksum = source / (ARCHIVES[platform] + ".SHA256SUMS.txt")
            if not archive.is_file() or not outer_checksum.is_file():
                fail("missing macOS archive or outer checksum for %s" % platform)
            line = outer_checksum.read_text(encoding="utf-8").strip()
            if not re.fullmatch(r"[0-9a-fA-F]{64} [ *]" + re.escape(archive.name), line):
                fail("invalid outer checksum for %s" % platform)
            if sha256(archive) != line.split()[0].lower():
                fail("outer checksum mismatch for %s" % platform)
            extracted = output_dir / (platform + "-verify")
            with tarfile.open(str(archive), "r:gz") as tar:
                members = tar.getmembers()
                if any(member.name.startswith("/") or ".." in Path(member.name).parts or member.issym() or member.islnk() for member in members):
                    fail("unsafe archive member in %s" % platform)
                tar.extractall(str(extracted))
            verify_checksum_file(extracted, expected)
            shutil.rmtree(extracted)
        else:
            verify_checksum_file(source, expected)
        destination = package_dir / platform
        copy_tree(source, destination)
        for path in sorted(destination.rglob("*")):
            if path.is_file():
                artifacts.append({"platform": platform, "filename": str(path.relative_to(package_dir)), "sha256": sha256(path)})
    manifest = {
        "schema": "badcoin.release.staging.v1",
        "candidate_commit": candidate_sha,
        "release_tag": release_tag,
        "unsigned": True,
        "signing_status": "not-configured",
        "artifacts": artifacts,
        "provenance": provenance,
    }
    encoded = json.dumps(manifest, sort_keys=True, indent=2) + "\n"
    (output_dir / "release-manifest.json").write_text(encoded, encoding="utf-8")
    manifest_sha = sha256(output_dir / "release-manifest.json")
    report = "\n".join([
        "Badcoin release staging validation report",
        "candidate_commit: %s" % candidate_sha,
        "release_tag: %s" % release_tag,
        "manifest_sha256: %s" % manifest_sha,
        "signing: unsigned; no signing infrastructure was used",
        "validation: PASS (checksums, executable inventory, and candidate provenance verified)",
        "publication: not performed",
        "",
    ])
    (output_dir / "VALIDATION-REPORT.txt").write_text(report, encoding="utf-8")
    return manifest_sha


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-dir", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--candidate-sha", required=True)
    parser.add_argument("--release-tag", required=True)
    args = parser.parse_args()
    try:
        manifest_sha = prepare(args.input_dir, args.output_dir, args.candidate_sha, args.release_tag)
    except (OSError, ValueError, tarfile.TarError) as error:
        print("release staging validation failed: %s" % error, file=sys.stderr)
        return 1
    print("release staging validation passed: %s" % manifest_sha)
    return 0


if __name__ == "__main__":
    sys.exit(main())
