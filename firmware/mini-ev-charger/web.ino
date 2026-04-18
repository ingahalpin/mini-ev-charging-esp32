// ---------- HTML Builders ----------

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

  // Live data — spans updated by JS polling
  html += "<p>Status: <b id='s-state'>" + getStateName() + "</b></p>";

  html += "<p>Energy: <span id='s-kwh'>" + String(kWh, 2) + "</span> kWh";
  html += " <span style='color:#888;font-size:14px;'>/ " + String(maxKWh, 1) + " kWh limit</span></p>";

  html += "<p>Cost: EUR <span id='s-cost'>" + String(kWh * pricePerKWh, 2) + "</span>";
  if (maxCost > 0.0) html += " <span style='color:#888;font-size:14px;'>/ EUR " + String(maxCost, 2) + " cap</span>";
  html += "</p>";

  html += "<p>Duration: <span id='s-dur'>" + formatDuration(sessionElapsedSeconds) + "</span></p>";

  html += "<hr><div id='s-btn'>" + getActionButton() + "</div>";

  html += "<hr><p><a href='/settings'>Settings</a> &nbsp;&middot;&nbsp; <a href='/history'>Session History</a></p>";

  // Sync browser time on load, then poll /api/status every second
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
      float    kwh    = prefs.getFloat(kwhKey,   0);
      float    cost   = prefs.getFloat(costKey,  0);
      uint32_t dur    = prefs.getUInt(durKey,    0);
      uint32_t tStart = prefs.getUInt(startKey,  0);
      uint32_t tEnd   = prefs.getUInt(endKey,    0);

      html += "<div class='card'>"
              "<div class='card-title'>Session " + String(i + 1) + "</div>"
              "<div class='card-time'>" + formatDateTime(tStart) + " &rarr; " + formatTime(tEnd) + "</div>"
              "<div class='card-stats'>"
              + String(kwh, 2) + " kWh &nbsp;&middot;&nbsp; EUR " + String(cost, 2)
              + " &nbsp;&middot;&nbsp; " + formatDuration(dur)
              + "</div></div>";
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
  json += "\"state\":\""    + getStateName()                     + "\",";
  json += "\"kwh\":\""      + String(kWh, 2)                     + "\",";
  json += "\"cost\":\""     + String(kWh * pricePerKWh, 2)       + "\",";
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
