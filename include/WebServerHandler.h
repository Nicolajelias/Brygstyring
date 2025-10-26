#pragma once
#ifndef WEBSERVERHANDLER_H
#define WEBSERVERHANDLER_H

#include <WebServer.h>
#include <HTTPUpdateServer.h>

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

    // Proces-ruter
    static void handleStartMashing();
    static void handleStartMashout();
    static void handleStartBoiling();
    static void handleStopProcess();
    static void handlePauseProcess();
    static void handleResumeProcess();

private:
    static void handleResetProcessState();
    static WebServer server;
    static HTTPUpdateServer httpUpdater;
};

#endif // WEBSERVERHANDLER_H
