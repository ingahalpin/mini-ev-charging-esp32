// ---------- State Machine ----------

uint32_t getCurrentTime() {
  if (syncedEpoch == 0) return 0;
  return syncedEpoch + (uint32_t)((millis() - syncedAtMs) / 1000UL);
}

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

void transitionTo(ChargerState newState) {
  if (newState == CHARGING) {
    chargingStartMs       = millis();
    sessionElapsedSeconds = 0;
    sessionStartTime      = getCurrentTime();
  }
  if (newState == CHARGING_STOPPED || newState == CHARGING_COMPLETE) {
    saveSession(kWh, kWh * pricePerKWh, sessionElapsedSeconds, sessionStartTime, getCurrentTime());
  }
  if (newState == IDLE || newState == READY) {
    kWh                   = 0.0;
    sessionElapsedSeconds = 0;
    sessionStartTime      = 0;
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
