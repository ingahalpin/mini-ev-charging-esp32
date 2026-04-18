#include <WiFi.h>
#include <WebServer.h>
#include <LCD_ST7032.h>
#include <Preferences.h>

LCD_ST7032 lcd;
Preferences prefs;

// ---------- Pins ----------
const int LED_PIN_RED   = 23;
const int LED_PIN_GREEN = 4;
const int BUTTON_PIN    = 18;

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
float kWh         = 0.0;
float maxKWh      = 1.0;   // configurable charge limit
float pricePerKWh = 0.35;  // configurable price
float maxCost     = 0.0;   // cost cap (0 = disabled)

// ---------- Session Timer ----------
unsigned long chargingStartMs      = 0;
uint32_t      sessionElapsedSeconds = 0;

// ---------- Wall Clock (browser-synced) ----------
uint32_t      syncedEpoch    = 0;  // UTC Unix timestamp received from browser
unsigned long syncedAtMs     = 0;  // millis() at the moment of sync
uint32_t      sessionStartTime = 0; // UTC epoch when current charging session began

// ---------- WiFi AP ----------
const char* ssid     = "EV-Charger";
const char* password = "12345678";

// ---------- Web Server ----------
WebServer server(80);

// ---------- Session History ----------
const int MAX_HISTORY = 5;

// ---------- Loop Timing ----------
unsigned long lastUpdate = 0;
const unsigned long updateInterval = 500;

// ---------- Setup ----------
void setup() {
  pinMode(LED_PIN_RED,   OUTPUT);
  pinMode(LED_PIN_GREEN, OUTPUT);
  pinMode(BUTTON_PIN,    INPUT_PULLUP);

  lcd.begin();
  lcd.setcontrast(24);
  Serial.begin(115200);

  WiFi.softAP(ssid, password);
  Serial.println("WiFi AP started");
  Serial.println(WiFi.softAPIP());

  server.on("/",             handleRoot);
  server.on("/settings",     handleSettings);
  server.on("/ready",        handleReady);
  server.on("/start",        handleStart);
  server.on("/stop",         handleStop);
  server.on("/reset",        handleReset);
  server.on("/resume",       handleResume);
  server.on("/setlimit",     handleSetLimit);
  server.on("/setprice",     handleSetPrice);
  server.on("/setcostcap",   handleSetCostCap);
  server.on("/api/status",   handleApiStatus);
  server.on("/history",      handleHistory);
  server.on("/clearhistory", handleClearHistory);
  server.on("/synctime",     handleSyncTime);
  server.begin();

  digitalWrite(LED_PIN_GREEN, HIGH);
  digitalWrite(LED_PIN_RED,   LOW);
}

// ---------- Loop ----------
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
