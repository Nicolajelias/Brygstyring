#pragma once
struct EEPROMMock {
  template<class T> void get(int, T& value) { value = T{}; }
  template<class T> void put(int, const T&) {}
  bool commit() { return true; }
};
extern EEPROMMock EEPROM;
