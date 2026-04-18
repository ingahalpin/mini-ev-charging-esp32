#include <WiFi.h>
#include <WebServer.h>
#include <LCD_ST7032.h>
#include <Preferences.h>

LCD_ST7032 lcd;
Preferences prefs;

// ---------- Pins ----------
const int LED_PIN_RED = 23;
const int LED_PIN_GREEN = 4;
const int BUTTON_PIN = 18;

// ---------- State Machine ----------
enum ChargerState {
  IDLE,
  READY,
  CHARGING,
  CHARGING_STOPPED,
  CHARGING_COMPLETE
};

ChargerState state = IDLE;

// ---------- LED Blink ----------
unsigned long lastBlinkTime = 0;
const unsigned long blinkInterval = 500;
bool blinkToggle = false;

// ---------- Button ----------
const unsigned long debounceDelay = 50;

// ---------- Simulation ----------
float kWh = 0.0;
float maxKWh     = 1.0;   // Feature 1: configurable charge limit
float pricePerKWh = 0.35; // Feature 3: configurable price
float maxCost    = 0.0;   // Feature 7: cost cap (0 = disabled)

// ---------- Session Timer ----------
unsigned long chargingStartMs = 0;
uint32_t sessionElapsedSeconds = 0;

// ---------- Wall Clock (browser-synced) ----------
uint32_t syncedEpoch = 0;      // UTC Unix timestamp received from browser
unsigned long syncedAtMs = 0;  // millis() at the moment of sync
uint32_t sessionStartTime = 0; // UTC epoch when current charging session began

uint32_t getCurrentTime() {
  if (syncedEpoch == 0) return 0;
  return syncedEpoch + (uint32_t)((millis() - syncedAtMs) / 1000UL);
}

// ---------- WiFi AP ----------
const char* ssid = "EV-Charger";
const char* password = "12345678";

// ---------- Web Server ----------
WebServer server(80);

// ---------- Session History (NVS) ----------
const int MAX_HISTORY = 5;

void saveSession(float kwh, float cost, uint32_t duration_s, uint32_t startTime, uint32_t endTime) {
  prefs.begin("history", false);
  uint8_t count = prefs.getUChar("count", 0);

  // Shift existing sessions down (newest at index 0)
  int shiftTo = min((int)count, MAX_HISTORY - 1) - 1;
  for (int i = shiftTo; i >= 0; i--) {
    char fromKwh[8],  toKwh[8],  fromCost[9], toCost[9];
    char fromDur[8],  toDur[8],  fromStart[9], toStart[9], fromEnd[7], toEnd[7];
    sprintf(fromKwh,   "s%dkwh",   i);  sprintf(toKwh,   "s%dkwh",   i + 1);
    sprintf(fromCost,  "s%dcost",  i);  sprintf(toCost,  "s%dcost",  i + 1);
    sprintf(fromDur,   "s%ddur",   i);  sprintf(toDur,   "s%ddur",   i + 1);
    sprintf(fromStart, "s%dstart", i);  sprintf(toStart, "s%dstart", i + 1);
    sprintf(fromEnd,   "s%dend",   i);  sprintf(toEnd,   "s%dend",   i + 1);
    prefs.putFloat(toKwh,   prefs.getFloat(fromKwh,  0));
    prefs.putFloat(toCost,  prefs.getFloat(fromCost, 0));
    prefs.putUInt(toDur,    prefs.getUInt(fromDur,   0));
    prefs.putUInt(toStart,  prefs.getUInt(fromStart, 0));
    prefs.putUInt(toEnd,    prefs.getUInt(fromEnd,   0));
  }

  prefs.putFloat("s0kwh",   kwh);
  prefs.putFloat("s0cost",  cost);
  prefs.putUInt("s0dur",    duration_s);
  prefs.putUInt("s0start",  startTime);
  prefs.putUInt("s0end",    endTime);

  if (count < MAX_HISTORY) prefs.putUChar("count", count + 1);
  prefs.end();
}

// ---------- State Helpers ----------
String getStateName() {
  switch (state) {
    case IDLE:              return "Idle";
    case READY:             return "Ready!";
    case CHARGING:          return "Charging...";
    case CHARGING_STOPPED:  return "Charging stopped";
    case CHARGING_COMPLETE: return "Charging complete";
    default:                return "Unknown";
  }
}

String formatDuration(uint32_t seconds) {
  char buf[8];
  sprintf(buf, "%u:%02u", seconds / 60, seconds % 60);
  return String(buf);
}

// Convert UTC epoch to "DD.MM.YY HH:MM"
String formatDateTime(uint32_t epoch) {
  if (epoch == 0) return "--.--.-- --:--";
  uint32_t t    = epoch;
  uint32_t min  = (t / 60) % 60;
  uint32_t hour = (t / 3600) % 24;
  uint32_t days = t / 86400;
  uint16_t year = 1970;
  while (true) {
    bool leap    = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    uint16_t diy = leap ? 366 : 365;
    if (days < diy) break;
    days -= diy; year++;
  }
  const uint8_t dim[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  bool leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
  uint8_t month = 0;
  for (month = 0; month < 12; month++) {
    uint8_t d = (month == 1 && leap) ? 29 : dim[month];
    if (days < d) break;
    days -= d;
  }
  char buf[15];
  sprintf(buf, "%02u.%02u.%02u %02u:%02u", (uint32_t)(days + 1), (uint32_t)(month + 1), year % 100, hour, min);
  return String(buf);
}

// Extract just "HH:MM" from an epoch (for end-time display on the same day)
String formatTime(uint32_t epoch) {
  if (epoch == 0) return "--:--";
  char buf[6];
  sprintf(buf, "%02u:%02u", (epoch / 3600) % 24, (epoch / 60) % 60);
  return String(buf);
}

void transitionTo(ChargerState newState) {
  if (newState == CHARGING) {
    chargingStartMs = millis();
    sessionElapsedSeconds = 0;
    sessionStartTime = getCurrentTime(); // record wall-clock start of fresh session
  }
  if (newState == CHARGING_STOPPED || newState == CHARGING_COMPLETE) {
    saveSession(kWh, kWh * pricePerKWh, sessionElapsedSeconds, sessionStartTime, getCurrentTime());
  }
  if (newState == IDLE || newState == READY) {
    kWh = 0.0;
    sessionElapsedSeconds = 0;
    sessionStartTime = 0;
  }
  state = newState;
  Serial.print("State -> ");
  Serial.println(getStateName());
}

// Resume from CHARGING_STOPPED: keep kWh and continue the timer from where it paused
void resumeCharging() {
  chargingStartMs = millis() - (unsigned long)(sessionElapsedSeconds * 1000UL);
  state = CHARGING;
  Serial.println("State -> Charging... (resumed)");
}

// ---------- HTML helpers ----------
String getActionButton() {
  switch (state) {
    case IDLE:
      return "<a href='/ready'><button class='btn' style='background:#f0ad00;'>Ready!</button></a>";
    case READY:
      return "<a href='/start'><button class='btn' style='background:#28a745;'>Start Charging</button></a>";
    case CHARGING:
      return "<a href='/stop'><button class='btn' style='background:#dc3545;'>Stop Charging</button></a>";
    case CHARGING_STOPPED:
      return "<a href='/resume'><button class='btn' style='background:#28a745;'>Resume Charging</button></a>"
             "&nbsp;&nbsp;"
             "<a href='/reset'><button class='btn' style='background:#6c757d;'>Finish</button></a>";
    case CHARGING_COMPLETE:
      return "<a href='/reset'><button class='btn' style='background:#6c757d;'>Finish</button></a>";
    default:
      return "";
  }
}

String getSettingsHTML() {
  String html = "<!DOCTYPE html><html><head>"
                "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                "<title>Settings</title>"
                "<style>"
                "body{font-family:sans-serif;max-width:400px;margin:40px auto;padding:0 16px;}"
                ".btn-sm{font-size:14px;padding:4px 10px;border:none;border-radius:4px;cursor:pointer;background:#007bff;color:#fff;}"
                "label{font-size:15px;}"
                "hr{margin:16px 0;}"
                "</style></head><body>";
  html += "<h2>Settings</h2><p><a href='/'>&#8592; Back</a></p><hr>";

  html += "<form action='/setlimit' method='get'>";
  html += "<label>Charge limit (kWh):<br>";
  html += "<input type='number' name='kwh' step='0.1' min='0.1' max='100' value='" + String(maxKWh, 1) + "' style='width:80px;'></label> ";
  html += "<button type='submit' class='btn-sm'>Set</button></form><br>";

  html += "<form action='/setprice' method='get'>";
  html += "<label>Price (EUR / kWh):<br>";
  html += "<input type='number' name='eur' step='0.01' min='0.01' max='9.99' value='" + String(pricePerKWh, 2) + "' style='width:80px;'></label> ";
  html += "<button type='submit' class='btn-sm'>Set</button></form><br>";

  html += "<form action='/setcostcap' method='get'>";
  html += "<label>Cost cap EUR (0 = off):<br>";
  html += "<input type='number' name='eur' step='0.01' min='0' max='999' value='" + String(maxCost, 2) + "' style='width:80px;'></label> ";
  html += "<button type='submit' class='btn-sm'>Set</button></form>";

  html += "</body></html>";
  return html;
}

String getHTML() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>EV Charger</title>";
  html += "<style>"
          "body{font-family:sans-serif;max-width:400px;margin:40px auto;padding:0 16px;}"
          ".btn{font-size:20px;color:#fff;padding:12px 24px;border:none;border-radius:6px;cursor:pointer;}"
          ".btn-sm{font-size:14px;padding:4px 10px;border:none;border-radius:4px;cursor:pointer;background:#007bff;color:#fff;}"
          "label{font-size:15px;}"
          "</style></head><body>";

  html += "<h2>Mini EV Charger</h2>";

  // Live data region — individual spans updated by JS polling
  html += "<p>Status: <b id='s-state'>" + getStateName() + "</b></p>";

  html += "<p>Energy: <span id='s-kwh'>" + String(kWh, 2) + "</span> kWh";
  html += " <span style='color:#888;font-size:14px;'>/ " + String(maxKWh, 1) + " kWh limit</span></p>";

  html += "<p>Cost: EUR <span id='s-cost'>" + String(kWh * pricePerKWh, 2) + "</span>";
  if (maxCost > 0.0) html += " <span style='color:#888;font-size:14px;'>/ EUR " + String(maxCost, 2) + " cap</span>";
  html += "</p>";

  html += "<p>Duration: <span id='s-dur'>" + formatDuration(sessionElapsedSeconds) + "</span></p>";

  html += "<hr><div id='s-btn'>" + getActionButton() + "</div>";

  html += "<hr><p><a href='/settings'>Settings</a> &nbsp;&middot;&nbsp; <a href='/history'>Session History</a></p>";

  // JS: sync browser time on load, then poll /api/status every second
  html += "<script>"
          "fetch('/synctime?ts='+Math.floor(Date.now()/1000));"
          "var lastState=document.getElementById('s-state').textContent;"
          "var btns={"
          "'Idle':'<a href=\"/ready\"><button class=\"btn\" style=\"background:#f0ad00;\">Ready!</button></a>',"
          "'Ready!':'<a href=\"/start\"><button class=\"btn\" style=\"background:#28a745;\">Start Charging</button></a>',"
          "'Charging...':'<a href=\"/stop\"><button class=\"btn\" style=\"background:#dc3545;\">Stop Charging</button></a>',"
          "'Charging stopped':'<a href=\"/resume\"><button class=\"btn\" style=\"background:#28a745;\">Resume Charging</button></a>&nbsp;&nbsp;<a href=\"/reset\"><button class=\"btn\" style=\"background:#6c757d;\">Finish</button></a>',"
          "'Charging complete':'<a href=\"/reset\"><button class=\"btn\" style=\"background:#6c757d;\">Finish</button></a>'"
          "};"
          "function update(){"
          "fetch('/api/status').then(r=>r.json()).then(d=>{"
          "if(d.state!==lastState){lastState=d.state;location.reload();return;}"
          "document.getElementById('s-kwh').textContent=d.kwh;"
          "document.getElementById('s-cost').textContent=d.cost;"
          "document.getElementById('s-dur').textContent=d.duration;"
          "if(btns[d.state])document.getElementById('s-btn').innerHTML=btns[d.state];"
          "}).catch(()=>{});}"
          "setInterval(update,1000);"
          "</script>";

  html += "</body></html>";
  return html;
}

String getHistoryHTML() {
  String html = "<!DOCTYPE html><html><head>"
                "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                "<title>Session History</title>"
                "<style>"
                "body{font-family:sans-serif;max-width:400px;margin:40px auto;padding:0 16px;}"
                ".card{border:1px solid #ddd;border-radius:8px;padding:12px 14px;margin-bottom:12px;}"
                ".card-title{font-weight:bold;margin-bottom:4px;}"
                ".card-time{color:#555;font-size:14px;margin-bottom:6px;}"
                ".card-stats{font-size:15px;}"
                "</style></head><body>";
  html += "<h2>Session History</h2><p><a href='/'>&#8592; Back</a></p>";

  prefs.begin("history", true);
  uint8_t count = prefs.getUChar("count", 0);

  if (count == 0) {
    html += "<p>No sessions recorded yet.</p>";
  } else {
    for (int i = 0; i < count; i++) {
      char kwhKey[8], costKey[9], durKey[8], startKey[9], endKey[7];
      sprintf(kwhKey,   "s%dkwh",   i);
      sprintf(costKey,  "s%dcost",  i);
      sprintf(durKey,   "s%ddur",   i);
      sprintf(startKey, "s%dstart", i);
      sprintf(endKey,   "s%dend",   i);
      float    kwh   = prefs.getFloat(kwhKey,  0);
      float    cost  = prefs.getFloat(costKey, 0);
      uint32_t dur   = prefs.getUInt(durKey,   0);
      uint32_t tStart = prefs.getUInt(startKey, 0);
      uint32_t tEnd   = prefs.getUInt(endKey,   0);

      html += "<div class='card'>"
              "<div class='card-title'>Session " + String(i + 1) + "</div>"
              "<div class='card-time'>" + formatDateTime(tStart) + " &rarr; " + formatTime(tEnd) + "</div>"
              "<div class='card-stats'>"
              + String(kwh, 2) + " kWh &nbsp;&middot;&nbsp; EUR " + String(cost, 2)
              + " &nbsp;&middot;&nbsp; " + formatDuration(dur) +
              "</div></div>";
    }
    html += "<a href='/clearhistory'><button style='padding:6px 12px;background:#dc3545;color:#fff;border:none;border-radius:4px;cursor:pointer;'>Clear History</button></a>";
  }

  prefs.end();
  html += "</body></html>";
  return html;
}

// ---------- HTTP Handlers ----------
void handleRoot()     { server.send(200, "text/html", getHTML()); }
void handleSettings() { server.send(200, "text/html", getSettingsHTML()); }
void handleHistory()  { server.send(200, "text/html", getHistoryHTML()); }

void handleApiStatus() {
  String json = "{";
  json += "\"state\":\"" + getStateName() + "\",";
  json += "\"kwh\":\""  + String(kWh, 2)  + "\",";
  json += "\"cost\":\"" + String(kWh * pricePerKWh, 2) + "\",";
  json += "\"duration\":\"" + formatDuration(sessionElapsedSeconds) + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleReady() {
  if (state == IDLE) transitionTo(READY);
  server.sendHeader("Location", "/"); server.send(302, "text/plain", "");
}
void handleStart() {
  if (state == READY) transitionTo(CHARGING);
  server.sendHeader("Location", "/"); server.send(302, "text/plain", "");
}
void handleStop() {
  if (state == CHARGING) transitionTo(CHARGING_STOPPED);
  server.sendHeader("Location", "/"); server.send(302, "text/plain", "");
}
void handleReset() {
  if (state == CHARGING_STOPPED || state == CHARGING_COMPLETE) transitionTo(IDLE);
  server.sendHeader("Location", "/"); server.send(302, "text/plain", "");
}
void handleResume() {
  if (state == CHARGING_STOPPED) resumeCharging();
  server.sendHeader("Location", "/"); server.send(302, "text/plain", "");
}

void handleSetLimit() {
  if (server.hasArg("kwh")) {
    float val = server.arg("kwh").toFloat();
    if (val >= 0.1) maxKWh = val;
  }
  server.sendHeader("Location", "/settings"); server.send(302, "text/plain", "");
}
void handleSetPrice() {
  if (server.hasArg("eur")) {
    float val = server.arg("eur").toFloat();
    if (val >= 0.01) pricePerKWh = val;
  }
  server.sendHeader("Location", "/settings"); server.send(302, "text/plain", "");
}
void handleSetCostCap() {
  if (server.hasArg("eur")) {
    float val = server.arg("eur").toFloat();
    if (val >= 0.0) maxCost = val;
  }
  server.sendHeader("Location", "/settings"); server.send(302, "text/plain", "");
}
void handleSyncTime() {
  if (server.hasArg("ts")) {
    uint32_t ts = (uint32_t)server.arg("ts").toInt();
    if (ts > 1000000000UL) { // sanity check: after year 2001
      syncedEpoch = ts;
      syncedAtMs  = millis();
    }
  }
  server.send(200, "text/plain", "ok");
}

void handleClearHistory() {
  prefs.begin("history", false);
  prefs.clear();
  prefs.end();
  server.sendHeader("Location", "/history"); server.send(302, "text/plain", "");
}

// ---------- LCD ----------
String getLcdStatusLine() {
  switch (state) {
    case IDLE:              return "Idle            ";
    case READY:             return "Ready!          ";
    case CHARGING: {
      String s = "Chg " + formatDuration(sessionElapsedSeconds);
      while (s.length() < 16) s += " ";
      return s;
    }
    case CHARGING_STOPPED:  return "Chrg Stopped    ";
    case CHARGING_COMPLETE: return "Chrg Complete!  ";
    default:                return "                ";
  }
}

// ---------- Setup ----------
void setup() {
  pinMode(LED_PIN_RED, OUTPUT);
  pinMode(LED_PIN_GREEN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  lcd.begin();
  lcd.setcontrast(24);
  Serial.begin(115200);

  WiFi.softAP(ssid, password);
  Serial.println("WiFi AP started");
  Serial.println(WiFi.softAPIP());

  server.on("/",            handleRoot);
  server.on("/settings",    handleSettings);
  server.on("/ready",       handleReady);
  server.on("/start",       handleStart);
  server.on("/stop",        handleStop);
  server.on("/reset",       handleReset);
  server.on("/resume",      handleResume);
  server.on("/setlimit",    handleSetLimit);
  server.on("/setprice",    handleSetPrice);
  server.on("/setcostcap",  handleSetCostCap);
  server.on("/api/status",  handleApiStatus);
  server.on("/history",     handleHistory);
  server.on("/clearhistory",handleClearHistory);
  server.on("/synctime",    handleSyncTime);
  server.begin();

  digitalWrite(LED_PIN_GREEN, HIGH);
  digitalWrite(LED_PIN_RED, LOW);
}

// ---------- Button Handling ----------
void handleButton() {
  static bool stableState = HIGH;
  static bool lastReading = HIGH;
  static unsigned long lastChangeTime = 0;

  bool reading = digitalRead(BUTTON_PIN);
  if (reading != lastReading) { lastChangeTime = millis(); lastReading = reading; }

  if (millis() - lastChangeTime > debounceDelay && reading != stableState) {
    stableState = reading;
    if (stableState == LOW) {
      switch (state) {
        case IDLE:              transitionTo(READY);            break;
        case READY:             transitionTo(CHARGING);         break;
        case CHARGING:          transitionTo(CHARGING_STOPPED); break;
        case CHARGING_STOPPED:
        case CHARGING_COMPLETE: transitionTo(IDLE);             break;
      }
    }
  }
}

// ---------- LED Update ----------
void updateLEDs() {
  unsigned long now = millis();
  if (now - lastBlinkTime >= blinkInterval) { lastBlinkTime = now; blinkToggle = !blinkToggle; }

  switch (state) {
    case IDLE:
      digitalWrite(LED_PIN_GREEN, HIGH); digitalWrite(LED_PIN_RED, LOW);  break;
    case READY:
      digitalWrite(LED_PIN_GREEN, LOW);  digitalWrite(LED_PIN_RED,  blinkToggle ? HIGH : LOW); break;
    case CHARGING:
      digitalWrite(LED_PIN_GREEN, LOW);  digitalWrite(LED_PIN_RED,  HIGH); break;
    case CHARGING_STOPPED:
    case CHARGING_COMPLETE:
      digitalWrite(LED_PIN_GREEN, blinkToggle ? HIGH : LOW); digitalWrite(LED_PIN_RED, LOW); break;
  }
}

// ---------- Loop ----------
unsigned long lastUpdate = 0;
const unsigned long updateInterval = 500;

void loop() {
  server.handleClient();
  handleButton();
  updateLEDs();

  if (millis() - lastUpdate >= updateInterval) {
    lastUpdate = millis();

    if (state == CHARGING) {
      sessionElapsedSeconds = (millis() - chargingStartMs) / 1000;
      kWh += 0.01;

      bool limitHit = (kWh >= maxKWh);
      bool costHit  = (maxCost > 0.0 && (kWh * pricePerKWh) >= maxCost);

      if (limitHit || costHit) {
        if (limitHit) kWh = maxKWh;
        transitionTo(CHARGING_COMPLETE);
      }
    }

    // LCD row 0: state + timer
    lcd.setCursor(0, 0);
    lcd.print(getLcdStatusLine());

    // LCD row 1: kWh + cost
    {
      String kWhStr   = String(kWh, 2) + " kWh";
      String priceStr = "E" + String(kWh * pricePerKWh, 2);
      String row1 = (kWhStr.length() + 1 + priceStr.length() <= 16)
                    ? kWhStr + " " + priceStr
                    : kWhStr;
      while (row1.length() < 16) row1 += " ";
      lcd.setCursor(1, 0);
      lcd.print(row1);
    }
  }
}
