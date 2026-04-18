// ---------- Format Helpers ----------

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
    bool     leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    uint16_t diy  = leap ? 366 : 365;
    if (days < diy) break;
    days -= diy; year++;
  }
  const uint8_t dim[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  bool    leap  = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
  uint8_t month = 0;
  for (month = 0; month < 12; month++) {
    uint8_t d = (month == 1 && leap) ? 29 : dim[month];
    if (days < d) break;
    days -= d;
  }
  char buf[15];
  sprintf(buf, "%02u.%02u.%02u %02u:%02u",
          (uint32_t)(days + 1), (uint32_t)(month + 1), year % 100, hour, min);
  return String(buf);
}

// Extract just "HH:MM" from an epoch (for end-time display on the same day)
String formatTime(uint32_t epoch) {
  if (epoch == 0) return "--:--";
  char buf[6];
  sprintf(buf, "%02u:%02u", (epoch / 3600) % 24, (epoch / 60) % 60);
  return String(buf);
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

// ---------- LEDs ----------

void updateLEDs() {
  unsigned long now = millis();
  if (now - lastBlinkTime >= blinkInterval) {
    lastBlinkTime = now;
    blinkToggle   = !blinkToggle;
  }

  switch (state) {
    case IDLE:
      digitalWrite(LED_PIN_GREEN, HIGH);
      digitalWrite(LED_PIN_RED,   LOW);
      break;
    case READY:
      digitalWrite(LED_PIN_GREEN, LOW);
      digitalWrite(LED_PIN_RED,   blinkToggle ? HIGH : LOW);
      break;
    case CHARGING:
      digitalWrite(LED_PIN_GREEN, LOW);
      digitalWrite(LED_PIN_RED,   HIGH);
      break;
    case CHARGING_STOPPED:
    case CHARGING_COMPLETE:
      digitalWrite(LED_PIN_GREEN, blinkToggle ? HIGH : LOW);
      digitalWrite(LED_PIN_RED,   LOW);
      break;
  }
}

// ---------- Button ----------

void handleButton() {
  static bool          stableState     = HIGH;
  static bool          lastReading     = HIGH;
  static unsigned long lastChangeTime  = 0;

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
