from __future__ import annotations

import json
import subprocess
import tempfile
import unittest
from pathlib import Path

from scripts.versioning import artifact_filename, collect_version, manifest_data, serialize_manifest


class VersioningTests(unittest.TestCase):
    def make_repository(self) -> Path:
        root = Path(".pio/test-versioning")
        root.mkdir(parents=True, exist_ok=True)
        directory = tempfile.TemporaryDirectory(dir=root)
        self.addCleanup(directory.cleanup)
        path = Path(directory.name)
        self.git(path, "init", "-q")
        self.git(path, "config", "user.email", "test@example.invalid")
        self.git(path, "config", "user.name", "Version Test")
        (path / "file.txt").write_text("one\n", encoding="utf-8")
        self.git(path, "add", "file.txt")
        self.git(path, "commit", "-q", "-m", "initial")
        return path

    @staticmethod
    def git(path: Path, *arguments: str) -> str:
        return subprocess.run(["git", *arguments], cwd=path, check=True,
                              capture_output=True, text=True).stdout.strip()

    def test_annotated_tag_and_development_version(self) -> None:
        repo = self.make_repository()
        self.git(repo, "tag", "-a", "v2.2.0", "-m", "release")
        self.assertEqual(collect_version(repo).version, "2.2.0")
        (repo / "file.txt").write_text("two\n", encoding="utf-8")
        self.git(repo, "add", "file.txt")
        self.git(repo, "commit", "-q", "-m", "second")
        self.assertRegex(collect_version(repo).version, r"^2\.2\.0-dev\.1\+g[0-9a-f]{7}$")

    def test_dirty_and_no_tag_are_explicit(self) -> None:
        repo = self.make_repository()
        self.assertTrue(collect_version(repo).fallback)
        (repo / "file.txt").write_text("dirty\n", encoding="utf-8")
        self.assertTrue(collect_version(repo).version.endswith(".dirty"))

    def test_lightweight_tag_is_not_a_release(self) -> None:
        repo = self.make_repository()
        self.git(repo, "tag", "v9.9.9")
        self.assertTrue(collect_version(repo).fallback)

    def test_reproducible_timestamp_and_manifest(self) -> None:
        metadata = collect_version(self.make_repository(), {"SOURCE_DATE_EPOCH": "0"})
        self.assertEqual(metadata.build_timestamp_utc, "1970-01-01T00:00:00Z")
        artifact = artifact_filename("brygstyring", metadata, "esp32")
        data = json.loads(serialize_manifest(manifest_data("brygstyring", metadata, "esp32", artifact)))
        self.assertEqual(data["artifact"], artifact)
        self.assertNotIn("+", artifact)


if __name__ == "__main__":
    unittest.main()
