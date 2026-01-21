/*
 * ESP Web Controller (SH1106 Version)
 *
 * Controls an SH1106 OLED display via Firebase backend.
 * Uses WiFiManager for easy WiFi and API key configuration.
 *
 * Hardware:
 *   - ESP32 Dev Module
 *   - SH1106 128x64 OLED (I2C: SDA=21, SCL=22)
 *
 * Setup:
 *   1. Upload this sketch
 *   2. Connect to "ESP32-Setup" WiFi network
 *   3. Enter your WiFi credentials and API key from the dashboard
 *   4. Hold BOOT button for 3 seconds to reconfigure anytime
 *
 * Branch: sh1106
 */

#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <time.h>
#include <EEPROM.h>

// ===========================================
// CONFIGURATION
// ===========================================

// OLED Display (SH1106 128x64)
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// Firebase Cloud Function URL (replace with your deployed function URL)
const char* API_BASE_URL = "https://us-central1-esp-web-2625a.cloudfunctions.net/api";


// Config button (BOOT button on most ESP32 boards)
#define CONFIG_PIN 0
#define EEPROM_SIZE 512  // Increased to avoid conflicts with WiFiManager
#define API_KEY_ADDR 100  // Store API key at offset 100 to avoid WiFiManager conflicts

// Display dimensions
const int SCREEN_WIDTH = 128;
const int SCREEN_HEIGHT = 64;

// Website URL
const char* WEBSITE_URL = "espcon.vercel.app";

// ===========================================
// ESPCON LOGO BITMAP (32x32 pixels) - Chip/Circuit design
// ===========================================
static const unsigned char ESPCON_LOGO[] PROGMEM = {
  0x00, 0x0F, 0xF0, 0x00,  // Row 0
  0x00, 0x0F, 0xF0, 0x00,  // Row 1
  0x00, 0x0F, 0xF0, 0x00,  // Row 2
  0x00, 0x0F, 0xF0, 0x00,  // Row 3
  0x0F, 0xFF, 0xFF, 0xF0,  // Row 4
  0x0F, 0xFF, 0xFF, 0xF0,  // Row 5
  0x0F, 0x00, 0x00, 0xF0,  // Row 6
  0x0F, 0x00, 0x00, 0xF0,  // Row 7
  0xFF, 0x0F, 0xF0, 0xFF,  // Row 8  - pins left/right
  0xFF, 0x0F, 0xF0, 0xFF,  // Row 9
  0x0F, 0x0F, 0xF0, 0xF0,  // Row 10
  0x0F, 0x0F, 0xF0, 0xF0,  // Row 11
  0xFF, 0x0F, 0xF0, 0xFF,  // Row 12 - pins
  0xFF, 0x0F, 0xF0, 0xFF,  // Row 13
  0x0F, 0x0F, 0xF0, 0xF0,  // Row 14
  0x0F, 0x0F, 0xF0, 0xF0,  // Row 15
  0xFF, 0x0F, 0xF0, 0xFF,  // Row 16 - pins
  0xFF, 0x0F, 0xF0, 0xFF,  // Row 17
  0x0F, 0x0F, 0xF0, 0xF0,  // Row 18
  0x0F, 0x0F, 0xF0, 0xF0,  // Row 19
  0xFF, 0x0F, 0xF0, 0xFF,  // Row 20 - pins
  0xFF, 0x0F, 0xF0, 0xFF,  // Row 21
  0x0F, 0x00, 0x00, 0xF0,  // Row 22
  0x0F, 0x00, 0x00, 0xF0,  // Row 23
  0x0F, 0xFF, 0xFF, 0xF0,  // Row 24
  0x0F, 0xFF, 0xFF, 0xF0,  // Row 25
  0x00, 0x0F, 0xF0, 0x00,  // Row 26
  0x00, 0x0F, 0xF0, 0x00,  // Row 27
  0x00, 0x0F, 0xF0, 0x00,  // Row 28
  0x00, 0x0F, 0xF0, 0x00,  // Row 29
  0x00, 0x0F, 0xF0, 0x00,  // Row 30
  0x00, 0x0F, 0xF0, 0x00   // Row 31
};

// ===========================================
// GLOBAL VARIABLES
// ===========================================

WiFiManager wm;
WiFiManagerParameter* custom_api_key;
char apiKey[45] = "";
String userId = "";

// Mode management
String currentMode = "";  // Empty means no sketch chosen
String lastMode = "";
unsigned long lastModeCheck = 0;
const unsigned long MODE_CHECK_INTERVAL = 3000;
bool dataLoaded = false;  // Track if mode data has been loaded

// OLED Text Display
String displayMessage = "ESP32\nController";
int currentAnimation = 1;
int animationPos = 0;
int pulseState = 0;
int pulseDirection = 1;
int cornerAngle = 0;
int scanPos = 0;
int breatheSize = 0;
int breatheDir = 1;
unsigned long lastOledFetch = 0;
const unsigned long OLED_FETCH_INTERVAL = 5000;

// Pixel Paint
const int GRID_W = 96;
const int GRID_H = 48;
bool canvas[96][48];
unsigned long lastCanvasFetch = 0;
const unsigned long CANVAS_FETCH_INTERVAL = 1000;  // Reduced from 2000ms to 1000ms for faster updates
bool pixelPaintHasData = false;

// Image to Pixel
bool imagePixelCanvas[96][48];
unsigned long lastImagePixelFetch = 0;
const unsigned long IMAGE_PIXEL_FETCH_INTERVAL = 1000;  // Reduced from 2000ms to 1000ms for faster updates
bool imagePixelHasData = false;

// Weather Monitor
struct City {
  const char* name;
  const char* query;
  const char* tz;
  float temp;
  int humidity;
};

City cities[] = {
  // Asia
  {"Calicut", "Kozhikode,IN", "IST-5:30", 0, 0},
  {"Yelahanka", "Yelahanka,IN", "IST-5:30", 0, 0},
  {"Patna", "Patna,IN", "IST-5:30", 0, 0},
  {"Mumbai", "Mumbai,IN", "IST-5:30", 0, 0},
  {"Dubai", "Dubai,AE", "GST-4", 0, 0},
  {"Abu Dhabi", "Abu Dhabi,AE", "GST-4", 0, 0},
  {"Riyadh", "Riyadh,SA", "AST-3", 0, 0},
  {"Singapore", "Singapore,SG", "SGT-8", 0, 0},
  {"Tokyo", "Tokyo,JP", "JST-9", 0, 0},
  {"Beijing", "Beijing,CN", "CST-8", 0, 0},
  {"Seoul", "Seoul,KR", "KST-9", 0, 0},
  {"Bangkok", "Bangkok,TH", "ICT-7", 0, 0},
  {"Hong Kong", "Hong Kong,HK", "HKT-8", 0, 0},
  {"Istanbul", "Istanbul,TR", "TRT-3", 0, 0},

  // Europe
  {"Berlin", "Berlin,DE", "CET-1CEST,M3.5.0/2,M10.5.0/3", 0, 0},
  {"Stuttgart", "Stuttgart,DE", "CET-1CEST,M3.5.0/2,M10.5.0/3", 0, 0},
  {"London", "London,GB", "GMT0BST,M3.5.0/1,M10.5.0", 0, 0},
  {"Paris", "Paris,FR", "CET-1CEST,M3.5.0,M10.5.0/3", 0, 0},
  {"Madrid", "Madrid,ES", "CET-1CEST,M3.5.0,M10.5.0/3", 0, 0},
  {"Rome", "Rome,IT", "CET-1CEST,M3.5.0,M10.5.0/3", 0, 0},
  {"Amsterdam", "Amsterdam,NL", "CET-1CEST,M3.5.0,M10.5.0/3", 0, 0},
  {"Moscow", "Moscow,RU", "MSK-3", 0, 0},

  // North America
  {"New York", "New York,US", "EST5EDT,M3.2.0,M11.1.0", 0, 0},
  {"San Francisco", "San Francisco,US", "PST8PDT,M3.2.0,M11.1.0", 0, 0},
  {"Chicago", "Chicago,US", "CST6CDT,M3.2.0,M11.1.0", 0, 0},
  {"Los Angeles", "Los Angeles,US", "PST8PDT,M3.2.0,M11.1.0", 0, 0},
  {"Toronto", "Toronto,CA", "EST5EDT,M3.2.0,M11.1.0", 0, 0},
  {"Vancouver", "Vancouver,CA", "PST8PDT,M3.2.0,M11.1.0", 0, 0},
  {"Mexico City", "Mexico City,MX", "CST6CDT,M4.1.0,M10.5.0", 0, 0},

  // South America
  {"Sao Paulo", "Sao Paulo,BR", "BRT3BRST,M10.3.0/0,M2.3.0/0", 0, 0},
  {"Buenos Aires", "Buenos Aires,AR", "ART3", 0, 0},
  {"Lima", "Lima,PE", "PET5", 0, 0},
  {"Bogota", "Bogota,CO", "COT5", 0, 0},

  // Africa
  {"Cairo", "Cairo,EG", "EET-2", 0, 0},
  {"Lagos", "Lagos,NG", "WAT-1", 0, 0},
  {"Johannesburg", "Johannesburg,ZA", "SAST-2", 0, 0},
  {"Nairobi", "Nairobi,KE", "EAT-3", 0, 0},

  // Oceania
  {"Sydney", "Sydney,AU", "AEST-10AEDT,M10.1.0,M4.1.0/3", 0, 0},
  {"Melbourne", "Melbourne,AU", "AEST-10AEDT,M10.1.0,M4.1.0/3", 0, 0},
  {"Auckland", "Auckland,NZ", "NZST-12NZDT,M9.5.0,M4.1.0/3", 0, 0}
};
const int NUM_CITIES = 40;
int currentCity = -1;  // -1 means no city chosen
int lastCity = -1;
unsigned long lastWeatherSettingsFetch = 0;
unsigned long lastWeatherFetch = 0;
const unsigned long WEATHER_SETTINGS_INTERVAL = 3000;
const unsigned long WEATHER_FETCH_INTERVAL = 60000;
bool weatherDataLoaded = false;

// ===========================================
// SETUP
// ===========================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=== ESP Web Controller ===\n");

  pinMode(CONFIG_PIN, INPUT_PULLUP);

  // Initialize EEPROM and load API key
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(API_KEY_ADDR, apiKey);

  // Validate API key format
  if (strncmp(apiKey, "esp_", 4) != 0) {
    memset(apiKey, 0, sizeof(apiKey));
    Serial.println("No valid API Key found in EEPROM");
  } else {
    Serial.println("Loaded API Key: " + String(apiKey));
  }

  // Initialize OLED
  u8g2.begin();
  showMessage("Starting...");

  // Clear canvases
  memset(canvas, 0, sizeof(canvas));
  memset(imagePixelCanvas, 0, sizeof(imagePixelCanvas));

  // Setup WiFiManager
  setupWiFiManager();

  // Configure NTP
  configTime(0, 0, "pool.ntp.org");

  // Initial data fetch
  if (strlen(apiKey) > 0) {
    showMessage("Syncing...");
    if (fetchUserData()) {
      initCurrentMode();
    } else {
      showMessage("Sync failed\nCheck API key\nat\nespcon.vercel.app");
      delay(3000);
    }
  } else {
    // WiFi connected but no API key
    showNoApiKeyConnected();
    delay(3000);
  }
}

// ===========================================
// WiFi MANAGER SETUP
// ===========================================
void setupWiFiManager() {
  // Create custom parameter for API key
  custom_api_key = new WiFiManagerParameter("apikey", "ESP API Key", apiKey, 44);
  wm.addParameter(custom_api_key);

  // Set callbacks
  wm.setSaveConfigCallback(saveConfigCallback);
  wm.setConfigPortalTimeout(180);  // 3 minute timeout

  // Check if WiFi credentials are saved
  if (WiFi.SSID() == "") {
    // No WiFi configured - show setup instructions
    showFirstTimeSetup();
  } else {
    showMessage("Connecting\nWiFi...");
  }

  // Try to connect, or start config portal
  if (!wm.autoConnect("ESP32-Setup")) {
    Serial.println("Failed to connect, restarting...");
    showMessage("WiFi Failed\nRestarting...");
    delay(2000);
    ESP.restart();
  }

  Serial.println("Connected to WiFi!");
  Serial.println("IP: " + WiFi.localIP().toString());
  showMessage("Connected!\n" + WiFi.localIP().toString());
  delay(1000);
}

void saveConfigCallback() {
  Serial.println("Config saved!");

  // Save API key to EEPROM at dedicated offset
  if (strlen(custom_api_key->getValue()) > 0) {
    strcpy(apiKey, custom_api_key->getValue());
    EEPROM.put(API_KEY_ADDR, apiKey);
    if (EEPROM.commit()) {
      Serial.println("API Key saved successfully: " + String(apiKey));
    } else {
      Serial.println("ERROR: Failed to save API Key to EEPROM");
    }
  }
}

// ===========================================
// MAIN LOOP
// ===========================================
void loop() {
  unsigned long now = millis();

  // Check for config button press (hold for 3 seconds)
  checkConfigButton();

  // Skip if no API key
  if (strlen(apiKey) == 0) {
    showNoApiKey();
    delay(2000);
    return;
  }

  // Reconnect WiFi if disconnected
  if (WiFi.status() != WL_CONNECTED) {
    showNoWiFi();
    WiFi.reconnect();
    delay(5000);
    return;
  }

  // Check for mode changes
  if (now - lastModeCheck > MODE_CHECK_INTERVAL) {
    fetchUserData();
    lastModeCheck = now;
  }

  // Handle mode change
  if (currentMode != lastMode) {
    Serial.println("Mode changed to: " + currentMode);
    initCurrentMode();
    lastMode = currentMode;
  }

  // Run current mode
  if (currentMode == "" || currentMode == "null" || currentMode == "none") {
    showWelcomeScreen();
    delay(100);
  } else if (currentMode == "oled") {
    loopOLED();
  } else if (currentMode == "pixel-paint") {
    loopPixelPaint();
  } else if (currentMode == "image-to-pixel") {
    loopImageToPixel();
  } else if (currentMode == "weather") {
    loopWeather();
  } else {
    // Unknown mode - show welcome
    showWelcomeScreen();
    delay(100);
  }
}

void checkConfigButton() {
  if (digitalRead(CONFIG_PIN) == LOW) {
    delay(50);  // Debounce

    unsigned long pressStart = millis();
    while (digitalRead(CONFIG_PIN) == LOW) {
      if (millis() - pressStart > 3000) {
        Serial.println("\nOpening config portal...");
        showMessage("Config Mode\n\nConnect to:\nESP32-Setup");

        // Update parameter with current API key
        custom_api_key->setValue(apiKey, 44);

        wm.startConfigPortal("ESP32-Setup");

        Serial.println("Portal closed");

        // Refetch data with new settings
        if (strlen(apiKey) > 0) {
          fetchUserData();
          initCurrentMode();
        }
        break;
      }
    }
  }
}

// ===========================================
// API FUNCTIONS
// ===========================================
bool fetchUserData() {
  HTTPClient http;
  http.setTimeout(5000);

  String url = String(API_BASE_URL) + "/device?apiKey=" + String(apiKey);

  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      userId = doc["userId"].as<String>();
      currentMode = doc["activeSketch"].as<String>();
      Serial.println("User ID: " + userId);
      Serial.println("Active Mode: " + currentMode);
      http.end();
      return true;
    }
  } else {
    Serial.println("API error: " + String(httpCode));
  }

  http.end();
  return false;
}

// ===========================================
// MODE INITIALIZATION
// ===========================================
void initCurrentMode() {
  // Reset data flags
  dataLoaded = false;

  if (currentMode == "" || currentMode == "null" || currentMode == "none") {
    // No mode selected - welcome screen will be shown
    return;
  } else if (currentMode == "oled") {
    showMessage("OLED Mode");
    fetchOledSettings();
  } else if (currentMode == "pixel-paint") {
    showMessage("Pixel Paint");
    pixelPaintHasData = false;
    fetchCanvas();
  } else if (currentMode == "image-to-pixel") {
    showMessage("Image Mode");
    imagePixelHasData = false;
    fetchImagePixel();
  } else if (currentMode == "weather") {
    showMessage("Weather Mode");
    weatherDataLoaded = false;
    fetchWeatherSettings();
    if (currentCity >= 0) {
      applyTimezone();
      fetchWeather();
    }
  }
  delay(500);
}

// ===========================================
// OLED TEXT DISPLAY MODE
// ===========================================
void loopOLED() {
  unsigned long now = millis();

  if (now - lastOledFetch > OLED_FETCH_INTERVAL) {
    fetchOledSettings();
    lastOledFetch = now;
  }

  u8g2.clearBuffer();

  switch (currentAnimation) {
    case 0: break;
    case 1: drawBorderChase(); break;
    case 2: drawPulseBorder(); break;
    case 3: drawCornerSpin(); break;
    case 4: drawScanLine(); break;
    case 5: drawBreathingBox(); break;
  }

  drawMessage();
  u8g2.sendBuffer();
  delay(10);  // Reduced from 30ms to 10ms for smoother animations
}

void fetchOledSettings() {
  HTTPClient http;
  http.setTimeout(3000);

  String url = String(API_BASE_URL) + "/oled?apiKey=" + String(apiKey);

  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      displayMessage = doc["message"].as<String>();
      currentAnimation = doc["animation"].as<int>();
      Serial.println("OLED updated: " + displayMessage);
    }
  }

  http.end();
}

void drawMessage() {
  String msg = displayMessage;
  const int MAX_CHARS_PER_LINE = 19;
  const int MAX_LINES = 5;

  String lines[10];
  int numLines = 0;
  int lineStart = 0;

  for (int i = 0; i <= msg.length() && numLines < MAX_LINES; i++) {
    if (i == msg.length() || msg[i] == '\n') {
      String segment = msg.substring(lineStart, i);

      while (segment.length() > 0 && numLines < MAX_LINES) {
        if (segment.length() <= MAX_CHARS_PER_LINE) {
          lines[numLines++] = segment;
          break;
        } else {
          int breakAt = MAX_CHARS_PER_LINE;
          for (int j = MAX_CHARS_PER_LINE; j > 0; j--) {
            if (segment[j] == ' ') {
              breakAt = j;
              break;
            }
          }
          lines[numLines++] = segment.substring(0, breakAt);
          segment = segment.substring(breakAt);
          segment.trim();
        }
      }
      lineStart = i + 1;
    }
  }

  if (numLines == 0) return;

  u8g2.setFont(u8g2_font_6x10_tr);
  int lineHeight = 11;
  int lineSpacing = 2;
  int fontWidth = 6;

  int totalHeight = (numLines * lineHeight) + ((numLines - 1) * lineSpacing);
  int startY = (SCREEN_HEIGHT - totalHeight) / 2;
  if (startY < 0) startY = 0;

  int currentY = startY;

  for (int ln = 0; ln < numLines; ln++) {
    String line = lines[ln];
    if (line.length() > MAX_CHARS_PER_LINE) {
      line = line.substring(0, MAX_CHARS_PER_LINE);
    }

    int lineWidth = line.length() * fontWidth;
    int startX = (SCREEN_WIDTH - lineWidth) / 2;
    if (startX < 0) startX = 0;

    u8g2.setCursor(startX, currentY + lineHeight);
    u8g2.print(line);
    currentY += lineHeight + lineSpacing;
  }
}

// Animation functions
void drawBorderChase() {
  int perimeter = 2 * (SCREEN_WIDTH + SCREEN_HEIGHT);
  int segmentLength = 30;

  for (int i = 0; i < segmentLength; i++) {
    int pos = (animationPos + i) % perimeter;
    drawBorderPixel(pos, 3);
  }
  animationPos = (animationPos + 4) % perimeter;
}

void drawPulseBorder() {
  int thickness = 1 + (pulseState / 10);
  for (int t = 0; t < thickness && t < 5; t++) {
    u8g2.drawFrame(t, t, SCREEN_WIDTH - 2*t, SCREEN_HEIGHT - 2*t);
  }
  pulseState += pulseDirection * 2;
  if (pulseState >= 40 || pulseState <= 0) pulseDirection *= -1;
}

void drawCornerSpin() {
  int cornerSize = 15;
  int corners[4][2] = {{0, 0}, {SCREEN_WIDTH - cornerSize, 0},
                       {SCREEN_WIDTH - cornerSize, SCREEN_HEIGHT - cornerSize}, {0, SCREEN_HEIGHT - cornerSize}};
  int activeCorner = (cornerAngle / 20) % 4;

  for (int c = 0; c < 4; c++) {
    if (c == activeCorner) {
      u8g2.drawBox(corners[c][0], corners[c][1], cornerSize, cornerSize);
    } else {
      u8g2.drawFrame(corners[c][0], corners[c][1], cornerSize, cornerSize);
    }
  }
  cornerAngle = (cornerAngle + 3) % 80;
}

void drawScanLine() {
  for (int i = 0; i < 3; i++) {
    int y = (scanPos + i) % SCREEN_HEIGHT;
    u8g2.drawHLine(0, y, SCREEN_WIDTH);
  }
  u8g2.drawFrame(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
  scanPos = (scanPos + 2) % SCREEN_HEIGHT;
}

void drawBreathingBox() {
  int margin = breatheSize / 3;
  u8g2.drawFrame(margin, margin, SCREEN_WIDTH - 2*margin, SCREEN_HEIGHT - 2*margin);
  u8g2.drawFrame(margin + 2, margin + 2, SCREEN_WIDTH - 2*margin - 4, SCREEN_HEIGHT - 2*margin - 4);

  breatheSize += breatheDir;
  if (breatheSize >= 30 || breatheSize <= 0) breatheDir *= -1;
}

void drawBorderPixel(int pos, int thickness) {
  int x, y;
  int perimeter = 2 * (SCREEN_WIDTH + SCREEN_HEIGHT);
  pos = pos % perimeter;

  if (pos < SCREEN_WIDTH) {
    x = pos; y = 0;
    for (int t = 0; t < thickness; t++) u8g2.drawPixel(x, y + t);
  } else if (pos < SCREEN_WIDTH + SCREEN_HEIGHT) {
    x = SCREEN_WIDTH - 1; y = pos - SCREEN_WIDTH;
    for (int t = 0; t < thickness; t++) u8g2.drawPixel(x - t, y);
  } else if (pos < 2 * SCREEN_WIDTH + SCREEN_HEIGHT) {
    x = SCREEN_WIDTH - 1 - (pos - SCREEN_WIDTH - SCREEN_HEIGHT); y = SCREEN_HEIGHT - 1;
    for (int t = 0; t < thickness; t++) u8g2.drawPixel(x, y - t);
  } else {
    x = 0; y = SCREEN_HEIGHT - 1 - (pos - 2 * SCREEN_WIDTH - SCREEN_HEIGHT);
    for (int t = 0; t < thickness; t++) u8g2.drawPixel(x + t, y);
  }
}

// ===========================================
// PIXEL PAINT MODE
// ===========================================
void loopPixelPaint() {
  unsigned long now = millis();

  if (now - lastCanvasFetch > CANVAS_FETCH_INTERVAL) {
    fetchCanvas();
    lastCanvasFetch = now;
  }

  // Always draw the canvas, even if empty (no "no canvas data" message)
  drawCanvasToOLED(canvas);

  delay(50);
}

void fetchCanvas() {
  HTTPClient http;
  http.setTimeout(3000);

  String url = String(API_BASE_URL) + "/pixel-paint?apiKey=" + String(apiKey);

  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      String canvasData = doc["canvas"].as<String>();
      pixelPaintHasData = (canvasData.length() > 0);
      parseCanvasData(canvasData, canvas);
      // Canvas will be drawn in loopPixelPaint(), whether empty or not
    }
  }

  http.end();
}

void parseCanvasData(String canvasData, bool targetCanvas[96][48]) {
  memset(targetCanvas, 0, 96 * 48 * sizeof(bool));

  if (canvasData.length() == 0) return;

  int i = 0;
  while (i < canvasData.length()) {
    int comma = canvasData.indexOf(',', i);
    int semi = canvasData.indexOf(';', i);

    if (comma < 0) break;

    int x = canvasData.substring(i, comma).toInt();
    int y = canvasData.substring(comma + 1, semi < 0 ? canvasData.length() : semi).toInt();

    if (x >= 0 && x < GRID_W && y >= 0 && y < GRID_H) {
      targetCanvas[x][y] = true;
    }

    if (semi < 0) break;
    i = semi + 1;
  }
}

void drawCanvasToOLED(bool sourceCanvas[96][48]) {
  u8g2.clearBuffer();

  float scaleX = (float)SCREEN_WIDTH / GRID_W;
  float scaleY = (float)SCREEN_HEIGHT / GRID_H;

  for (int x = 0; x < GRID_W; x++) {
    for (int y = 0; y < GRID_H; y++) {
      if (sourceCanvas[x][y]) {
        int ox = (int)(x * scaleX);
        int oy = (int)(y * scaleY);

        int w = (int)((x + 1) * scaleX) - ox;
        int h = (int)((y + 1) * scaleY) - oy;
        if (w < 1) w = 1;
        if (h < 1) h = 1;

        u8g2.drawBox(ox, oy, w, h);
      }
    }
  }

  u8g2.sendBuffer();
}

// ===========================================
// IMAGE TO PIXEL MODE
// ===========================================
void loopImageToPixel() {
  unsigned long now = millis();

  if (now - lastImagePixelFetch > IMAGE_PIXEL_FETCH_INTERVAL) {
    fetchImagePixel();
    lastImagePixelFetch = now;
  }

  // Always draw the canvas, even if empty (consistent with pixel paint)
  if (!imagePixelHasData) {
    drawDefaultImageToPixel();
  } else {
    drawCanvasToOLED(imagePixelCanvas);
  }

  delay(50);
}

void fetchImagePixel() {
  HTTPClient http;
  http.setTimeout(3000);

  String url = String(API_BASE_URL) + "/image-to-pixel?apiKey=" + String(apiKey);

  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      String canvasData = doc["canvas"].as<String>();
      imagePixelHasData = (canvasData.length() > 0);
      parseCanvasData(canvasData, imagePixelCanvas);
      if (imagePixelHasData) {
        drawCanvasToOLED(imagePixelCanvas);
      } else {
        drawDefaultImageToPixel();
      }
    }
  }

  http.end();
}

// ===========================================
// WEATHER MONITOR MODE
// ===========================================
void loopWeather() {
  unsigned long now = millis();

  if (now - lastWeatherSettingsFetch > WEATHER_SETTINGS_INTERVAL) {
    fetchWeatherSettings();
    lastWeatherSettingsFetch = now;
  }

  // Check if no city is selected
  if (currentCity < 0) {
    drawNoLocationChosen();
    delay(100);  // Reduced delay
    return;
  }

  if (now - lastWeatherFetch > WEATHER_FETCH_INTERVAL || currentCity != lastCity) {
    if (currentCity != lastCity) {
      applyTimezone();
      lastCity = currentCity;
    }
    fetchWeather();
    weatherDataLoaded = true;
    lastWeatherFetch = now;
  }

  drawWeatherOLED();
  delay(100);  // Reduced from 1000ms to 100ms for smooth second updates
}

void fetchWeatherSettings() {
  HTTPClient http;
  http.setTimeout(3000);

  String url = String(API_BASE_URL) + "/weather?apiKey=" + String(apiKey);

  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      // Check if selectedCity exists and is valid
      if (doc.containsKey("selectedCity") && !doc["selectedCity"].isNull()) {
        int newCity = doc["selectedCity"].as<int>();
        if (newCity >= 0 && newCity < NUM_CITIES) {
          if (newCity != currentCity) {
            currentCity = newCity;
            weatherDataLoaded = false;
            Serial.println("City changed to: " + String(cities[currentCity].name));
          }
        }
      } else {
        currentCity = -1;  // No city selected
      }
    }
  }

  http.end();
}

void fetchWeather() {
  if (strlen(apiKey) == 0) {
    Serial.println("API key not configured");
    return;
  }

  HTTPClient http;
  http.setTimeout(10000);

  String city = cities[currentCity].query;
  city.replace(" ", "%20");

  // Use Firebase proxy for weather data
  String url = String(API_BASE_URL) + "/weather-data?apiKey=" + String(apiKey) + "&city=" + city;

  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, http.getString());

    if (!error) {
      cities[currentCity].temp = doc["temp"].as<float>();
      cities[currentCity].humidity = doc["humidity"].as<int>();
      Serial.println("Weather updated for " + String(cities[currentCity].name));
    }
  } else {
    Serial.println("Weather fetch failed: " + String(httpCode));
  }

  http.end();
}

void applyTimezone() {
  setenv("TZ", cities[currentCity].tz, 1);
  tzset();
}

void drawWeatherOLED() {
  struct tm t;
  if (!getLocalTime(&t)) return;

  int hour = t.tm_hour;
  bool pm = hour >= 12;
  if (hour == 0) hour = 12;
  if (hour > 12) hour -= 12;

  char timeStr[16];
  sprintf(timeStr, "%02d:%02d:%02d %s", hour, t.tm_min, t.tm_sec, pm ? "PM" : "AM");

  char dateStr[24];
  strftime(dateStr, sizeof(dateStr), "%a, %d %b %Y", &t);

  char tempBuf[20], humBuf[20];
  sprintf(tempBuf, "Temp: %.1f C", cities[currentCity].temp);
  sprintf(humBuf, "Hum : %d %%", cities[currentCity].humidity);

  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawStr(0, 10, cities[currentCity].name);

  u8g2.setFont(u8g2_font_8x13_tf);
  u8g2.drawStr(0, 26, timeStr);

  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawStr(0, 40, dateStr);
  u8g2.drawStr(0, 52, tempBuf);
  u8g2.drawStr(0, 64, humBuf);

  u8g2.sendBuffer();
}

// ===========================================
// UTILITY FUNCTIONS
// ===========================================
void showMessage(String msg) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);

  int y = 20;
  int lineStart = 0;

  for (int i = 0; i <= msg.length(); i++) {
    if (i == msg.length() || msg[i] == '\n') {
      String line = msg.substring(lineStart, i);
      int x = (SCREEN_WIDTH - line.length() * 6) / 2;
      u8g2.drawStr(x, y, line.c_str());
      y += 12;
      lineStart = i + 1;
    }
  }

  u8g2.sendBuffer();
}

// ===========================================
// WELCOME & STATUS SCREENS
// ===========================================

// Draw the ESPCON logo bitmap
void drawLogo(int x, int y) {
  u8g2.drawXBMP(x, y, 32, 32, ESPCON_LOGO);
}

// Welcome screen when no sketch is chosen
void showWelcomeScreen() {
  static int animFrame = 0;

  u8g2.clearBuffer();

  // Draw logo centered at top
  drawLogo(48, 2);

  // Draw "ESPCON" text
  u8g2.setFont(u8g2_font_7x14B_tr);
  const char* title = "ESPCON";
  int titleWidth = strlen(title) * 7;
  u8g2.drawStr((SCREEN_WIDTH - titleWidth) / 2, 48, title);

  // Draw website URL with subtle animation
  u8g2.setFont(u8g2_font_5x7_tr);
  const char* url = "espcon.vercel.app";
  int urlWidth = strlen(url) * 5;
  u8g2.drawStr((SCREEN_WIDTH - urlWidth) / 2, 60, url);

  // Animated border dots
  int dotPos = animFrame % (2 * (SCREEN_WIDTH + SCREEN_HEIGHT));
  for (int i = 0; i < 8; i++) {
    int pos = (dotPos + i * 10) % (2 * (SCREEN_WIDTH + SCREEN_HEIGHT));
    int px, py;
    if (pos < SCREEN_WIDTH) {
      px = pos; py = 0;
    } else if (pos < SCREEN_WIDTH + SCREEN_HEIGHT) {
      px = SCREEN_WIDTH - 1; py = pos - SCREEN_WIDTH;
    } else if (pos < 2 * SCREEN_WIDTH + SCREEN_HEIGHT) {
      px = SCREEN_WIDTH - 1 - (pos - SCREEN_WIDTH - SCREEN_HEIGHT); py = SCREEN_HEIGHT - 1;
    } else {
      px = 0; py = SCREEN_HEIGHT - 1 - (pos - 2 * SCREEN_WIDTH - SCREEN_HEIGHT);
    }
    u8g2.drawPixel(px, py);
  }

  animFrame += 2;
  u8g2.sendBuffer();
}

// No WiFi connected screen
void showNoWiFi() {
  u8g2.clearBuffer();

  // Draw logo
  drawLogo(48, 0);

  // Message
  u8g2.setFont(u8g2_font_6x10_tr);
  const char* line1 = "No WiFi";
  const char* line2 = "Connected";
  const char* line3 = "Reconnecting...";

  u8g2.drawStr((SCREEN_WIDTH - strlen(line1) * 6) / 2, 42, line1);
  u8g2.drawStr((SCREEN_WIDTH - strlen(line2) * 6) / 2, 53, line2);
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr((SCREEN_WIDTH - strlen(line3) * 5) / 2, 63, line3);

  u8g2.sendBuffer();
}

// No API key configured screen
void showNoApiKey() {
  u8g2.clearBuffer();

  // Draw logo
  drawLogo(48, 0);

  // Message
  u8g2.setFont(u8g2_font_5x8_tr);
  const char* line1 = "No API Key";
  const char* line2 = "Hold BOOT 3s";
  const char* line3 = "to configure";
  const char* line4 = "espcon.vercel.app";

  u8g2.drawStr((SCREEN_WIDTH - strlen(line1) * 5) / 2, 40, line1);
  u8g2.drawStr((SCREEN_WIDTH - strlen(line2) * 5) / 2, 50, line2);
  u8g2.drawStr((SCREEN_WIDTH - strlen(line3) * 5) / 2, 58, line3);
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr((SCREEN_WIDTH - strlen(line4) * 5) / 2, 64, line4);

  u8g2.sendBuffer();
}

// First time setup screen (no WiFi or API key)
void showFirstTimeSetup() {
  u8g2.clearBuffer();

  // Draw logo
  drawLogo(48, 0);

  // Message
  u8g2.setFont(u8g2_font_6x10_tr);
  const char* line1 = "First Setup";
  const char* line2 = "Connect to:";

  u8g2.drawStr((SCREEN_WIDTH - strlen(line1) * 6) / 2, 40, line1);
  u8g2.drawStr((SCREEN_WIDTH - strlen(line2) * 6) / 2, 50, line2);

  u8g2.setFont(u8g2_font_7x14B_tr);
  const char* ssid = "ESP32-Setup";
  u8g2.drawStr((SCREEN_WIDTH - strlen(ssid) * 7) / 2, 63, ssid);

  u8g2.sendBuffer();
}

// WiFi connected but no API key configured
void showNoApiKeyConnected() {
  u8g2.clearBuffer();

  // Draw logo
  drawLogo(48, 0);

  // Message
  u8g2.setFont(u8g2_font_5x8_tr);
  const char* line1 = "WiFi Connected!";
  const char* line2 = "Get API key at:";
  const char* line3 = "espcon.vercel.app";
  const char* line4 = "Hold BOOT 3s";
  const char* line5 = "to add key";

  u8g2.drawStr((SCREEN_WIDTH - strlen(line1) * 5) / 2, 36, line1);
  u8g2.drawStr((SCREEN_WIDTH - strlen(line2) * 5) / 2, 45, line2);

  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr((SCREEN_WIDTH - strlen(line3) * 6) / 2, 54, line3);

  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr((SCREEN_WIDTH - strlen(line4) * 5) / 2, 62, line4);

  u8g2.sendBuffer();
}

// Default display for Pixel Paint (draws pixel art style logo)
void drawDefaultPixelPaint() {
  u8g2.clearBuffer();

  // Draw a pixel art style "brush/paint" icon in center
  // Simple paint brush icon made of pixels
  int startX = 48;
  int startY = 8;

  // Draw a simple paintbrush pixel art
  // Brush tip
  u8g2.drawBox(startX + 12, startY, 8, 4);
  u8g2.drawBox(startX + 8, startY + 4, 16, 4);
  u8g2.drawBox(startX + 4, startY + 8, 24, 8);
  // Handle
  u8g2.drawBox(startX + 10, startY + 16, 12, 20);
  u8g2.drawFrame(startX + 8, startY + 16, 16, 20);

  // Text
  u8g2.setFont(u8g2_font_6x10_tr);
  const char* line1 = "Pixel Paint";
  const char* line2 = "No canvas data";
  u8g2.drawStr((SCREEN_WIDTH - strlen(line1) * 6) / 2, 52, line1);
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr((SCREEN_WIDTH - strlen(line2) * 5) / 2, 62, line2);

  u8g2.sendBuffer();
}

// Default display for Image to Pixel (draws logo)
void drawDefaultImageToPixel() {
  u8g2.clearBuffer();

  // Draw the ESPCON logo
  drawLogo(48, 2);

  // Text
  u8g2.setFont(u8g2_font_6x10_tr);
  const char* line1 = "Image to Pixel";
  const char* line2 = "No image data";
  u8g2.drawStr((SCREEN_WIDTH - strlen(line1) * 6) / 2, 48, line1);
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr((SCREEN_WIDTH - strlen(line2) * 5) / 2, 60, line2);

  u8g2.sendBuffer();
}

// No location chosen for weather
void drawNoLocationChosen() {
  u8g2.clearBuffer();

  // Draw a simple location/pin icon
  int cx = 64;
  int cy = 16;

  // Location pin shape
  u8g2.drawCircle(cx, cy, 8);
  u8g2.drawDisc(cx, cy, 4);
  u8g2.drawTriangle(cx - 8, cy + 4, cx + 8, cy + 4, cx, cy + 20);

  // Clear inner part of triangle to make pin shape
  u8g2.setDrawColor(0);
  u8g2.drawBox(cx - 3, cy + 8, 6, 8);
  u8g2.setDrawColor(1);

  // Text
  u8g2.setFont(u8g2_font_6x10_tr);
  const char* line1 = "Weather Monitor";
  const char* line2 = "No Location";
  const char* line3 = "Chosen";

  u8g2.drawStr((SCREEN_WIDTH - strlen(line1) * 6) / 2, 42, line1);
  u8g2.drawStr((SCREEN_WIDTH - strlen(line2) * 6) / 2, 53, line2);
  u8g2.drawStr((SCREEN_WIDTH - strlen(line3) * 6) / 2, 63, line3);

  u8g2.sendBuffer();
}
