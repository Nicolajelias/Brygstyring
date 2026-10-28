#pragma once
#include "Arduino.h"
#include <map>
class WebServer {
 public:
  std::map<std::string, String> arguments;
  int responseCode = 0;
  String response;
  explicit WebServer(int) {}
  bool hasArg(const String& name) { return arguments.count(name) != 0; }
  String arg(const String& name) { return arguments[name]; }
  void send(int code, const char*, const String& body) { responseCode = code; response = body; }
  void send_P(int code, const char* type, const char* body) { send(code, type, body); }
  void sendHeader(const char*, const char*) {}
};
