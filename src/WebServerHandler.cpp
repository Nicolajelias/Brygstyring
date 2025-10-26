#include "WebServerHandler.h"
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <Arduino.h>
#include "EEPROMHandler.h"
#include "ProcessHandler.h"
#include "TemperatureHandler.h"
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <Version.h>

// Statisk webserver og HTTPUpdateServer
ESP8266WebServer WebServerHandler::server(80);
ESP8266HTTPUpdateServer WebServerHandler::httpUpdater;

// HTML-header og -footer
const char* HTML_HEADER = R"html(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>Brygkontroller</title>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <!-- Data-URL til favicon -->
  <link rel="icon" type="image/x-icon" href="data:image/x-icon;base64,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">
  <style>
    body { font-family: Arial, sans-serif; margin: 10px; }
    .button { display: inline-block; padding: 10px 20px; margin: 5px; background: #007BFF; color: #fff; text-decoration: none; border-radius: 5px; }
    .button:hover { background: #0056b3; }
    .label { font-weight: bold; }
    .container { max-width: 600px; margin: auto; }
    input[type="text"], input[type="password"] { width: 100%; padding: 8px; margin: 5px 0; }
  </style>
  <script>
    function updateStatus() {
      fetch('/status')
        .then(response => response.json())
        .then(data => {
          document.getElementById('grydeTemp').innerText = data.grydeTemp + ' °C';
          document.getElementById('ventilTemp').innerText = data.ventilTemp + ' °C';
          document.getElementById('currentTime').innerText = data.currentTime;
          document.getElementById('pumpStatus').innerText = data.pumpStatus;
          document.getElementById('gasValveStatus').innerText = data.gasValveStatus;
          document.getElementById('startTime').innerText = data.startTime;
          document.getElementById('endTime').innerText = data.endTime;
          document.getElementById('processStatus').innerText = data.processStatus;
          let secRemain = data.timeRemaining;
          let mm = Math.floor(secRemain / 60);
          let ss = secRemain % 60;
          document.getElementById('timeRemaining').innerText = mm + ":" + (ss < 10 ? "0" + ss : ss);
    
          // Opdater indstillingsfelter kun hvis de ikke er i fokus
          const updateIfNotFocused = (id, value) => {
            const el = document.getElementById(id);
            if (document.activeElement !== el) {
              el.value = value;
            }
          };
    
          updateIfNotFocused('mashTime', data.mashTime / 60);
          updateIfNotFocused('mashoutTime', data.mashoutTime / 60);
          updateIfNotFocused('boilTime', data.boilTime / 60);
          updateIfNotFocused('mashSetpoint', data.mashSetpoint);
          updateIfNotFocused('mashoutSetpoint', data.mashoutSetpoint);
          updateIfNotFocused('hysteresis', data.hysteresis);
          updateIfNotFocused('valveOffset', data.valveOffset);
        })
        .catch(err => {
          console.error("Status update error:", err);
        });
    }
    setInterval(updateStatus, 1000);
    
    function togglePump() {
      fetch('/togglePump')
        .then(response => response.text())
        .then(data => {
          alert(data);
          updateStatus();
        });
    }
    
    function toggleGasValve() {
      fetch('/toggleGasValve')
        .then(response => response.text())
        .then(data => {
          alert(data);
          updateStatus();
        });
    }
    
    function startMashing() {
      fetch('/startMashing')
        .then(response => response.text())
        .then(data => {
          alert(data);
          updateStatus();
        });
    }
    
    function startMashout() {
      fetch('/startMashout')
        .then(response => response.text())
        .then(data => {
          alert(data);
          updateStatus();
        });
    }
    
    function startBoiling() {
      fetch('/startBoiling')
        .then(response => response.text())
        .then(data => {
          alert(data);
          updateStatus();
        });
    }
    
    function stopProcess() {
      fetch('/stopProcess')
        .then(response => response.text())
        .then(msg => {
          alert(msg);
          updateStatus();
        });
    }
    
    function pauseProcess() {
      fetch('/pauseProcess')
        .then(response => response.text())
        .then(data => {
          alert(data);
          updateStatus();
        });
    }
    
    function resumeProcess() {
      fetch('/resumeProcess')
        .then(response => response.text())
        .then(data => {
          alert(data);
          updateStatus();
        });
    }
    
    function resetProcessState() {
      fetch('/resetProcessState')
        .then(response => response.text())
        .then(data => {
          alert(data);
          updateStatus();
        });
    }
    
    function saveSettings(event) {
      event.preventDefault();
      const formData = new FormData(event.target);
      fetch('/saveSettings', {
        method: 'POST',
        body: new URLSearchParams(formData)
      })
      .then(response => response.text())
      .then(data => {
        alert('Indstillinger gemt.');
        setTimeout(() => { location.reload(); }, 1500);
        // Alternativt: ESP.restart() kan kaldes fra server-siden.
      });
    }
  </script>
</head>
<body onload="updateStatus()">
<div class="container">
)html";

const char* HTML_FOOTER = R"html(
</div>
</body>
</html>
)html";

// --- Endpoints ---

void WebServerHandler::handleRoot() {
  const Config &cfg = EEPROMHandler::getConfig();
  String html = HTML_HEADER;
  html += R"html(
  <h1>Brygkontroller</h1>
  <h2>Status</h2>
  <div style="line-height:1.5em;">
    <strong>Aktuel tid:</strong> <span id='currentTime'></span><br/>
    <strong>Gryde Temp:</strong> <span id='grydeTemp'></span> °C<br/>
    <strong>Ventil Temp:</strong> <span id='ventilTemp'></span> °C<br/>
    <strong>Pumpe Status:</strong> <span id='pumpStatus'></span><br/>
    <strong>Gasventil Status:</strong> <span id='gasValveStatus'></span><br/>
    <strong>Starttidspunkt:</strong> <span id='startTime'></span><br/>
    <strong>Sluttidspunkt:</strong> <span id='endTime'></span><br/>
    <strong>Proces Status:</strong> <span id='processStatus'></span><br/>
    <strong>Resterende tid:</strong> <span id='timeRemaining'></span>
  </div>
  <br/>
  <div style="display:flex; flex-wrap:wrap; gap:10px;">
    <button class='button' onclick='startMashing()' title="Start Mæskning">Start Mæskning</button>
    <button class='button' onclick='startMashout()' title="Start Udmæskning">Start Udmæskning</button>
    <button class='button' onclick='startBoiling()' title="Start Kogning">Start Kogning</button>
    <button class='button' onclick='pauseProcess()' title="Pause Proces">║║</button>
    <button class='button' onclick='resumeProcess()' title="Genoptag Proces">►</button>
    <button class='button' onclick='stopProcess()' title="Stop Proces">■</button>
    <button class='button' onclick='togglePump()' title="Toggle Pumpe">Pumpe</button>
    <button class='button' onclick='toggleGasValve()' title="Toggle Gas">Gas</button>
  </div>
  <hr/>
  <form onsubmit='saveSettings(event)' style="max-width:800px; margin:auto;">
    <!-- Første række: Mæsketid og Mæskning Setpoint -->
    <div style="display:flex; justify-content: space-between; align-items: center; margin-bottom:10px;">
      <div style="flex:1; margin-right:10px;">
        <label class='label'>Mæsketid (min):</label><br/>
        <input type='text' id='mashTime' name='mashTime' style="width:80px;"/>
      </div>
      <div style="flex:1;">
        <label class='label'>Mæskning Setpoint (°C):</label><br/>
        <input type='text' id='mashSetpoint' name='mashSetpoint' style="width:80px;"/>
      </div>
    </div>
    <!-- Anden række: Udmæskningstid og Udmæskning Setpoint -->
    <div style="display:flex; justify-content: space-between; align-items: center; margin-bottom:10px;">
      <div style="flex:1; margin-right:10px;">
        <label class='label'>Udmæskningstid (min):</label><br/>
        <input type='text' id='mashoutTime' name='mashoutTime' style="width:80px;"/>
      </div>
      <div style="flex:1;">
        <label class='label'>Udmæskning Setpoint (°C):</label><br/>
        <input type='text' id='mashoutSetpoint' name='mashoutSetpoint' style="width:80px;"/>
      </div>
    </div>
    <!-- Tredje række: Kogetid -->
    <div style="margin-bottom:10px;">
      <label class='label'>Kogetid (min):</label><br/>
      <input type='text' id='boilTime' name='boilTime' style="width:80px;"/>
    </div>
    <!-- Fjerde række: Hysterese og Ventil Offset -->
    <div style="display:flex; justify-content: flex-start; align-items: flex-start; margin-bottom:10px;">
      <div style="margin-right:10px;">
        <label class='label'>Hysterese (°C):</label><br/>
        <input type='text' id='hysteresis' name='hysteresis' style="width:60px;"/>
      </div>
)html";
  html += "<div><label class='label'>Ventil Offset (°C):</label><br/><input type='text' name='offset' value='" + String(cfg.tempOffset) + "' style='width:60px;'/></div>";
  html += R"html(
    </div>
    <div style="text-align:left; margin-bottom:10px;">
      <input class='button' type='submit' value='Gem Indstillinger'/>
    </div>
  </form>
  <div style="text-align:left; margin-bottom:10px;">
    <button class='button' onclick='resetProcessState()' title="Reset Process">Reset Process</button>
  </div>
  <div style="text-align:left;">
    <button class='button' onclick="location.href='/settings'">Indstillinger</button>
  </div>
)html";
  html += HTML_FOOTER;
  server.send(200, "text/html", html);
  WiFi.scanDelete();
}

void WebServerHandler::handleStatus() {
  String json = "{";
  json += "\"grydeTemp\":\"" + String(TemperatureHandler::getGrydeTemp(), 1) + "\","; 
  json += "\"ventilTemp\":\"" + String(TemperatureHandler::getVentilTemp(), 1) + "\","; 
  json += "\"currentTime\":\"" + ProcessHandler::getFormattedTime() + "\","; 
  json += "\"pumpStatus\":\"" + String(ProcessHandler::isPumpOn() ? "Pumpe tændt" : "Pumpe slukket") + "\","; 
  json += "\"gasValveStatus\":\"" + String(ProcessHandler::isGasValveOn() ? "Gas åben" : "Gas lukket") + "\","; 
  json += "\"startTime\":\"" + ProcessHandler::getStartTime() + "\","; 
  json += "\"endTime\":\"" + ProcessHandler::getEndTime() + "\","; 
  json += "\"processStatus\":\"" + ProcessHandler::getProcessStatus() + "\","; 
  json += "\"timeRemaining\":\"" + String(ProcessHandler::getRemainingTime()) + "\","; 
  json += "\"mashTime\":\"" + String(ProcessHandler::getMashTime()) + "\","; 
  json += "\"mashoutTime\":\"" + String(ProcessHandler::getMashoutTime()) + "\","; 
  json += "\"boilTime\":\"" + String(ProcessHandler::getBoilTime()) + "\","; 
  json += "\"mashSetpoint\":\"" + String(ProcessHandler::getMashSetpoint()) + "\","; 
  json += "\"mashoutSetpoint\":\"" + String(ProcessHandler::getMashoutSetpoint()) + "\","; 
  json += "\"hysteresis\":\"" + String(ProcessHandler::getHysteresis()) + "\",";
  json += "\"valveOffset\":\"" + String(ProcessHandler::getValveOffset()) + "\",";
  json += "\"version\":\"" + String(SOFTWARE_VERSION) + "\"";  json += "}";
  server.send(200, "application/json", json);
}

//Debug endpoint
void WebServerHandler::handleDebug() {
  String debugInfo = EEPROMHandler::getConfigAsString();
  server.send(200, "text/plain", debugInfo);
}

void WebServerHandler::handleTogglePump() {
  bool pumpState = ProcessHandler::togglePump();
  server.send(200, "text/plain", pumpState ? "Pumpe tændt" : "Pumpe slukket");
}

void WebServerHandler::handleToggleGasValve() {
  bool gasState = ProcessHandler::toggleGasValve();
  server.send(200, "text/plain", gasState ? "Gas åben" : "Gas lukket");
}

void WebServerHandler::handleStartMashing() {
  ProcessHandler::startMashing();
  server.send(200, "text/plain", "Mæskning startet");
}

void WebServerHandler::handleStartMashout() {
  ProcessHandler::startMashout();
  server.send(200, "text/plain", "Udmæskning startet");
}

void WebServerHandler::handleStartBoiling() {
  ProcessHandler::startBoiling();
  server.send(200, "text/plain", "Kogning startet");
}

void WebServerHandler::handleStopProcess() {
  ProcessHandler::stopProcess();
  server.send(200, "text/plain", "Proces stoppet");
}

void WebServerHandler::handlePauseProcess() {
  ProcessHandler::pauseProcess();
  server.send(200, "text/plain", "Proces pauset");
}

void WebServerHandler::handleResumeProcess() {
  ProcessHandler::resumeProcess();
  server.send(200, "text/plain", "Proces genoptaget");
}

void WebServerHandler::handleResetProcessState() {
  ProcessHandler::resetProcessState();
  server.send(200, "text/plain", "Process state reset. System is now IDLE.");
}

void WebServerHandler::handleSaveSettings() {
  Config cfg = EEPROMHandler::getConfig();
  if (server.hasArg("ssid"))
    strncpy(cfg.ssid, server.arg("ssid").c_str(), sizeof(cfg.ssid));
  if (server.hasArg("password"))
    strncpy(cfg.password, server.arg("password").c_str(), sizeof(cfg.password));
  if (server.hasArg("ip"))
    strncpy(cfg.ip, server.arg("ip").c_str(), sizeof(cfg.ip));
  if (server.hasArg("gw"))
    strncpy(cfg.gw, server.arg("gw").c_str(), sizeof(cfg.gw));
  if (server.hasArg("sn"))
    strncpy(cfg.sn, server.arg("sn").c_str(), sizeof(cfg.sn));
  
  if (server.hasArg("offset")) {
  cfg.tempOffset = server.arg("offset").toFloat();
  ProcessHandler::setValveOffset(cfg.tempOffset);
  }

  if (server.hasArg("hysteresis")) {
  cfg.hysteresis = server.arg("hysteresis").toFloat();
  ProcessHandler::setHysteresis(cfg.hysteresis);
  }
  
  if (server.hasArg("mashTime")) {
    cfg.mashTime = server.arg("mashTime").toInt() * 60;
    ProcessHandler::setMashTime(cfg.mashTime);
  }
  if (server.hasArg("mashoutTime")) {
    cfg.mashoutTime = server.arg("mashoutTime").toInt() * 60;
    ProcessHandler::setMashoutTime(cfg.mashoutTime);
  }
  if (server.hasArg("boilTime")) {
    cfg.boilTime = server.arg("boilTime").toInt() * 60;
    ProcessHandler::setBoilTime(cfg.boilTime);
  }
  if (server.hasArg("mashSetpoint")) {
    cfg.mashSetpoint = server.arg("mashSetpoint").toFloat();
    ProcessHandler::setMashSetpoint(cfg.mashSetpoint);
  }
  if (server.hasArg("mashoutSetpoint")) {
    cfg.mashoutSetpoint = server.arg("mashoutSetpoint").toFloat();
    ProcessHandler::setMashoutSetpoint(cfg.mashoutSetpoint);
  }
  EEPROMHandler::saveConfig(cfg);
  server.send(200, "text/html", "<h1>Indstillinger gemt</h1><p>Indstillingerne er blevet gemt.</p>");
}

void WebServerHandler::handleResetSettings() {
  EEPROMHandler::resetToDefaults();
  server.send(200, "text/html", "<h1>Indstillinger nulstillet</h1><p>Indstillingerne er blevet nulstillet.</p>");
  delay(1500);
  ESP.restart();
}

void WebServerHandler::handleReadSensorAddresses() {
  String grydeAddr, ventilAddr;
  if (TemperatureHandler::readActualSensorAddresses(grydeAddr, ventilAddr)) {
    String json = "{";
    json += "\"gryde\":\"" + grydeAddr + "\",";
    json += "\"ventil\":\"" + ventilAddr + "\"";
    json += "}";
    server.send(200, "application/json", json);
  } else {
    String json = "{\"error\":\"Kunne ikke læse sensoradresser.\"}";
    server.send(500, "application/json", json);
  }
}

void WebServerHandler::handleSwapSensorAddresses() {
  TemperatureHandler::swapSensorAddresses();
  String grydeAddr = TemperatureHandler::getGrydeAddressString();
  String ventilAddr = TemperatureHandler::getVentilAddressString();
  String json = "{";
  json += "\"gryde\":\"" + grydeAddr + "\",";
  json += "\"ventil\":\"" + ventilAddr + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void WebServerHandler::handleSaveSensorSettings() {
  Config cfg = EEPROMHandler::getConfig();
  
  if (server.hasArg("grydeAddress")) {
    String addr = server.arg("grydeAddress");
    addr.trim();
    if (addr.length() == 16) {
      strncpy(cfg.grydeAddress, addr.c_str(), sizeof(cfg.grydeAddress));
    }
  }
  if (server.hasArg("ventilAddress")) {
    String addr = server.arg("ventilAddress");
    addr.trim();
    if (addr.length() == 16) {
      strncpy(cfg.ventilAddress, addr.c_str(), sizeof(cfg.ventilAddress));
    }
  }
  
  EEPROMHandler::saveConfig(cfg);
  server.send(200, "text/plain", "Sensor indstillinger gemt.");
}

void WebServerHandler::handleSettings() {
  const Config &cfg = EEPROMHandler::getConfig();
  String currentGrydeAddr = TemperatureHandler::getGrydeAddressString();
  String currentVentilAddr = TemperatureHandler::getVentilAddressString();

  String html = HTML_HEADER;
  
  // Tilbage-knap til hovedsiden
  html += "<div style='text-align:left; margin-bottom:10px;'><button class='button' onclick=\"location.href='/'\">Tilbage til hovedsiden</button></div>";
  
  html += "<h2>WiFi Indstillinger</h2>";
  html += "<form action='/saveSettings' method='POST'>";
  html += "<label class='label'>SSID:</label><br/>";
  html += "<input type='text' name='ssid' value='" + String(cfg.ssid) + "'/><br/>";
  html += "<label class='label'>Password:</label><br/>";
  html += "<input type='password' name='password' value='" + String(cfg.password) + "'/><br/>";
  html += "<label class='label'>Fast IP:</label><br/>";
  html += "<input type='text' name='ip' value='" + String(cfg.ip) + "'/><br/>";
  html += "<label class='label'>Gateway:</label><br/>";
  html += "<input type='text' name='gw' value='" + String(cfg.gw) + "'/><br/>";
  html += "<label class='label'>Subnet:</label><br/>";
  html += "<input type='text' name='sn' value='" + String(cfg.sn) + "'/><br/><br/>";
  html += "<input class='button' type='submit' value='Gem WiFi Indstillinger'/>";
  html += "</form>";

  html += "<hr/>";
  html += "<h2>Sensor Indstillinger</h2>";
  html += "<form action='/saveSensorSettings' method='POST'>";
  html += "<label class='label'>Gryde Adresse (hex):</label><br/>";
  html += "<input type='text' id='grydeAddress' name='grydeAddress' value='" + currentGrydeAddr + "'/><br/>";
  html += "<label class='label'>Ventil Adresse (hex):</label><br/>";
  html += "<input type='text' id='ventilAddress' name='ventilAddress' value='" + currentVentilAddr + "'/><br/><br/>";
  html += "<p><button class='button' type='button' onclick='readSensorAddresses()'>Læs Aktuelle Sensoradresser</button></p>";
  html += "<p><button class='button' type='button' onclick='swapSensorAddresses()'>Byt Sensoradresser</button></p>";
  html += "<input class='button' type='submit' value='Gem Sensor Indstillinger'/>";
  html += "</form>";
  
  html += "<hr/>";
  html += "<div style='text-align:left; margin-bottom:10px;'>";
  html += "<button class='button' onclick=\"location.href='/update'\">Firmware-opdatering</button>&nbsp;";
  html += "<button class='button' onclick=\"location.href='/resetSettings'\">Nulstil Alle Indstillinger</button>";
  html += "</div>";
  html += "<div style='text-align:center; margin-top:20px; font-size:smaller;'>Version: " + String(SOFTWARE_VERSION) + "</div>";
  html += HTML_FOOTER;
  
  // JavaScript til sensoradresser
  html += R"html(
  <script>
    function readSensorAddresses() {
      fetch('/readSensorAddresses')
        .then(response => response.json().then(data => {
          if (!response.ok) {
            const message = data && data.error ? data.error : "Kunne ikke læse sensoradresser.";
            throw new Error(message);
          }
          return data;
        }))
        .then(data => {
          if (data.gryde && data.ventil) {
            document.getElementById('grydeAddress').value = data.gryde;
            document.getElementById('ventilAddress').value = data.ventil;
            alert("Sensoradresser opdateret i felterne. Tryk 'Gem Sensor Indstillinger' for at gemme og genstarte.");
          } else {
            throw new Error("Kunne ikke læse sensoradresser.");
          }
        })
        .catch(err => {
          alert("Fejl: " + err.message);
        });
    }
    
    function swapSensorAddresses() {
      fetch('/swapSensorAddresses')
        .then(response => response.json())
        .then(data => {
          if(data.gryde && data.ventil) {
            document.getElementById('grydeAddress').value = data.gryde;
            document.getElementById('ventilAddress').value = data.ventil;
            alert("Sensoradresser byttet. Tryk 'Gem Sensor Indstillinger' for at gemme ændringerne.");
          } else {
            alert("Kunne ikke bytte sensoradresser.");
          }
        })
        .catch(err => {
          alert("Fejl: " + err.message);
        });
    }
  </script>
  )html";
  
  server.send(200, "text/html", html);
}

void WebServerHandler::begin() {
  server.on("/", handleRoot);
  server.on("/settings", handleSettings);
  server.on("/saveSettings", HTTP_POST, handleSaveSettings);
  server.on("/resetSettings", handleResetSettings);
  server.on("/status", handleStatus);
  server.on("/togglePump", handleTogglePump);
  server.on("/toggleGasValve", handleToggleGasValve);
  server.on("/startMashing", handleStartMashing);
  server.on("/startMashout", handleStartMashout);
  server.on("/startBoiling", handleStartBoiling);
  server.on("/stopProcess", handleStopProcess);
  server.on("/pauseProcess", HTTP_GET, handlePauseProcess);
  server.on("/resumeProcess", HTTP_GET, handleResumeProcess);
  server.on("/resetProcessState", HTTP_GET, handleResetProcessState);
  server.on("/swapSensorAddresses", HTTP_GET, handleSwapSensorAddresses);
  server.on("/saveSensorSettings", HTTP_POST, handleSaveSensorSettings);
  server.on("/readSensorAddresses", HTTP_GET, handleReadSensorAddresses);
  server.on("/debug", handleDebug);

  httpUpdater.setup(&server);
  server.begin();
  Serial.println("[WebServerHandler] Webserver kører på port 80...");
}

void WebServerHandler::handleClient() {
  server.handleClient();
}
