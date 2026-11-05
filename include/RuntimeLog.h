#pragma once
#include <Arduino.h>
class RuntimeLogger {
 public:
  void begin();
  void drain();
  void printf(const char* format, ...);
  void println(const char* message);
  void println(const String& message) { println(message.c_str()); }
};
extern RuntimeLogger RuntimeLog;
