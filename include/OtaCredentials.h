#pragma once

#if __has_include("secrets.hpp")
#include "secrets.hpp"
#else
inline constexpr const char* OTA_USERNAME = "";
inline constexpr const char* OTA_PASSWORD = "";
#endif

namespace OtaCredentials {
inline bool configured() {
  return OTA_USERNAME && OTA_PASSWORD && OTA_USERNAME[0] != '\0' &&
         __builtin_strlen(OTA_PASSWORD) >= 12;
}
}  // namespace OtaCredentials
