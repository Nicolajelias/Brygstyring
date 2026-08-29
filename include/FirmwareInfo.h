#pragma once

#include <cstdint>
#include "firmware_version.hpp"

struct FirmwareInfo {
  const char* version;
  const char* baseVersion;
  uint32_t buildNumber;
  const char* gitHash;
  bool dirty;
  bool fallback;
  const char* buildTimestampUtc;
};

inline constexpr FirmwareInfo FIRMWARE_INFO{
    firmware_version_generated::version,
    firmware_version_generated::baseVersion,
    firmware_version_generated::buildNumber,
    firmware_version_generated::gitHash,
    firmware_version_generated::gitDirty,
    firmware_version_generated::fallback,
    firmware_version_generated::buildTimestampUtc,
};
