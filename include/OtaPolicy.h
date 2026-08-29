#pragma once

#include <cstdint>

namespace OtaPolicy {
constexpr uint8_t MAX_AUTH_FAILURES = 5;
constexpr uint32_t AUTH_LOCKOUT_MS = 60000UL;
constexpr uint32_t BOOT_VALIDATION_MS = 30000UL;
constexpr uint32_t MIN_HEALTHY_HEAP = 50000UL;

constexpr bool authenticationLocked(uint8_t failures, uint32_t ageMs) {
  return failures >= MAX_AUTH_FAILURES && ageMs < AUTH_LOCKOUT_MS;
}
constexpr bool bootValidationReady(bool pending, uint32_t stableAgeMs,
                                   uint32_t loopCount, uint32_t freeHeap) {
  return pending && stableAgeMs >= BOOT_VALIDATION_MS && loopCount > 0 &&
         freeHeap >= MIN_HEALTHY_HEAP;
}
constexpr bool successfulUploadMayReboot(bool finished, bool hasError) {
  return finished && !hasError;
}
}  // namespace OtaPolicy
