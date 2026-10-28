#pragma once
#include <cmath>
#include <cstdint>

// Measured temperatures are the UNCALIBRATED readings. Preserve the observations.
struct TemperatureCalibration {
  uint32_t points = 0;
  float measured1 = 0.0f;
  float reference1 = 0.0f;
  float measured2 = 100.0f;
  float reference2 = 100.0f;
};

namespace TemperatureCalibrationMath {
inline bool inRange(float value) {
  return std::isfinite(value) && value >= -20.0f && value <= 125.0f;
}
inline bool coefficients(const TemperatureCalibration& data, float& gain, float& offset) {
  gain = 1.0f;
  offset = 0.0f;
  // Validate inactive fields too: persisted corrupt floats must not reach the JSON API.
  if (!inRange(data.measured1) || !inRange(data.reference1) ||
      !inRange(data.measured2) || !inRange(data.reference2)) return false;
  if (data.points == 0) return true;
  if (data.points > 2 || !inRange(data.measured1) || !inRange(data.reference1)) return false;
  if (data.points == 2) {
    if (!inRange(data.measured2) || !inRange(data.reference2) ||
        data.measured2 - data.measured1 < 10.0f || data.reference2 - data.reference1 < 10.0f) return false;
    gain = (data.reference2 - data.reference1) / (data.measured2 - data.measured1);
    if (std::fabs(data.reference2 - data.measured2) > 10.0f) return false;
  }
  offset = data.reference1 - gain * data.measured1;
  return std::isfinite(gain) && std::isfinite(offset) && gain >= 0.9f && gain <= 1.1f &&
         std::fabs(offset) <= 10.0f && std::fabs(data.reference1 - data.measured1) <= 10.0f;
}
inline float apply(float measured, const TemperatureCalibration& data) {
  float gain, offset;
  if (!std::isfinite(measured) || !coefficients(data, gain, offset)) return NAN;
  return measured * gain + offset;
}
}  // namespace TemperatureCalibrationMath
