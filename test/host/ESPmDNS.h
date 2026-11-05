#pragma once
struct MDNSMock {
 void end() {}
 bool begin(const char*) {return true;}
 void setInstanceName(const char*) {}
 void addService(const char*,const char*,int) {}
};
extern MDNSMock MDNS;
