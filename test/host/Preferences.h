#pragma once
#include <vector>
#include <cstring>
#include <map>
#include <string>
class Preferences {
  std::string ns;
  std::vector<unsigned char>& storage() { return ns == "pt100-cal" ? blob : records[ns]; }
 public:
  static std::map<std::string, std::vector<unsigned char>> records;
  static std::vector<unsigned char> blob;
  static bool failWrites;
  bool begin(const char* name, bool readOnly) { ns = name; return !readOnly || !storage().empty(); }
  void end() {}
  size_t getBytesLength(const char*) { return storage().size(); }
  size_t getBytes(const char*, void* data, size_t length) {
    if (length != storage().size()) return 0;
    std::memcpy(data, storage().data(), length); return length;
  }
  size_t putBytes(const char*, const void* data, size_t length) {
    if (failWrites) return 0;
    const auto* bytes = static_cast<const unsigned char*>(data);
    storage().assign(bytes, bytes + length); return length;
  }
};
