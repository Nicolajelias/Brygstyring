#pragma once
#include <array>
#include <cstring>
struct EEPROMMock {
  std::array<unsigned char,512> data{};
  bool begin(int) { return true; }
  template<class T> void get(int offset, T& value) { std::memcpy(&value,data.data()+offset,sizeof(value)); }
  template<class T> void put(int offset, const T& value) { std::memcpy(data.data()+offset,&value,sizeof(value)); }
  bool commit() { return true; }
};
extern EEPROMMock EEPROM;
