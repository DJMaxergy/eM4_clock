#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <Adafruit_NeoPixel.h>
#include <time.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>

// LED setup
#define LED_PIN     2
#define LED_COUNT   12
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// Buzzer setup
#define BUZZER_PIN  3
#define BUZZER_FREQ 4000
#define BUZZER_RES  8         // 8-bit resolution
bool buzzerActive = false;
unsigned long buzzerEndTime = 0;

// Web server
WebServer server(80);
const char PAGE_TEMPLATE[] PROGMEM = R"rawliteral(
<html>
  <body>
    <h2>Clock Config</h2>
    <form action="/save" method="POST">
      SSID:<br>
      <input name="ssid" value="%SSID%"><br>
      
      Password:<br>
      <input name="password" type="password" value="%PASSWORD%"><br>
      
      Timezone:<br>
      <input name="timezone" value="%TIMEZONE%"><br>
      
      Brightness (0–255):<br>
      <input name="brightness" type="range" min="0" max="255" value="%BRIGHTNESS%"><br>
      
      Theme:<br>
      <select name="theme">
        <option value="classic" %SEL_CLASSIC%>Classic</option>
        <option value="cool" %SEL_COOL%>Cool</option>
        <option value="warm" %SEL_WARM%>Warm</option>
        <option value="abl" %SEL_ABL%>ABL</option>
      </select><br><br>
      
      Buzzer:<br>
      <input type="checkbox" name="buzzer" value="1" %BUZZER%><br><br>
      
      <input type="submit" value="Save">
    </form>
  </body>
</html>
)rawliteral";

// Config struct
struct Config {
  String ssid;
  String password;
  String timezone;
  int brightness;
  String theme;
  bool buzzer;
};
Config config;

// Time
bool timeInitialized = false;

uint32_t hourColorAM, hourColorPM, minuteColor;

// --- Pulsating State ---
bool pulsating = false;
uint32_t pulsatingColor = 0;

void startPulsate(uint32_t color) {
  pulsating = true;
  pulsatingColor = color;
}

void updatePulsate() {
  if (!pulsating) return;

  float pulse = getPulseBrightness();
  uint32_t scaled = scaleColor(pulsatingColor, pulse);
  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, scaled);
  }
  strip.show();
}

// --- File I/O ---
bool loadConfig() {
  if (!LittleFS.begin(true)) {
    Serial.println("Failed to mount or format LittleFS");
    return false;
  }
  if (!LittleFS.exists("/config.json")) return false;

  File file = LittleFS.open("/config.json", "r");
  if (!file) return false;

  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, file);
  if (error) return false;

  config.ssid = doc["ssid"].as<String>();
  config.password = doc["password"].as<String>();
  config.timezone = doc["timezone"].as<String>();
  config.brightness = doc["brightness"] | 100; // Default to 100
  config.theme = doc["theme"] | "abl";
  config.buzzer = doc["buzzer"] | true;  // default enabled

  file.close();
  return true;
}

bool saveConfig() {
  StaticJsonDocument<512> doc;
  doc["ssid"] = config.ssid;
  doc["password"] = config.password;
  doc["timezone"] = config.timezone;
  doc["brightness"] = config.brightness;
  doc["theme"] = config.theme;
  doc["buzzer"] = config.buzzer;

  File file = LittleFS.open("/config.json", "w");
  if (!file) return false;

  serializeJson(doc, file);
  file.close();
  return true;
}

// --- Buzzer functions ---
// Start a non-blocking beep for the given duration (ms)
void startBeep(unsigned long durationMs) {
  if (!config.buzzer) return;
  Serial.println("Starting to beep");
  buzzerActive = true;
  buzzerEndTime = millis() + durationMs;

  // 50% duty cycle at 8-bit resolution = 128
  ledcWrite(BUZZER_PIN, 128);
}

// Must be called regularly from loop()
void updateBeep() {
  if (buzzerActive && millis() >= buzzerEndTime) {
    ledcWrite(BUZZER_PIN, 0);  // stop sound
    buzzerActive = false;
  }
}

// --- Clock LED position remap ---
int clockToLED(int clockIndex) {
  //return (12 - clockIndex) % 12; //reversed
  return clockIndex % 12;
}

// --- Time Setup ---
void setupTime() {
  configTzTime(config.timezone.c_str(), "pool.ntp.org", "time.nist.gov");
  struct tm timeinfo;
  for (int i = 0; i < 10; i++) {
    if (getLocalTime(&timeinfo)) {
      Serial.println("Time synced.");
      timeInitialized = true;
      break;
    }
    delay(500);
  }
}

// --- Access Point Web UI ---
void startAPMode() {
  WiFi.softAP("eM4ClockConfig");

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  setupWebServer();
}

void showStaticRainbow() {
  uint32_t colors[4] = {
    strip.Color(255, 0, 0),   // Red
    strip.Color(0, 255, 0),   // Green
    strip.Color(0, 0, 255),   // Blue
    strip.Color(255, 255, 255) // White
  };

  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, colors[i % 4]); // repeat RGBW
  }
  strip.show();
}

void applyTheme(String theme) {
  if (theme == "cool") {
    hourColorAM = strip.Color(0, 128, 255);    // light blue
    hourColorPM = strip.Color(0, 255, 128);    // mint green
    minuteColor = strip.Color(255, 255, 255);  // white
  } else if (theme == "warm") {
    hourColorAM = strip.Color(255, 140, 0);    // dark orange
    hourColorPM = strip.Color(255, 69, 0);     // red-orange
    minuteColor = strip.Color(255, 255, 0);    // yellow
  } else if (theme == "abl") {
    hourColorAM = strip.Color(0, 255, 0);      // green
    hourColorPM = strip.Color(255, 255, 255);  // white
    minuteColor = strip.Color(0, 0, 255);      // blue
  } else {
    // classic
    hourColorAM = strip.Color(255, 0, 0);      // red
    hourColorPM = strip.Color(0, 0, 255);      // blue
    minuteColor = strip.Color(0, 255, 0);      // green
  }
}

float getPulseBrightness() {
  const float period = 3000.0; // 3 seconds for full pulse cycle
  float time = millis() % (int)period;
  float phase = 2 * PI * time / period;
  float brightness = 0.5 * (1 + sin(phase - PI / 2));  // Maps sine from -1..1 to 0..1
  return brightness;
}

uint32_t scaleColor(uint32_t color, float brightness) {
  uint8_t r = (uint8_t)( (uint8_t)(color >> 16) * brightness );
  uint8_t g = (uint8_t)( (uint8_t)(color >> 8 & 0xFF) * brightness );
  uint8_t b = (uint8_t)( (uint8_t)(color & 0xFF) * brightness );
  return strip.Color(r, g, b);
}

void setupWebServer() {
  server.on("/", []() {
    // Use default timezone if none saved
    String timezoneValue = config.timezone.length() > 0
      ? config.timezone
      : "CET-1CEST,M3.5.0,M10.5.0/3";

    // Build HTML from template in PROGMEM
    String html = FPSTR(PAGE_TEMPLATE);

    // Fill basic fields
    html.replace("%SSID%", config.ssid);
    html.replace("%PASSWORD%", config.password);
    html.replace("%TIMEZONE%", timezoneValue);
    html.replace("%BRIGHTNESS%", String(config.brightness));

    // Theme selections
    html.replace("%SEL_CLASSIC%", (config.theme == "classic") ? "selected" : "");
    html.replace("%SEL_COOL%",    (config.theme == "cool")    ? "selected" : "");
    html.replace("%SEL_WARM%",    (config.theme == "warm")    ? "selected" : "");
    html.replace("%SEL_ABL%",     (config.theme == "abl")     ? "selected" : "");

    // Buzzer checkbox
    html.replace("%BUZZER%", config.buzzer ? "checked" : "");

    server.send(200, "text/html", html);
  });

  server.on("/save", []() {
    // Get values from the form
    config.ssid       = server.arg("ssid");
    config.password   = server.arg("password");
    config.timezone   = server.arg("timezone");
    config.brightness = server.arg("brightness").toInt();
    config.theme      = server.arg("theme");
    config.buzzer     = server.hasArg("buzzer") && server.arg("buzzer") == "1";

    saveConfig();

    String html = F("<html><body><h3>Saved. Rebooting...</h3></body></html>");
    server.send(200, "text/html", html);

    delay(2000);
    ESP.restart();
  });

  server.begin();
  Serial.println("Web server started.");
}

// --- Setup ---
void setup() {
  Serial.begin(115200);
  delay(100);

  strip.begin();
  strip.show();
  strip.setBrightness(100);

  // Buzzer setup
  ledcAttach(BUZZER_PIN, BUZZER_FREQ, 8);
  ledcWrite(BUZZER_PIN, 0);   // start off

  // Show static rainbow at startup
  showStaticRainbow();
  delay(2000); // keep it visible for 2 seconds before continuing

  if (!loadConfig()) {
    Serial.println("No config found. Starting AP mode.");
    startAPMode();
    startPulsate(strip.Color(255,255,255)); // White pulsating if no config
    return;
  }

  // Startup beep (non-blocking) for 1 second
  startBeep(1000);

  strip.setBrightness(config.brightness);
  applyTheme(config.theme);

  // Connect to saved Wi-Fi
  WiFi.begin(config.ssid.c_str(), config.password.c_str());
  Serial.print("Connecting to WiFi");
  for (int i = 0; i < 20; i++) {
    if (WiFi.status() == WL_CONNECTED) break;
    delay(500);
    Serial.print(".");
    updateBeep();
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Failed to connect. Starting AP mode.");
    startAPMode();
    startPulsate(strip.Color(255,0,0)); // Red pulsating if Wi-Fi fails
    return;
  }

  Serial.println("WiFi connected.");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  if (!MDNS.begin("eM4clock")) {
    Serial.println("Error setting up mDNS responder!");
  } else {
    Serial.println("mDNS responder started: http://eM4clock.local");
  }

  setupWebServer();
  setupTime();

  if (!timeInitialized) {
    startPulsate(strip.Color(255,0,100)); // Magenta pulsating if fetching time fails
  }
}

// --- Loop ---
void loop() {
  // Always handle incoming HTTP requests
  server.handleClient();

  // Update buzzer state (non-blocking)
  updateBeep();

  if (pulsating) {
    updatePulsate();
    return; // show only pulsating effect
  }

  // If in AP mode, no clock display is needed
  if (WiFi.getMode() == WIFI_AP) return;

  // Don't proceed if time isn't ready
  if (!timeInitialized) return;

  static unsigned long lastClockUpdate = 0;
  static unsigned long lastAnimUpdate = 0;
  unsigned long now = millis();

  // Clock values
  static int hour = 12;
  static int minute = 0;
  static int hour24 = 0;
  static uint32_t currentHourColor = hourColorAM;
  static int lastMinute = -1;

  // Update time every 10s
  if (now - lastClockUpdate >= 10000 || !timeInitialized) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      hour24 = timeinfo.tm_hour;
      hour = hour24 % 12;
      if (hour == 0) hour = 12;
      minute = timeinfo.tm_min;

      // Full-hour beep (only once per transition)
      if (minute == 0 && minute != lastMinute) {
        startBeep(500);
      }
      lastMinute = minute;

      currentHourColor = (hour24 < 12) ? hourColorAM : hourColorPM;
      //currentHourColor = ((hour24 < 13) && (hour24 > 0)) ? hourColorAM : hourColorPM;
      timeInitialized = true;
    }
    lastClockUpdate = now;
  }

  // Update animation every 40ms
  if (now - lastAnimUpdate >= 40 && timeInitialized) {
    lastAnimUpdate = now;

    float pulse = getPulseBrightness();
    strip.clear();

    // Draw hours with pulse
    for (int i = 1; i <= hour; i++) {
      int ledIndex = clockToLED(i % 12);
      uint32_t scaledColor = scaleColor(currentHourColor, pulse);
      strip.setPixelColor(ledIndex, scaledColor);
    }

    // Draw minute
    int minuteIndex = (minute + 2) / 5;
    if (minuteIndex == 12) minuteIndex = 0;
    int minuteLED = clockToLED(minuteIndex);

    int topHourLED = clockToLED(hour % 12);
    bool isOverlap = (minuteLED == topHourLED);

    if (isOverlap) {
      // Apply pulse effect to minute color (just like hours)
      uint32_t pulsedMinuteColor = scaleColor(minuteColor, pulse);
      strip.setPixelColor(minuteLED, pulsedMinuteColor);
    } else {
      // Normal static minute color
      strip.setPixelColor(minuteLED, minuteColor);
    }

    strip.show();
  }
}
