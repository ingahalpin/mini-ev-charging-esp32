#include <WiFi.h>
#include <WebServer.h>
#include <LCD_ST7032.h>

LCD_ST7032 lcd;

// ---------- Pins ----------
const int LED_PIN_RED = 23;
const int LED_PIN_GREEN = 4;
const int BUTTON_PIN = 18;

// ---------- State ----------
bool charging = false;
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// ---------- Simulation ----------
float kWh = 0.0;
const float pricePerKWh = 0.35;

// ---------- Web Server ----------
WebServer server(80);

// ---------- WiFi AP ----------
const char* ssid = "EV-Charger";
const char* password = "12345678";

// ---------- HTML ----------
String getHTML() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<meta http-equiv='refresh' content='2'>";
  html += "<title>EV Charger</title></head><body>";

  html += "<h2>Mini EV Charger</h2>";

  html += "<p>Status: <b>";
  html += (charging ? "Charging" : "Idle");
  html += "</b></p>";

  html += "<p>Energy: " + String(kWh, 2) + " kWh</p>";
  html += "<p>Cost: EUR" + String(kWh * pricePerKWh, 2) + "</p>";

  html += "<hr>";

  html += "<a href='/start'><button style='font-size:20px;'>Start Charging</button></a>";
  html += "<br><br>";
  html += "<a href='/stop'><button style='font-size:20px;'>Stop Charging</button></a>";

  html += "</body></html>";

  return html;
}

void handleRoot() {
  server.send(200, "text/html", getHTML());
}

// ---------- Setup ----------
void setup() {
  pinMode(LED_PIN_RED, OUTPUT);
  pinMode(LED_PIN_GREEN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  lcd.begin();
  lcd.setcontrast(24);

  Serial.begin(115200);

  // WiFi Access Point
  WiFi.softAP(ssid, password);
  Serial.println("WiFi AP started");
  Serial.println(WiFi.softAPIP());

  // Web Roots
  server.on("/", handleRoot);
  server.on("/start", handleStart);
  server.on("/stop", handleStop);
  server.begin();

  Serial.println(digitalRead(BUTTON_PIN));
}

// ---------- Button Handling ----------
void handleButton() {
  static bool stableState = HIGH;
  static bool lastReading = HIGH;
  static unsigned long lastChangeTime = 0;

  bool reading = digitalRead(BUTTON_PIN);

  // detect change
  if (reading != lastReading) {
    lastChangeTime = millis();
    lastReading = reading;
  }

  // debounce
  if (millis() - lastChangeTime > debounceDelay) {
    if (reading != stableState) {
      stableState = reading;

      // trigger only on PRESS
      if (stableState == LOW) {
        charging = !charging;
        Serial.println(charging ? "Charging ON (button)" : "Charging OFF (button)");
      }
    }
  }
}

void handleStart() {
  charging = true;
  server.send(200, "text/plain", "Charging started");
}

void handleStop() {
  charging = false;
  server.send(200, "text/plain", "Charging stopped");
}

// ---------- Loop ----------
unsigned long lastUpdate = 0;
const unsigned long updateInterval = 500;

void loop() {
  server.handleClient();
  handleButton();

  // LED reflects state
  digitalWrite(LED_PIN_RED, charging ? HIGH : LOW);
  digitalWrite(LED_PIN_GREEN, charging ? LOW : HIGH);

  // Only update simulation and LCD every 500ms, without blocking
  if (millis() - lastUpdate >= updateInterval) {
    lastUpdate = millis();

    if (charging) {
      kWh += 0.01;
    }

    // LCD output
    lcd.setCursor(0, 0);
    lcd.print(charging ? "Charging        " : "Idle            ");

    lcd.setCursor(1, 0);
    lcd.print(kWh, 2);
    lcd.print(" kWh        ");
  }
}
