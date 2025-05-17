#pragma once
#ifndef WEBSERVERHANDLER_H
#define WEBSERVERHANDLER_H

#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>

class WebServerHandler {
public:
    static void begin();
    static void handleClient();
    static void handleDebug();

    // Routes
    static void handleRoot();
    static void handleSettings();
    static void handleSaveSettings();
    static void handleResetSettings();
    static void handleStatus();
    static void handleTogglePump();
    static void handleToggleGasValve();
    static void handleSwapSensorAddresses();
    static void handleSaveSensorSettings();

    // Proces-ruter
    static void handleStartMashing();
    static void handleStartMashout();
    static void handleStartBoiling();
    static void handleStopProcess();
    static void handlePauseProcess();
    static void handleResumeProcess();
    static void handleReadSensorAddresses();

private:
    static void handleResetProcessState();
    static ESP8266WebServer server;
    static ESP8266HTTPUpdateServer httpUpdater;
};

#endif // WEBSERVERHANDLER_H
