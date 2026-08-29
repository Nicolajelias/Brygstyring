#include "OTAHandler.h"

#include <Preferences.h>
#include <Update.h>
#include <cstring>
#include <esp_app_format.h>
#include <esp_ota_ops.h>

#include "FirmwareInfo.h"
#include "OtaCredentials.h"
#include "OtaPolicy.h"
#include "ProcessHandler.h"

namespace {
OTAHandler::Status status;
WebServer* webServer = nullptr;
Preferences preferences;
unsigned long bootStartedAt = 0;
unsigned long rebootAt = 0;
unsigned long authWindowStartedAt = 0;
uint32_t healthyLoopCount = 0;
uint8_t authenticationFailures = 0;
bool requestAuthenticated = false;
bool uploadStarted = false;
bool uploadFinished = false;
size_t expectedUploadSize = 0;
uint8_t imageHeader[sizeof(esp_image_header_t)] = {};
size_t imageHeaderBytes = 0;
bool imageHeaderValidated = false;

constexpr char PREFERENCES_NAMESPACE[] = "ota";
constexpr char RESULT_KEY[] = "last_result";
constexpr char AUTH_REALM[] = "Brygkontrol OTA";

const char UPDATE_PAGE[] PROGMEM = R"HTML(<!doctype html><html lang="da"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#100c08"><title>Firmwareopdatering · Brygkontrol</title>
<style>:root{color-scheme:dark;--bg:#100c08;--line:#4b3925;--gold:#ffd27b;--amber:#d9892c;--green:#4dc486;--red:#ef6470;--muted:#b9a58c;font:16px system-ui,-apple-system,Segoe UI,sans-serif}*{box-sizing:border-box}body{min-height:100vh;margin:0;display:grid;place-items:center;padding:1rem;background:radial-gradient(circle at 80% 0,#5b3518 0,transparent 30rem),var(--bg);color:#f7ead8}.card{width:min(620px,100%);padding:1.4rem;background:linear-gradient(145deg,#251b12ed,#17110ced);border:1px solid var(--line);border-radius:1.1rem;box-shadow:0 20px 70px #0008}a{color:var(--gold)}h1{font:clamp(2rem,8vw,3rem)/1 Georgia,serif;margin:.65rem 0}.meta,.hint{color:var(--muted)}.status{margin:1rem 0;padding:.85rem;border:1px solid var(--line);border-radius:.7rem;background:#120e0a}.status.bad{border-color:var(--red);color:#ffd9dc}.status.ok{border-color:var(--green)}input{width:100%;padding:.85rem;border:1px dashed #76552f;border-radius:.7rem;background:#130e09;color:white}button{width:100%;margin-top:.8rem;padding:.85rem;border:1px solid #d28a31;border-radius:.7rem;background:linear-gradient(135deg,#a6611d,var(--amber));color:white;font:700 1rem system-ui;cursor:pointer}button:disabled{opacity:.45;cursor:not-allowed}.bar{height:.75rem;margin-top:1rem;background:#332617;border-radius:2rem;overflow:hidden}.bar span{display:block;width:0;height:100%;background:linear-gradient(90deg,var(--amber),var(--gold));transition:width .2s}</style></head><body><main class="card"><a href="/">← Brygning</a><h1>Sikker firmwareopdatering</h1><p class="meta">Installeret: <strong>%VERSION%</strong> · build %BUILD% · git %GIT%</p><div id="state" class="status">%READY%</div><form id="form"><input id="file" name="firmware" type="file" accept=".bin,application/octet-stream" required><button id="submit" type="submit" %DISABLED%>Kontrollér og installer</button></form><div class="bar"><span id="bar"></span></div><p class="hint">Kun firmware til ESP32-S3 accepteres. Enheden genstarter først, når hele firmwarefilen er valideret. Opdatering er kun mulig, når brygprocessen er stoppet, og pumpe og gas er slukket.</p><script>
const form=document.getElementById('form'),file=document.getElementById('file'),button=document.getElementById('submit'),state=document.getElementById('state'),bar=document.getElementById('bar');
form.addEventListener('submit',event=>{event.preventDefault();if(!file.files.length)return;button.disabled=true;state.className='status';state.textContent='Uploader firmware…';const data=new FormData();data.append('firmware',file.files[0]);const xhr=new XMLHttpRequest();xhr.open('POST','/update');xhr.upload.onprogress=e=>{if(e.lengthComputable){const p=Math.round(e.loaded*100/e.total);bar.style.width=p+'%';state.textContent='Uploader firmware… '+p+'%'}};xhr.onload=()=>{let result={};try{result=JSON.parse(xhr.responseText)}catch(e){}if(xhr.status>=200&&xhr.status<300&&result.ok){bar.style.width='100%';state.className='status ok';state.textContent='Firmware godkendt. Enheden genstarter nu…'}else{state.className='status bad';state.textContent=result.error||xhr.responseText||'Opdateringen mislykkedes';button.disabled=false}};xhr.onerror=()=>{state.className='status bad';state.textContent='Forbindelsen blev afbrudt';button.disabled=false};xhr.send(data)});
</script></main></body></html>)HTML";

void persistResult(const String& result) {
  status.lastResult = result;
  preferences.putString(RESULT_KEY, result);
}

void fail(const String& message) {
  status.state = OTAHandler::State::FAILED;
  status.error = message;
  uploadFinished = false;
  if (Update.isRunning()) Update.abort();
  persistResult("failed:" + message);
  Serial.printf("[OTA] Fejl: %s\n", message.c_str());
}

bool processIsSafe() {
  return ProcessHandler::getCurrentState() == ProcessHandler::BrewState::IDLE &&
         !ProcessHandler::isPumpOn() && !ProcessHandler::isGasValveOn();
}

void addSecurityHeaders() {
  webServer->sendHeader("Cache-Control", "no-store");
  webServer->sendHeader("X-Content-Type-Options", "nosniff");
  webServer->sendHeader("X-Frame-Options", "DENY");
}

bool authenticate() {
  if (!status.authConfigured) {
    addSecurityHeaders();
    webServer->send(503, "text/plain; charset=utf-8",
                    "OTA er deaktiveret: opret include/secrets.hpp fra secrets.example.hpp");
    return false;
  }
  const unsigned long now = millis();
  if (authenticationFailures >= OtaPolicy::MAX_AUTH_FAILURES &&
      now - authWindowStartedAt >= OtaPolicy::AUTH_LOCKOUT_MS) {
    authenticationFailures = 0;
    authWindowStartedAt = now;
  }
  if (OtaPolicy::authenticationLocked(authenticationFailures, now - authWindowStartedAt)) {
    addSecurityHeaders();
    webServer->send(429, "text/plain; charset=utf-8", "For mange loginforsøg. Prøv igen om et minut.");
    return false;
  }
  if (webServer->authenticate(OTA_USERNAME, OTA_PASSWORD)) {
    authenticationFailures = 0;
    return true;
  }
  if (authenticationFailures == 0) authWindowStartedAt = now;
  ++authenticationFailures;
  webServer->requestAuthentication(DIGEST_AUTH, AUTH_REALM, "Login kræves til firmwareopdatering");
  return false;
}

void handlePage() {
  if (!authenticate()) return;
  String page = FPSTR(UPDATE_PAGE);
  page.replace("%VERSION%", FIRMWARE_INFO.version);
  page.replace("%BUILD%", String(FIRMWARE_INFO.buildNumber));
  page.replace("%GIT%", FIRMWARE_INFO.gitHash);
  const bool safe = processIsSafe();
  page.replace("%READY%", safe ? "Klar til opdatering" : "Stop processen og sluk pumpe/gas før opdatering");
  page.replace("%DISABLED%", safe ? "" : "disabled");
  addSecurityHeaders();
  webServer->send(200, "text/html; charset=utf-8", page);
}

void handleUpload() {
  HTTPUpload& upload = webServer->upload();
  if (upload.status == UPLOAD_FILE_START) {
    status.error = "";
    status.progress = 0;
    uploadFinished = false;
    uploadStarted = false;
    expectedUploadSize = webServer->header("Content-Length").toInt();
    imageHeaderBytes = 0;
    imageHeaderValidated = false;
    requestAuthenticated = authenticate();
    if (!requestAuthenticated) return;
    if (!processIsSafe()) {
      fail("Brygprocessen skal være stoppet, og pumpe/gas skal være slukket");
      return;
    }
    const esp_partition_t* target = esp_ota_get_next_update_partition(nullptr);
    if (!target) {
      fail("Ingen ledig OTA-partition fundet");
      return;
    }
    if (!Update.begin(target->size, U_FLASH)) {
      fail("Kunne ikke starte flash-skrivning: " + String(Update.errorString()));
      return;
    }
    uploadStarted = true;
    status.state = OTAHandler::State::UPLOADING;
    Serial.printf("[OTA] Upload startet: %s, maks. %u bytes\n", upload.filename.c_str(), target->size);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (!requestAuthenticated || !uploadStarted || status.state == OTAHandler::State::FAILED) return;
    size_t offset = 0;
    if (!imageHeaderValidated) {
      const size_t needed = sizeof(imageHeader) - imageHeaderBytes;
      const size_t copied = upload.currentSize < needed ? upload.currentSize : needed;
      memcpy(imageHeader + imageHeaderBytes, upload.buf, copied);
      imageHeaderBytes += copied;
      offset += copied;
      if (imageHeaderBytes < sizeof(imageHeader)) return;
      esp_image_header_t header{};
      memcpy(&header, imageHeader, sizeof(header));
      if (header.magic != ESP_IMAGE_HEADER_MAGIC || header.chip_id != ESP_CHIP_ID_ESP32S3) {
        fail("Firmwarefilen er ikke et gyldigt ESP32-S3-image");
        return;
      }
      if (Update.write(imageHeader, sizeof(imageHeader)) != sizeof(imageHeader)) {
        fail("Fejl under flash-skrivning: " + String(Update.errorString()));
        return;
      }
      imageHeaderValidated = true;
    }
    const size_t remaining = upload.currentSize - offset;
    if (remaining && Update.write(upload.buf + offset, remaining) != remaining) {
      fail("Fejl under flash-skrivning: " + String(Update.errorString()));
      return;
    }
    const size_t received = upload.totalSize + upload.currentSize;
    status.progress = expectedUploadSize ? static_cast<uint8_t>(min<size_t>(99, received * 100 / expectedUploadSize)) : 0;
  } else if (upload.status == UPLOAD_FILE_END) {
    if (!requestAuthenticated || !uploadStarted || status.state == OTAHandler::State::FAILED) return;
    if (!imageHeaderValidated) {
      fail("Firmwarefilen er tom eller mangler en gyldig image-header");
      return;
    }
    status.state = OTAHandler::State::VALIDATING;
    if (!Update.end(true)) {
      fail("Firmwarevalidering fejlede: " + String(Update.errorString()));
      return;
    }
    uploadFinished = true;
    status.progress = 100;
    status.state = OTAHandler::State::SUCCESS;
    persistResult("installed:" + String(FIRMWARE_INFO.version));
    rebootAt = millis() + 2000UL;
    Serial.printf("[OTA] Firmware valideret: %u bytes. Genstarter.\n", upload.totalSize);
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    if (uploadStarted) fail("Upload blev afbrudt");
  }
  delay(0);
}

void handleResult() {
  if (!requestAuthenticated) {
    if (status.authConfigured) webServer->requestAuthentication(DIGEST_AUTH, AUTH_REALM);
    return;
  }
  addSecurityHeaders();
  if (OtaPolicy::successfulUploadMayReboot(uploadFinished, status.state == OTAHandler::State::FAILED)) {
    webServer->send(200, "application/json; charset=utf-8", "{\"ok\":true}");
  } else {
    const String message = status.error.length() ? status.error : "Opdateringen blev ikke fuldført";
    webServer->send(400, "application/json; charset=utf-8", "{\"ok\":false,\"error\":\"" + message + "\"}");
  }
}
}  // namespace

void OTAHandler::begin() {
  bootStartedAt = millis();
  preferences.begin(PREFERENCES_NAMESPACE, false);
  status.lastResult = preferences.getString(RESULT_KEY, "ingen tidligere opdatering");
  status.authConfigured = OtaCredentials::configured();
  const esp_partition_t* running = esp_ota_get_running_partition();
  esp_ota_img_states_t imageState = ESP_OTA_IMG_UNDEFINED;
  status.rollbackSupported = running && esp_ota_get_state_partition(running, &imageState) == ESP_OK;
  status.pendingBootValidation = status.rollbackSupported && imageState == ESP_OTA_IMG_PENDING_VERIFY;
  Serial.printf("[OTA] Klar. Login: %s, rollback: %s%s\n",
                status.authConfigured ? "konfigureret" : "MANGLER",
                status.rollbackSupported ? "ja" : "nej",
                status.pendingBootValidation ? " (afventer boot-validering)" : "");
}

void OTAHandler::registerRoutes(WebServer& server) {
  webServer = &server;
  const char* headers[] = {"Authorization", "Content-Length"};
  server.collectHeaders(headers, 2);
  server.on("/update", HTTP_GET, handlePage);
  server.on("/update", HTTP_POST, handleResult, handleUpload);
}

void OTAHandler::update(unsigned long now) {
  ++healthyLoopCount;
  if (OtaPolicy::bootValidationReady(status.pendingBootValidation, now - bootStartedAt,
                                     healthyLoopCount, ESP.getFreeHeap())) {
    if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
      status.pendingBootValidation = false;
      persistResult("boot_validated:" + String(FIRMWARE_INFO.version));
      Serial.println("[OTA] Ny firmware markeret som stabil");
    }
  }
  if (status.state == State::SUCCESS && static_cast<int32_t>(now - rebootAt) >= 0) {
    status.state = State::REBOOTING;
    delay(100);
    ESP.restart();
  }
}

bool OTAHandler::maintenanceActive() {
  return status.state == State::UPLOADING || status.state == State::VALIDATING ||
         status.state == State::SUCCESS || status.state == State::REBOOTING;
}

const OTAHandler::Status& OTAHandler::getStatus() { return status; }

const char* OTAHandler::stateName(State state) {
  switch (state) {
    case State::IDLE: return "idle";
    case State::UPLOADING: return "uploading";
    case State::VALIDATING: return "validating";
    case State::SUCCESS: return "success";
    case State::FAILED: return "failed";
    case State::REBOOTING: return "rebooting";
  }
  return "unknown";
}
