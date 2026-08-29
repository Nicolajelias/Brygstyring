"""Pure helpers for deterministic Git-derived firmware version metadata."""

from __future__ import annotations

import datetime as dt
import json
import os
import re
import subprocess
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Mapping

SEMVER_TAG = re.compile(r"^v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$")
UNSAFE_FILENAME = re.compile(r"[^A-Za-z0-9._-]+")


@dataclass(frozen=True)
class VersionMetadata:
    version: str
    base_version: str
    build_number: int
    git_hash: str
    dirty: bool
    build_timestamp_utc: str
    fallback: bool


def _run_git(repository: Path, arguments: list[str], git_executable: str) -> str:
    result = subprocess.run(
        [git_executable, *arguments], cwd=repository, check=True,
        capture_output=True, text=True, encoding="utf-8"
    )
    return result.stdout.strip()


def build_timestamp(environment: Mapping[str, str] | None = None) -> str:
    values = os.environ if environment is None else environment
    source_epoch = values.get("SOURCE_DATE_EPOCH")
    timestamp = (
        dt.datetime.fromtimestamp(int(source_epoch), tz=dt.timezone.utc)
        if source_epoch is not None else dt.datetime.now(tz=dt.timezone.utc)
    )
    return timestamp.strftime("%Y-%m-%dT%H:%M:%SZ")


def collect_version(repository: Path, environment: Mapping[str, str] | None = None,
                    git_executable: str = "git") -> VersionMetadata:
    timestamp = build_timestamp(environment)
    try:
        _run_git(repository, ["rev-parse", "--is-inside-work-tree"], git_executable)
        git_hash = _run_git(repository, ["rev-parse", "--short=7", "HEAD"], git_executable)
        build_number = int(_run_git(repository, ["rev-list", "--count", "HEAD"], git_executable))
        dirty = bool(_run_git(repository, ["status", "--porcelain", "--untracked-files=normal"], git_executable))
        tag_lines = _run_git(repository, [
            "for-each-ref", "--merged=HEAD", "--sort=-version:refname",
            "--format=%(refname:short)|%(objecttype)", "refs/tags"
        ], git_executable).splitlines()
        valid_tags = []
        for line in tag_lines:
            name, separator, object_type = line.partition("|")
            match = SEMVER_TAG.fullmatch(name)
            if separator and object_type == "tag" and match:
                valid_tags.append((tuple(int(value) for value in match.groups()), name))
        if not valid_tags:
            version = "0.0.0-unknown" + (".dirty" if dirty else "")
            return VersionMetadata(version, "0.0.0", build_number, git_hash, dirty, timestamp, True)
        _, tag = max(valid_tags)
        base_version = tag[1:]
        commits_since = int(_run_git(repository, ["rev-list", "--count", f"{tag}..HEAD"], git_executable))
        version = base_version if commits_since == 0 else f"{base_version}-dev.{commits_since}+g{git_hash}"
        if dirty:
            version += ".dirty"
        return VersionMetadata(version, base_version, build_number, git_hash, dirty, timestamp, False)
    except (FileNotFoundError, OSError, subprocess.CalledProcessError, ValueError):
        return VersionMetadata("0.0.0-unknown", "0.0.0", 0, "unknown", False, timestamp, True)


def sanitize_filename(value: str) -> str:
    return UNSAFE_FILENAME.sub("_", value).strip("._") or "unknown"


def artifact_filename(project: str, metadata: VersionMetadata, environment: str) -> str:
    return sanitize_filename(f"{project}_{metadata.version}_build{metadata.build_number}_{environment}.bin")


def manifest_data(project: str, metadata: VersionMetadata, environment: str,
                  artifact: str, sha256: str | None = None) -> dict[str, object]:
    data: dict[str, object] = {
        "project": project,
        **{key: value for key, value in asdict(metadata).items() if key != "fallback"},
        "environment": environment, "chip": "esp32-s3", "artifact": artifact,
    }
    if sha256 is not None:
        data["sha256"] = sha256
    return data


def serialize_manifest(data: dict[str, object]) -> str:
    return json.dumps(data, indent=2, sort_keys=True) + "\n"
