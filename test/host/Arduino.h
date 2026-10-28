#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>
#include <type_traits>
using std::min;
using std::isfinite;
constexpr int HIGH = 1, LOW = 0, OUTPUT = 1, INPUT_PULLUP = 2;
#define PROGMEM
#define F(value) value
struct String : std::string {
  String() = default;
  using std::string::string;
  String(const std::string& value) : std::string(value) {}
  template<class T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>
  String(T value) : std::string(std::to_string(value)) {}
  String(float value, int digits) {
    std::ostringstream out; out << std::fixed << std::setprecision(digits) << value; assign(out.str());
  }
  long toInt() const { return std::strtol(c_str(), nullptr, 10); }
};
struct SerialMock {
  template<class T> void println(const T&) {}
  template<class... T> void printf(const char*, T...) {}
};
extern SerialMock Serial;
extern uint32_t mockNow;
extern int mockPins[64];
inline unsigned long millis() { return mockNow; }
inline void delay(unsigned long ms) { mockNow += ms; }
inline void pinMode(int, int) {}
void digitalWrite(int pin, int level);
inline int digitalRead(int pin) { return mockPins[pin]; }
inline void ledcWrite(int, int) {}
inline void ledcWriteTone(int, int) {}
inline void ledcSetup(int, int, int) {}
inline void ledcAttachPin(int, int) {}
