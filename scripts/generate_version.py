"""PlatformIO integration for Git metadata and release artifacts."""

from __future__ import annotations

import hashlib
import shutil
import sys
from pathlib import Path

Import("env")  # type: ignore[name-defined]

PROJECT_NAME = "brygstyring"
project_dir = Path(env.subst("$PROJECT_DIR"))  # type: ignore[name-defined]
build_dir = Path(env.subst("$BUILD_DIR"))  # type: ignore[name-defined]
environment_name = env.subst("$PIOENV")  # type: ignore[name-defined]
is_test_build = bool(env.get("PIOTEST_RUNNING_NAME"))  # type: ignore[name-defined]
sys.path.insert(0, str(project_dir))

from scripts.versioning import artifact_filename, collect_version, manifest_data, serialize_manifest  # noqa: E402


def cpp_string(value: str) -> str:
    return value.replace("\\", "\\\\").replace('"', '\\"')


metadata = collect_version(project_dir)
generated_dir = build_dir / "generated"
generated_dir.mkdir(parents=True, exist_ok=True)
header_path = generated_dir / "firmware_version.hpp"
header_contents = f"""#pragma once
#include <cstdint>
namespace firmware_version_generated {{
inline constexpr char version[] = "{cpp_string(metadata.version)}";
inline constexpr char baseVersion[] = "{cpp_string(metadata.base_version)}";
inline constexpr uint32_t buildNumber = {metadata.build_number}U;
inline constexpr char gitHash[] = "{cpp_string(metadata.git_hash)}";
inline constexpr bool gitDirty = {str(metadata.dirty).lower()};
inline constexpr bool fallback = {str(metadata.fallback).lower()};
inline constexpr char buildTimestampUtc[] = "{metadata.build_timestamp_utc}";
}}  // namespace firmware_version_generated
"""
if not header_path.exists() or header_path.read_text(encoding="utf-8") != header_contents:
    header_path.write_text(header_contents, encoding="utf-8", newline="\n")
env.Append(CPPPATH=[str(generated_dir)])  # type: ignore[name-defined]

artifacts_dir = project_dir / ".build-artifacts" / environment_name
artifact_name = artifact_filename(PROJECT_NAME, metadata, environment_name)
manifest_path = artifacts_dir / "firmware-manifest.json"
if not is_test_build:
    artifacts_dir.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(serialize_manifest(manifest_data(
        PROJECT_NAME, metadata, environment_name, artifact_name)), encoding="utf-8", newline="\n")
if metadata.fallback:
    print(f"Warning: no annotated SemVer tag; using {metadata.version}")
print(f"Firmware version: {metadata.version} | build {metadata.build_number} | git {metadata.git_hash} | dirty={metadata.dirty}")


def package_firmware(source, target, env) -> None:
    del source, env
    binary_path = Path(str(target[0]))
    if binary_path.read_bytes()[:1] != b"\xE9":
        raise RuntimeError(f"Not an ESP application image: {binary_path}")
    packaged_binary = artifacts_dir / artifact_name
    shutil.copy2(binary_path, packaged_binary)
    digest = hashlib.sha256(packaged_binary.read_bytes()).hexdigest()
    (artifacts_dir / f"{artifact_name}.sha256").write_text(
        f"{digest}  {artifact_name}\n", encoding="ascii", newline="\n")
    manifest_path.write_text(serialize_manifest(manifest_data(
        PROJECT_NAME, metadata, environment_name, artifact_name, digest)), encoding="utf-8", newline="\n")
    print(f"Firmware artifact: {packaged_binary}")
    print(f"Firmware SHA-256: {digest}")


if not is_test_build:
    env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", package_firmware)  # type: ignore[name-defined]
