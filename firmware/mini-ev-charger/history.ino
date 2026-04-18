// ---------- Session History (NVS) ----------

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
