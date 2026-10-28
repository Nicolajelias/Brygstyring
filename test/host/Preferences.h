#pragma once
#include <vector>
#include <cstring>
class Preferences {
 public:
  static std::vector<unsigned char> blob;
  static bool failWrites;
  bool begin(const char*, bool readOnly) { return !readOnly || !blob.empty(); }
  void end() {}
  size_t getBytesLength(const char*) { return blob.size(); }
  size_t getBytes(const char*, void* data, size_t length) {
    if (length != blob.size()) return 0;
    std::memcpy(data, blob.data(), length); return length;
  }
  size_t putBytes(const char*, const void* data, size_t length) {
    if (failWrites) return 0;
    const auto* bytes = static_cast<const unsigned char*>(data);
    blob.assign(bytes, bytes + length); return length;
  }
};
