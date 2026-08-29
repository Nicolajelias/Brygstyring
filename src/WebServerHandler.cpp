#include "WebServerHandler.h"
#include "EEPROMHandler.h"
#include "ProcessHandler.h"
#include "TemperatureHandler.h"
#include "Version.h"
#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <math.h>

WebServer WebServerHandler::server(80);
HTTPUpdateServer WebServerHandler::httpUpdater;

namespace {
const char DASHBOARD_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="da"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#110d09"><title>Stouby Bryglaug</title>
<style>
:root{color-scheme:dark;--bg:#100c08;--panel:#1d1711;--panel2:#271e15;--line:#453522;--text:#f7ead8;--muted:#b9a58c;--amber:#efad3e;--gold:#ffd27b;--green:#4dc486;--red:#ef6470;--blue:#70b9dd;font:16px system-ui,-apple-system,Segoe UI,sans-serif}*{box-sizing:border-box}body{margin:0;background:radial-gradient(circle at 75% -10%,#5a3518 0,transparent 34rem),var(--bg);color:var(--text)}main,nav{width:min(1100px,calc(100% - 2rem));margin:auto}nav{display:flex;align-items:center;gap:.6rem;padding:1.1rem 0}nav strong{font-family:Georgia,serif;font-size:1.2rem;letter-spacing:.04em;margin-right:auto;color:var(--gold)}nav a{color:var(--muted);text-decoration:none;padding:.5rem .7rem;border-radius:.55rem}nav a:hover{background:#ffffff0d;color:white}.hero,.panel{border:1px solid var(--line);background:linear-gradient(145deg,#211910ed,#17120ded);box-shadow:0 16px 50px #0005;border-radius:1rem}.hero{display:grid;grid-template-columns:1fr auto;gap:1rem;align-items:center;padding:1.25rem 1.4rem}.eyebrow{text-transform:uppercase;letter-spacing:.15em;font-size:.7rem;color:var(--amber)}h1{font:clamp(2rem,7vw,3.5rem)/1 Georgia,serif;margin:.25rem 0}.subtitle{color:var(--muted)}.badge{display:inline-flex;gap:.45rem;align-items:center;background:#3c2d1d;padding:.45rem .75rem;border-radius:99px;font-weight:700}.badge:before{content:"";width:.55rem;height:.55rem;border-radius:50%;background:var(--amber);box-shadow:0 0 12px var(--amber)}.badge.ok:before{background:var(--green);box-shadow:0 0 12px var(--green)}.badge.bad:before{background:var(--red);box-shadow:0 0 12px var(--red)}.alarm{margin:.85rem 0 0;padding:.8rem 1rem;background:#5b2025;border:1px solid #b64751;border-radius:.75rem;color:#ffdadd;font-weight:650}.hidden{display:none!important}.panel{padding:1rem;margin:1rem 0}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(190px,1fr));gap:.75rem}.metric{min-height:112px;padding:.9rem;background:linear-gradient(145deg,var(--panel2),#19130e);border:1px solid #3a2b1e;border-radius:.8rem}.metric small{display:block;color:var(--muted);margin-bottom:.4rem}.metric strong{font-size:1.6rem;font-variant-numeric:tabular-nums}.metric span{display:block;font-size:.8rem;color:#a99479;margin-top:.3rem}.metric.fault{border-color:var(--red);background:#35191a}.progress{height:.65rem;background:#342719;border-radius:2rem;overflow:hidden;margin-top:.8rem}.progress span{display:block;width:0;height:100%;background:linear-gradient(90deg,#d78125,var(--gold));transition:width .4s}.panel h2{font:1.4rem Georgia,serif;margin:.15rem 0 .9rem}.controls{display:grid;grid-template-columns:repeat(auto-fit,minmax(145px,1fr));gap:.6rem}.btn{border:1px solid #624a2b;background:#342719;color:var(--text);border-radius:.65rem;padding:.8rem;font:650 .9rem system-ui;cursor:pointer;transition:.15s}.btn:hover{transform:translateY(-1px);background:#46331f}.btn.primary{background:#9b5d1c;border-color:#d28a31}.btn.stop{background:#702c31;border-color:#b64a53}.btn:disabled{opacity:.4;cursor:not-allowed;transform:none}.details{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:.6rem}.detail{display:flex;justify-content:space-between;gap:1rem;border-bottom:1px solid #3b2d20;padding:.55rem 0}.detail span:first-child{color:var(--muted)}.settings{display:grid;grid-template-columns:repeat(auto-fit,minmax(160px,1fr));gap:.7rem}.field label{display:block;color:var(--muted);font-size:.8rem;margin-bottom:.3rem}.field input{width:100%;background:#120e0a;color:white;border:1px solid #543e29;border-radius:.55rem;padding:.7rem;font:inherit}.form-actions{display:flex;gap:.6rem;align-items:center;margin-top:.8rem}.toast{position:fixed;right:1rem;bottom:1rem;max-width:calc(100vw - 2rem);padding:.8rem 1rem;border:1px solid #785a38;background:#261c13;border-radius:.7rem;box-shadow:0 10px 35px #0008}.toast.error{border-color:var(--red)}footer{text-align:center;color:#806f5c;padding:1rem;font-size:.8rem}@media(max-width:580px){.hero{grid-template-columns:1fr}.metric strong{font-size:1.35rem}nav a{font-size:.8rem;padding:.4rem}.controls{grid-template-columns:1fr 1fr}}
</style></head><body><nav><strong>STOUBY BRYGLAUG</strong><a href="/">Brygning</a><a href="/settings">System</a></nav><main>
<section class="hero"><div><div class="eyebrow">Aktuelt procestrin</div><h1 id="step">Forbinder…</h1><div class="subtitle"><span id="process">Henter status</span> · <span id="clock">--:--:--</span></div><div class="progress"><span id="progress"></span></div></div><span id="health" class="badge">Starter</span></section>
<div id="sensorAlarm" class="alarm hidden">Temperaturdata er for gammel eller mangler. Gas lukkes automatisk under temperaturstyring.</div>
<section class="panel"><div class="grid"><div id="potCard" class="metric"><small>Gryde</small><strong id="pot">--.- °C</strong><span id="potMeta">Venter på sensor</span></div><div id="valveCard" class="metric"><small>Gasventil</small><strong id="valve">--.- °C</strong><span id="valveMeta">Venter på sensor</span></div><div class="metric"><small>Resterende tid</small><strong id="remaining">--:--</strong><span id="window">Start -- · Slut --</span></div><div class="metric"><small>Udgange</small><strong id="outputs">-- / --</strong><span>Pumpe / gas</span></div></div></section>
<section class="panel"><h2>Proces</h2><div class="controls"><button class="btn primary" data-action="startMashing">Start mæskning</button><button class="btn primary" data-action="startMashout">Start udmæskning</button><button class="btn primary" data-action="startBoiling">Start kogning</button><button class="btn" data-action="pauseProcess">Pause</button><button class="btn" data-action="resumeProcess">Fortsæt</button><button class="btn stop" data-action="stopProcess">Stop proces</button></div></section>
<section class="panel"><h2>Manuel styring</h2><div class="controls"><button class="btn" data-action="togglePump">Skift pumpe</button><button class="btn" data-action="toggleGasValve">Skift gas</button></div></section>
<section class="panel"><h2>Brygprofil</h2><form id="brewForm"><div class="settings"><div class="field"><label for="mashTime">Mæsketid · min</label><input id="mashTime" name="mashTime" type="number" min="1" max="300"></div><div class="field"><label for="mashSetpoint">Mæsketemperatur · °C</label><input id="mashSetpoint" name="mashSetpoint" type="number" min="20" max="100" step="0.1"></div><div class="field"><label for="mashoutTime">Udmæskning · min</label><input id="mashoutTime" name="mashoutTime" type="number" min="1" max="120"></div><div class="field"><label for="mashoutSetpoint">Udmæskning · °C</label><input id="mashoutSetpoint" name="mashoutSetpoint" type="number" min="20" max="100" step="0.1"></div><div class="field"><label for="boilTime">Kogetid · min</label><input id="boilTime" name="boilTime" type="number" min="1" max="300"></div><div class="field"><label for="hysteresis">Hysterese · °C</label><input id="hysteresis" name="hysteresis" type="number" min="0.1" max="10" step="0.1"></div><div class="field"><label for="offset">Ventil-offset · °C</label><input id="offset" name="offset" type="number" min="0" max="30" step="0.1"></div></div><div class="form-actions"><button class="btn primary" type="submit">Gem brygprofil</button><span id="saveState"></span></div></form></section>
<section class="panel"><h2>Detaljer</h2><div class="details"><div class="detail"><span>Firmware</span><span id="version">--</span></div><div class="detail"><span>Sensorstatus</span><span id="sensorStatus">--</span></div><div class="detail"><span>Opdatering</span><a href="/update" style="color:var(--gold)">Upload firmware</a></div><div class="detail"><span>Netværk</span><a href="/settings" style="color:var(--gold)">Indstillinger</a></div></div></section>
</main><footer>Stouby Bryglaug · lokal brygkontrol</footer><div id="toast" class="toast hidden"></div>
<script>
const $=id=>document.getElementById(id), actions=document.querySelectorAll('[data-action]');
function toast(message,error=false){const e=$('toast');e.textContent=message;e.className='toast'+(error?' error':'');setTimeout(()=>e.className='toast hidden',3500)}
const temp=v=>v===null?'--.- °C':Number(v).toFixed(1)+' °C', age=v=>v===null?'ingen gyldig måling':v<1500?'opdateret nu':'opdateret for '+Math.round(v/1000)+' s siden';
function setInput(id,value){const e=$(id);if(document.activeElement!==e)e.value=value}
function render(d){const sensorsOk=d.sensors.pot.valid&&d.sensors.valve.valid,healthy=d.sensors.pot.healthy&&d.sensors.valve.healthy;$('step').textContent=d.processStep;$('process').textContent=d.processStatus;$('clock').textContent=d.currentTime;$('health').textContent=sensorsOk?(healthy?'Sensorer OK':'Sensor retry'):'Sensorfejl';$('health').className='badge '+(healthy?'ok':sensorsOk?'':'bad');$('sensorAlarm').className='alarm '+(sensorsOk?'hidden':'');$('pot').textContent=temp(d.sensors.pot.value);$('valve').textContent=temp(d.sensors.valve.value);$('potMeta').textContent=age(d.sensors.pot.ageMs)+(d.sensors.pot.failures?' · '+d.sensors.pot.failures+' fejl':'');$('valveMeta').textContent=age(d.sensors.valve.ageMs)+(d.sensors.valve.failures?' · '+d.sensors.valve.failures+' fejl':'');$('potCard').className='metric '+(d.sensors.pot.valid?'':'fault');$('valveCard').className='metric '+(d.sensors.valve.valid?'':'fault');const sec=Number(d.timeRemaining),m=Math.floor(sec/60),s=sec%60;$('remaining').textContent=String(m).padStart(2,'0')+':'+String(s).padStart(2,'0');$('progress').style.width=(d.stepDuration?Math.max(0,Math.min(100,100-sec*100/d.stepDuration)):0)+'%';$('window').textContent='Start '+(d.startTime||'--')+' · Slut '+(d.endTime||'--');$('outputs').textContent=(d.pumpOn?'ON':'OFF')+' / '+(d.gasOn?'ÅBEN':'LUKKET');$('version').textContent=d.version;$('sensorStatus').textContent=sensorsOk?'Begge målinger brugbare':'Gas-failsafe aktiv';setInput('mashTime',d.mashTime/60);setInput('mashoutTime',d.mashoutTime/60);setInput('boilTime',d.boilTime/60);setInput('mashSetpoint',d.mashSetpoint);setInput('mashoutSetpoint',d.mashoutSetpoint);setInput('hysteresis',d.hysteresis);setInput('offset',d.valveOffset)}
async function refresh(){try{const r=await fetch('/status',{cache:'no-store'});if(!r.ok)throw Error(r.status);render(await r.json())}catch(e){$('health').textContent='Forbindelse tabt';$('health').className='badge bad'}finally{setTimeout(refresh,1000)}}
async function action(name){if((name==='stopProcess'||name==='toggleGasValve')&&!confirm(name==='stopProcess'?'Stop den aktuelle proces?':'Skift gasventilens manuelle tilstand?'))return;actions.forEach(b=>b.disabled=true);try{const r=await fetch('/'+name,{method:'POST'}),text=await r.text();if(!r.ok)throw Error(text||r.status);toast(text);refresh()}catch(e){toast(e.message,true)}finally{actions.forEach(b=>b.disabled=false)}}actions.forEach(b=>b.onclick=()=>action(b.dataset.action));
$('brewForm').onsubmit=async e=>{e.preventDefault();try{const r=await fetch('/saveSettings',{method:'POST',body:new URLSearchParams(new FormData(e.target))}),text=await r.text();if(!r.ok)throw Error(text);toast('Brygprofil gemt');$('saveState').textContent='Gemt'}catch(err){toast(err.message,true)}};refresh();
</script></body></html>)HTML";

const char SETTINGS_HEADER[] PROGMEM = R"HTML(<!doctype html><html lang="da"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>System · Brygkontrol</title><style>:root{color-scheme:dark;font:16px system-ui;background:#100c08;color:#f7ead8}*{box-sizing:border-box}body{max-width:680px;margin:auto;padding:1rem;background:radial-gradient(circle at top,#4b2c16,transparent 28rem)}section{background:#1d1711;border:1px solid #493622;border-radius:1rem;padding:1.2rem;margin:1rem 0}h1{font-family:Georgia,serif;color:#ffd27b}label{display:block;color:#b9a58c;margin-top:.8rem}input{width:100%;padding:.7rem;background:#100c08;color:white;border:1px solid #594027;border-radius:.5rem;font:inherit}button,.link{display:inline-block;margin:.8rem .4rem 0 0;padding:.7rem 1rem;border:1px solid #b27329;border-radius:.55rem;background:#895119;color:white;text-decoration:none;font:inherit;cursor:pointer}.danger{background:#702c31;border-color:#b64a53}</style></head><body><a class="link" href="/">← Brygning</a><section><h1>Systemindstillinger</h1>)HTML";

String jsonFloat(const float value, const bool valid) {
  return valid && isfinite(value) ? String(value, 2) : "null";
}

String jsonAge(const unsigned long age) {
  return age == ULONG_MAX ? "null" : String(age);
}

const char* webProcessStep() {
  switch (ProcessHandler::getCurrentState()) {
    case ProcessHandler::BrewState::MASHING: return "Mæskning";
    case ProcessHandler::BrewState::MASHOUT: return "Udmæskning";
    case ProcessHandler::BrewState::BOILHEATUP: return "Opvarmning";
    case ProcessHandler::BrewState::BOILING: return ProcessHandler::isTimerStarted() ? "Kogning" : "Venter på kogepunkt";
    case ProcessHandler::BrewState::PAUSED: return "Pause";
    default: return "Klar til brygning";
  }
}

unsigned long currentStepDuration() {
  switch (ProcessHandler::getCurrentState()) {
    case ProcessHandler::BrewState::MASHING: return ProcessHandler::getMashTime();
    case ProcessHandler::BrewState::MASHOUT: return ProcessHandler::getMashoutTime();
    case ProcessHandler::BrewState::BOILHEATUP: return 10UL * 60UL;
    case ProcessHandler::BrewState::BOILING: return ProcessHandler::getBoilTime();
    default: return 0;
  }
}

String htmlEscape(const char* value) {
  String out;
  if (!value) return out;
  while (*value) {
    switch (*value++) {
      case '&': out += F("&amp;"); break;
      case '<': out += F("&lt;"); break;
      case '>': out += F("&gt;"); break;
      case '"': out += F("&quot;"); break;
      case '\'': out += F("&#39;"); break;
      default: out += value[-1];
    }
  }
  return out;
}

void copyString(char* destination, const size_t capacity, const String& value) {
  if (capacity == 0) return;
  value.substring(0, capacity - 1).toCharArray(destination, capacity);
  destination[capacity - 1] = '\0';
}
}  // namespace

void WebServerHandler::handleRoot() { server.send_P(200, "text/html; charset=utf-8", DASHBOARD_PAGE); }

void WebServerHandler::handleStatus() {
  const bool potValid = TemperatureHandler::isGrydeValid();
  const bool valveValid = TemperatureHandler::isVentilValid();
  String json;
  json.reserve(1100);
  json += F("{\"sensors\":{\"pot\":{\"value\":");
  json += jsonFloat(TemperatureHandler::getGrydeTemp(), potValid);
  json += F(",\"valid\":"); json += potValid ? F("true") : F("false");
  json += F(",\"healthy\":"); json += TemperatureHandler::isGrydeHealthy() ? F("true") : F("false");
  json += F(",\"failures\":"); json += TemperatureHandler::getGrydeFailureCount();
  json += F(",\"ageMs\":"); json += jsonAge(TemperatureHandler::getGrydeAgeMs());
  json += F("},\"valve\":{\"value\":");
  json += jsonFloat(TemperatureHandler::getVentilTemp(), valveValid);
  json += F(",\"valid\":"); json += valveValid ? F("true") : F("false");
  json += F(",\"healthy\":"); json += TemperatureHandler::isVentilHealthy() ? F("true") : F("false");
  json += F(",\"failures\":"); json += TemperatureHandler::getVentilFailureCount();
  json += F(",\"ageMs\":"); json += jsonAge(TemperatureHandler::getVentilAgeMs());
  json += F("}},\"currentTime\":\""); json += ProcessHandler::getFormattedTime();
  json += F("\",\"processStep\":\""); json += webProcessStep();
  json += F("\",\"processStatus\":\""); json += ProcessHandler::getProcessStatus();
  json += F("\",\"startTime\":\""); json += ProcessHandler::getStartTime();
  json += F("\",\"endTime\":\""); json += ProcessHandler::getEndTime();
  json += F("\",\"timeRemaining\":"); json += ProcessHandler::getRemainingTime();
  json += F(",\"stepDuration\":"); json += currentStepDuration();
  json += F(",\"pumpOn\":"); json += ProcessHandler::isPumpOn() ? F("true") : F("false");
  json += F(",\"gasOn\":"); json += ProcessHandler::isGasValveOn() ? F("true") : F("false");
  json += F(",\"mashTime\":"); json += ProcessHandler::getMashTime();
  json += F(",\"mashoutTime\":"); json += ProcessHandler::getMashoutTime();
  json += F(",\"boilTime\":"); json += ProcessHandler::getBoilTime();
  json += F(",\"mashSetpoint\":"); json += String(ProcessHandler::getMashSetpoint(), 1);
  json += F(",\"mashoutSetpoint\":"); json += String(ProcessHandler::getMashoutSetpoint(), 1);
  json += F(",\"hysteresis\":"); json += String(ProcessHandler::getHysteresis(), 1);
  json += F(",\"valveOffset\":"); json += String(ProcessHandler::getValveOffset(), 1);
  json += F(",\"version\":\""); json += SOFTWARE_VERSION; json += F("\"}");
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json; charset=utf-8", json);
}

void WebServerHandler::handleDebug() {
  String info = EEPROMHandler::getConfigAsString();
  server.send(200, "text/plain; charset=utf-8", info);
}

void WebServerHandler::handleTogglePump() { server.send(200, "text/plain; charset=utf-8", ProcessHandler::togglePump() ? "Pumpe tændt" : "Pumpe slukket"); }
void WebServerHandler::handleToggleGasValve() { server.send(200, "text/plain; charset=utf-8", ProcessHandler::toggleGasValve() ? "Gas åben" : "Gas lukket (kræver gyldige sensorer)"); }
void WebServerHandler::handleStartMashing() {
  if (!TemperatureHandler::isGrydeValid() || !TemperatureHandler::isVentilValid()) { server.send(409, "text/plain; charset=utf-8", "Kan ikke starte: temperaturdata mangler"); return; }
  ProcessHandler::startMashing(); server.send(200, "text/plain; charset=utf-8", "Mæskning startet");
}
void WebServerHandler::handleStartMashout() {
  if (!TemperatureHandler::isGrydeValid() || !TemperatureHandler::isVentilValid()) { server.send(409, "text/plain; charset=utf-8", "Kan ikke starte: temperaturdata mangler"); return; }
  ProcessHandler::startMashout(); server.send(200, "text/plain; charset=utf-8", "Udmæskning startet");
}
void WebServerHandler::handleStartBoiling() {
  if (!TemperatureHandler::isGrydeValid() || !TemperatureHandler::isVentilValid()) { server.send(409, "text/plain; charset=utf-8", "Kan ikke starte: temperaturdata mangler"); return; }
  ProcessHandler::startBoiling(); server.send(200, "text/plain; charset=utf-8", "Kogning startet");
}
void WebServerHandler::handleStopProcess() { ProcessHandler::stopProcess(); server.send(200, "text/plain; charset=utf-8", "Proces stoppet"); }
void WebServerHandler::handlePauseProcess() { ProcessHandler::pauseProcess(); server.send(200, "text/plain; charset=utf-8", "Proces pauset"); }
void WebServerHandler::handleResumeProcess() { ProcessHandler::resumeProcess(); server.send(200, "text/plain; charset=utf-8", "Proces genoptaget"); }
void WebServerHandler::handleResetProcessState() { ProcessHandler::resetProcessState(); server.send(200, "text/plain; charset=utf-8", "Procesdata nulstillet"); }

void WebServerHandler::handleSaveSettings() {
  Config cfg = EEPROMHandler::getConfig();
  auto ranged = [&](const char* name, float minimum, float maximum, float& output) {
    if (!server.hasArg(name)) return true;
    const String raw = server.arg(name);
    char* end = nullptr;
    const float value = strtof(raw.c_str(), &end);
    if (end == raw.c_str() || *end != '\0' || !isfinite(value) || value < minimum || value > maximum) return false;
    output = value;
    return true;
  };
  float offset = cfg.tempOffset, hysteresis = cfg.hysteresis, mashSp = cfg.mashSetpoint, mashoutSp = cfg.mashoutSetpoint;
  float mashMinutes = cfg.mashTime / 60.0f, mashoutMinutes = cfg.mashoutTime / 60.0f, boilMinutes = cfg.boilTime / 60.0f;
  if (!ranged("offset", 0, 30, offset) || !ranged("hysteresis", 0.1f, 10, hysteresis) ||
      !ranged("mashSetpoint", 20, 100, mashSp) || !ranged("mashoutSetpoint", 20, 100, mashoutSp) ||
      !ranged("mashTime", 1, 300, mashMinutes) || !ranged("mashoutTime", 1, 120, mashoutMinutes) ||
      !ranged("boilTime", 1, 300, boilMinutes)) {
    server.send(400, "text/plain; charset=utf-8", "Ugyldig værdi i formularen");
    return;
  }
  if (server.hasArg("ssid")) copyString(cfg.ssid, sizeof(cfg.ssid), server.arg("ssid"));
  if (server.hasArg("password") && !server.arg("password").isEmpty()) copyString(cfg.password, sizeof(cfg.password), server.arg("password"));
  if (server.hasArg("ip")) copyString(cfg.ip, sizeof(cfg.ip), server.arg("ip"));
  if (server.hasArg("gw")) copyString(cfg.gw, sizeof(cfg.gw), server.arg("gw"));
  if (server.hasArg("sn")) copyString(cfg.sn, sizeof(cfg.sn), server.arg("sn"));
  cfg.tempOffset = offset; cfg.hysteresis = hysteresis; cfg.mashSetpoint = mashSp; cfg.mashoutSetpoint = mashoutSp;
  cfg.mashTime = lroundf(mashMinutes * 60); cfg.mashoutTime = lroundf(mashoutMinutes * 60); cfg.boilTime = lroundf(boilMinutes * 60);
  EEPROMHandler::saveConfig(cfg);
  ProcessHandler::setValveOffset(offset); ProcessHandler::setHysteresis(hysteresis);
  ProcessHandler::setMashSetpoint(mashSp); ProcessHandler::setMashoutSetpoint(mashoutSp);
  ProcessHandler::setMashTime(cfg.mashTime); ProcessHandler::setMashoutTime(cfg.mashoutTime); ProcessHandler::setBoilTime(cfg.boilTime);
  server.send(200, "text/plain; charset=utf-8", "Indstillinger gemt");
}

void WebServerHandler::handleResetSettings() {
  EEPROMHandler::resetToDefaults();
  server.send(200, "text/plain; charset=utf-8", "Indstillinger nulstillet; genstarter");
  delay(500);
  ESP.restart();
}

void WebServerHandler::handleSettings() {
  const Config cfg = EEPROMHandler::getConfig();
  String html = FPSTR(SETTINGS_HEADER);
  html.reserve(3500);
  html += F("<form method='post' action='/saveSettings'><label>WiFi-navn (SSID)</label><input name='ssid' maxlength='31' value='"); html += htmlEscape(cfg.ssid);
  html += F("'><label>Ny adgangskode (tom = behold nuværende)</label><input name='password' type='password' maxlength='31' autocomplete='new-password'><label>Fast IP (tom = DHCP)</label><input name='ip' maxlength='15' value='"); html += htmlEscape(cfg.ip);
  html += F("'><label>Gateway</label><input name='gw' maxlength='15' value='"); html += htmlEscape(cfg.gw);
  html += F("'><label>Subnetmaske</label><input name='sn' maxlength='15' value='"); html += htmlEscape(cfg.sn);
  html += F("'><button type='submit'>Gem netværk</button></form></section><section><h2>Vedligeholdelse</h2><a class='link' href='/update'>Firmware-opdatering</a><form method='post' action='/resetSettings' onsubmit=\"return confirm('Nulstil alle indstillinger?')\"><button class='danger'>Nulstil alt</button></form><p>Firmware ");
  html += SOFTWARE_VERSION;
  html += F("</p></section></body></html>");
  server.send(200, "text/html; charset=utf-8", html);
}

void WebServerHandler::begin() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/settings", HTTP_GET, handleSettings);
  server.on("/saveSettings", HTTP_POST, handleSaveSettings);
  server.on("/resetSettings", HTTP_POST, handleResetSettings);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/togglePump", HTTP_POST, handleTogglePump);
  server.on("/toggleGasValve", HTTP_POST, handleToggleGasValve);
  server.on("/startMashing", HTTP_POST, handleStartMashing);
  server.on("/startMashout", HTTP_POST, handleStartMashout);
  server.on("/startBoiling", HTTP_POST, handleStartBoiling);
  server.on("/stopProcess", HTTP_POST, handleStopProcess);
  server.on("/pauseProcess", HTTP_POST, handlePauseProcess);
  server.on("/resumeProcess", HTTP_POST, handleResumeProcess);
  server.on("/resetProcessState", HTTP_POST, handleResetProcessState);
  server.on("/debug", HTTP_GET, handleDebug);
  server.onNotFound([] { WebServerHandler::server.send(404, "text/plain", "Not found"); });
  httpUpdater.setup(&server);
  server.begin();
  Serial.println("[WebServer] Listening on port 80");
}

void WebServerHandler::handleClient() { server.handleClient(); }
