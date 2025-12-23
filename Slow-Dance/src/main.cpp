#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ElegantOTA.h>
#include <Preferences.h>

// Debug configuration
#define DEBUG  // Comment out to disable debug output

#ifndef DEBUG_PRINT
#ifdef DEBUG
#define DEBUG_PRINT(x) Serial.println(x)
#else
#define DEBUG_PRINT(x)
#endif
#endif

// WiFi credentials - CHANGE THESE!
const char* ssid = "DUSHYANT-NEW";
const char* password = "ahuja987";

// GPIO pins
const int LED_PIN = 27;
const int MAGNET_PIN = 14;
const int MAGNET2_PIN = 12;
const int BUTTON_PIN = 26;

// Default values
#define DEFAULT_LED_FREQ 80.5
#define DEFAULT_MAGNET_FREQ 79.8
#define DEFAULT_LED_DUTY 50.0
#define DEFAULT_MAGNET_DUTY 50.0

// Settings (will be loaded from preferences)
float LED_FREQ = DEFAULT_LED_FREQ;
float MAGNET_FREQ = DEFAULT_MAGNET_FREQ;
float LED_DUTY = DEFAULT_LED_DUTY;
float MAGNET_DUTY = DEFAULT_MAGNET_DUTY;

// Timer handles
hw_timer_t *ledTimer = NULL;
hw_timer_t *magnetTimer = NULL;

// State variables
volatile bool ledState = false;
volatile bool magnetState = false;
volatile uint32_t ledHighUs = 0;
volatile uint32_t ledLowUs = 0;
volatile uint32_t magnetHighUs = 0;
volatile uint32_t magnetLowUs = 0;
volatile bool deviceEnabled = true;
volatile bool buttonPressed = false;
volatile unsigned long lastButtonTime = 0;

const unsigned long DEBOUNCE_DELAY = 300;

// Web server and preferences
WebServer server(80);
Preferences preferences;

// Function prototypes
void updateLEDTimer();
void updateMagnetTimer();
void saveSettings();
void loadSettings();

void IRAM_ATTR onLedTimer() {
  if (!deviceEnabled) return;
  
  ledState = !ledState;
  digitalWrite(LED_PIN, ledState);
  
  timerRestart(ledTimer);
  if (ledState) {
    timerAlarm(ledTimer, ledHighUs, false, 0);
  } else {
    timerAlarm(ledTimer, ledLowUs, false, 0);
  }
}

void IRAM_ATTR onMagnetTimer() {
  if (!deviceEnabled) return;
  
  magnetState = !magnetState;
  digitalWrite(MAGNET_PIN, magnetState);
  digitalWrite(MAGNET2_PIN, magnetState);
  
  timerRestart(magnetTimer);
  if (magnetState) {
    timerAlarm(magnetTimer, magnetHighUs, false, 0);
  } else {
    timerAlarm(magnetTimer, magnetLowUs, false, 0);
  }
}

void IRAM_ATTR onButtonPress() {
  unsigned long currentTime = millis();
  if (currentTime - lastButtonTime > DEBOUNCE_DELAY) {
    buttonPressed = true;
    lastButtonTime = currentTime;
  }
}

void updateLEDTimer() {
  float ledPeriodUs = 1000000.0f / LED_FREQ;
  ledHighUs = (uint32_t)(ledPeriodUs * (LED_DUTY / 100.0f));
  ledLowUs = (uint32_t)(ledPeriodUs * (1.0f - LED_DUTY / 100.0f));
  
  if (ledTimer != NULL && deviceEnabled) {
    timerRestart(ledTimer);
    timerAlarm(ledTimer, ledLowUs, false, 0);
  }
}

void updateMagnetTimer() {
  float magnetPeriodUs = 1000000.0f / MAGNET_FREQ;
  magnetHighUs = (uint32_t)(magnetPeriodUs * (MAGNET_DUTY / 100.0f));
  magnetLowUs = (uint32_t)(magnetPeriodUs * (1.0f - MAGNET_DUTY / 100.0f));
  
  if (magnetTimer != NULL && deviceEnabled) {
    timerRestart(magnetTimer);
    timerAlarm(magnetTimer, magnetLowUs, false, 0);
  }
}

void saveSettings() {
  preferences.begin("slowmo", false);
  preferences.putFloat("led_freq", LED_FREQ);
  preferences.putFloat("mag_freq", MAGNET_FREQ);
  preferences.putFloat("led_duty", LED_DUTY);
  preferences.putFloat("mag_duty", MAGNET_DUTY);
  preferences.end();
  DEBUG_PRINT("Settings saved to flash");
}

void loadSettings() {
  preferences.begin("slowmo", true);
  LED_FREQ = preferences.getFloat("led_freq", DEFAULT_LED_FREQ);
  MAGNET_FREQ = preferences.getFloat("mag_freq", DEFAULT_MAGNET_FREQ);
  LED_DUTY = preferences.getFloat("led_duty", DEFAULT_LED_DUTY);
  MAGNET_DUTY = preferences.getFloat("mag_duty", DEFAULT_MAGNET_DUTY);
  preferences.end();
  
  #ifdef DEBUG
  Serial.printf("Loaded settings: LED=%.1fHz@%.0f%%, Magnet=%.1fHz@%.0f%%\n", 
                LED_FREQ, LED_DUTY, MAGNET_FREQ, MAGNET_DUTY);
  #endif
}

void handleRoot() {
  String html = R"(
<!DOCTYPE html>
<html>
<head>
  <meta charset='UTF-8'>
  <meta name='viewport' content='width=device-width, initial-scale=1'>
  <title>Slow Motion Controller</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      max-width: 600px;
      margin: 20px auto;
      padding: 20px;
      background: #1a1a2e;
      color: #eee;
    }
    h1 { text-align: center; color: #00d4ff; }
    .section {
      background: #16213e;
      padding: 20px;
      margin: 15px 0;
      border-radius: 8px;
      border: 1px solid #00d4ff;
    }
    .section h2 { margin-top: 0; color: #00d4ff; font-size: 1.2em; }
    label {
      display: block;
      margin: 10px 0 5px;
      font-weight: bold;
    }
    input[type='number'] {
      width: 100%;
      padding: 10px;
      background: #0f3460;
      border: 1px solid #00d4ff;
      border-radius: 5px;
      color: #eee;
      font-size: 16px;
      box-sizing: border-box;
    }
    .value-display {
      display: inline-block;
      float: right;
      color: #00d4ff;
      font-weight: bold;
    }
    button {
      width: 100%;
      padding: 15px;
      margin: 10px 0;
      font-size: 16px;
      border: none;
      border-radius: 5px;
      cursor: pointer;
      font-weight: bold;
      transition: all 0.3s;
    }
    .btn-save { background: #00d4ff; color: #000; }
    .btn-reset { background: #e74c3c; color: #fff; }
    .btn-ota { background: #f39c12; color: #000; }
    button:hover { opacity: 0.8; transform: scale(1.02); }
    .status {
      text-align: center;
      padding: 10px;
      margin: 20px 0;
      background: #0f3460;
      border-radius: 5px;
      font-weight: bold;
    }
    .info { font-size: 0.9em; color: #aaa; margin-top: 5px; }
  </style>
</head>
<body>
  <h1>Slow Motion Controller</h1>
  
  <div class='status' id='status'>Loading...</div>
  
  <div class='section'>
    <h2>LED Strip</h2>
    <label>Frequency (Hz): <span class='value-display' id='ledFreqVal'>0</span></label>
    <input type='number' id='ledFreq' min='50' max='150' step='0.1' value='80.5'>
    <div class='info'>Recommended: 70-90 Hz</div>
    
    <label>Brightness (%): <span class='value-display' id='ledDutyVal'>0</span></label>
    <input type='number' id='ledDuty' min='1' max='100' step='1' value='50'>
    <div class='info'>Recommended: 30-70%</div>
  </div>
  
  <div class='section'>
    <h2>Electromagnets</h2>
    <label>Frequency (Hz): <span class='value-display' id='magFreqVal'>0</span></label>
    <input type='number' id='magFreq' min='50' max='150' step='0.1' value='79.8'>
    <div class='info'>Recommended: 70-90 Hz</div>
    
    <label>Duty Cycle (%): <span class='value-display' id='magDutyVal'>0</span></label>
    <input type='number' id='magDuty' min='10' max='90' step='1' value='50'>
    <div class='info'>Recommended: 40-60%</div>
  </div>
  
  <button class='btn-save' onclick='saveSettings()'>Save Settings</button>
  <button class='btn-reset' onclick='resetDefaults()'>Reset to Defaults</button>
  <button class='btn-ota' onclick='location.href="/update"'>OTA Update</button>

  <script>
    let autoRefresh = true;
    let userIsEditing = false;
    
    function updateDisplays() {
      document.getElementById('ledFreqVal').textContent = document.getElementById('ledFreq').value;
      document.getElementById('ledDutyVal').textContent = document.getElementById('ledDuty').value;
      document.getElementById('magFreqVal').textContent = document.getElementById('magFreq').value;
      document.getElementById('magDutyVal').textContent = document.getElementById('magDuty').value;
    }
    
    // Mark when user is editing
    ['ledFreq', 'ledDuty', 'magFreq', 'magDuty'].forEach(id => {
      const elem = document.getElementById(id);
      elem.oninput = () => {
        updateDisplays();
        userIsEditing = true;
        setTimeout(() => { userIsEditing = false; }, 5000);
      };
    });
    
    function loadSettings() {
      if (userIsEditing) return; // Don't override while user is typing
      
      fetch('/api/settings')
        .then(r => r.json())
        .then(data => {
          document.getElementById('ledFreq').value = data.ledFreq;
          document.getElementById('ledDuty').value = data.ledDuty;
          document.getElementById('magFreq').value = data.magFreq;
          document.getElementById('magDuty').value = data.magDuty;
          updateDisplays();
          document.getElementById('status').textContent = 'Status: ' + (data.enabled ? 'ENABLED' : 'DISABLED');
        });
    }
    
    function saveSettings() {
      const data = {
        ledFreq: parseFloat(document.getElementById('ledFreq').value),
        ledDuty: parseFloat(document.getElementById('ledDuty').value),
        magFreq: parseFloat(document.getElementById('magFreq').value),
        magDuty: parseFloat(document.getElementById('magDuty').value)
      };
      
      fetch('/api/settings', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(data)
      })
      .then(r => r.text())
      .then(msg => {
        alert('Settings saved! New values applied.');
        userIsEditing = false;
        loadSettings();
      });
    }
    
    function resetDefaults() {
      if (confirm('Reset to default settings?')) {
        fetch('/api/reset', {method: 'POST'})
          .then(r => r.text())
          .then(msg => {
            alert('Settings reset to defaults!');
            userIsEditing = false;
            loadSettings();
          });
      }
    }
    
    loadSettings();
    setInterval(loadSettings, 3000);
  </script>
</body>
</html>
  )";
  server.send(200, "text/html", html);
}

void handleGetSettings() {
  String json = "{";
  json += "\"ledFreq\":" + String(LED_FREQ, 1) + ",";
  json += "\"ledDuty\":" + String(LED_DUTY, 1) + ",";
  json += "\"magFreq\":" + String(MAGNET_FREQ, 1) + ",";
  json += "\"magDuty\":" + String(MAGNET_DUTY, 1) + ",";
  json += "\"enabled\":" + String(deviceEnabled ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}

void handleSetSettings() {
  if (server.hasArg("plain")) {
    String body = server.arg("plain");
    
    // Simple JSON parsing
    int ledFreqIdx = body.indexOf("\"ledFreq\":");
    int ledDutyIdx = body.indexOf("\"ledDuty\":");
    int magFreqIdx = body.indexOf("\"magFreq\":");
    int magDutyIdx = body.indexOf("\"magDuty\":");
    
    if (ledFreqIdx > 0) LED_FREQ = body.substring(ledFreqIdx + 10).toFloat();
    if (ledDutyIdx > 0) LED_DUTY = body.substring(ledDutyIdx + 10).toFloat();
    if (magFreqIdx > 0) MAGNET_FREQ = body.substring(magFreqIdx + 10).toFloat();
    if (magDutyIdx > 0) MAGNET_DUTY = body.substring(magDutyIdx + 10).toFloat();
    
    // Update timers
    updateLEDTimer();
    updateMagnetTimer();
    
    // Save to flash
    saveSettings();
    
    #ifdef DEBUG
    Serial.printf("Updated: LED=%.1fHz@%.0f%%, Mag=%.1fHz@%.0f%%\n", 
                  LED_FREQ, LED_DUTY, MAGNET_FREQ, MAGNET_DUTY);
    #endif
    
    server.send(200, "text/plain", "Settings updated and saved");
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

void handleReset() {
  LED_FREQ = DEFAULT_LED_FREQ;
  MAGNET_FREQ = DEFAULT_MAGNET_FREQ;
  LED_DUTY = DEFAULT_LED_DUTY;
  MAGNET_DUTY = DEFAULT_MAGNET_DUTY;
  
  updateLEDTimer();
  updateMagnetTimer();
  saveSettings();
  
  DEBUG_PRINT("Settings reset to defaults");
  server.send(200, "text/plain", "Reset to defaults");
}

void setup() {
  #ifdef DEBUG
  Serial.begin(115200);
  delay(1000);
  #endif
  
  DEBUG_PRINT("\nESP32 Precise PWM with OTA Updates");
  
  // Load saved settings
  loadSettings();
  
  // Configure GPIO pins
  pinMode(LED_PIN, OUTPUT);
  pinMode(MAGNET_PIN, OUTPUT);
  pinMode(MAGNET2_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), onButtonPress, FALLING);
  
  // Test magnets
  DEBUG_PRINT("Testing magnet pins directly...");
  digitalWrite(MAGNET_PIN, HIGH);
  digitalWrite(MAGNET2_PIN, HIGH);
  delay(1000);
  digitalWrite(MAGNET_PIN, LOW);
  digitalWrite(MAGNET2_PIN, LOW);
  delay(500);
  DEBUG_PRINT("Direct test complete");
  
  digitalWrite(LED_PIN, LOW);
  digitalWrite(MAGNET_PIN, LOW);
  digitalWrite(MAGNET2_PIN, LOW);
  
  // Calculate timer values
  updateLEDTimer();
  updateMagnetTimer();
  
  #ifdef DEBUG
  Serial.printf("\nLED: %.1f Hz @ %.0f%% duty\n", LED_FREQ, LED_DUTY);
  Serial.printf("Magnet: %.1f Hz @ %.0f%% duty\n\n", MAGNET_FREQ, MAGNET_DUTY);
  #endif
  
  // Connect to WiFi
  DEBUG_PRINT("Connecting to WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  int wifiTimeout = 0;
  while (WiFi.status() != WL_CONNECTED && wifiTimeout < 20) {
    delay(500);
    #ifdef DEBUG
    Serial.print(".");
    #endif
    wifiTimeout++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    DEBUG_PRINT("\nWiFi connected!");
    #ifdef DEBUG
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Control panel: http://");
    Serial.println(WiFi.localIP());
    #endif
  } else {
    DEBUG_PRINT("\nWiFi connection failed - continuing without web interface");
  }
  
  // Setup web server
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/settings", HTTP_GET, handleGetSettings);
  server.on("/api/settings", HTTP_POST, handleSetSettings);
  server.on("/api/reset", HTTP_POST, handleReset);
  
  ElegantOTA.begin(&server);
  server.begin();
  DEBUG_PRINT("Web server started");
  
  // Create timers
  DEBUG_PRINT("\nCreating timers...");
  ledTimer = timerBegin(1000000);
  magnetTimer = timerBegin(1000000);
  
  if (ledTimer == NULL || magnetTimer == NULL) {
    DEBUG_PRINT("ERROR: Failed to create timers!");
    return;
  }
  
  timerAttachInterrupt(ledTimer, &onLedTimer);
  timerAttachInterrupt(magnetTimer, &onMagnetTimer);
  
  timerAlarm(ledTimer, ledLowUs, false, 0);
  timerAlarm(magnetTimer, magnetLowUs, false, 0);
  
  DEBUG_PRINT("\n=================================");
  DEBUG_PRINT("PWM running!");
  DEBUG_PRINT("=================================\n");
}

void loop() {
  // Handle web server - MUST be called frequently
  server.handleClient();
  
  #ifdef DEBUG
  ElegantOTA.loop();
  #endif
  
  if (buttonPressed) {
    buttonPressed = false;
    
    // Quick check without blocking
    if (digitalRead(BUTTON_PIN) == LOW) {
      deviceEnabled = !deviceEnabled;
      
      if (deviceEnabled) {
        DEBUG_PRINT("\n>>> DEVICE ENABLED <<<");
        ledState = false;
        magnetState = false;
        digitalWrite(LED_PIN, LOW);
        digitalWrite(MAGNET_PIN, LOW);
        digitalWrite(MAGNET2_PIN, LOW);
        
        timerRestart(ledTimer);
        timerRestart(magnetTimer);
        timerAlarm(ledTimer, ledLowUs, false, 0);
        timerAlarm(magnetTimer, magnetLowUs, false, 0);
      } else {
        DEBUG_PRINT("\n>>> DEVICE DISABLED <<<");
        digitalWrite(LED_PIN, LOW);
        digitalWrite(MAGNET_PIN, LOW);
        digitalWrite(MAGNET2_PIN, LOW);
        ledState = false;
        magnetState = false;
      }
    }
  }
  
  #ifdef DEBUG
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 5000) {
    Serial.printf("Status: %s | LED: %.1fHz@%.0f%% | Mag: %.1fHz@%.0f%% | IP: %s\n", 
                  deviceEnabled ? "ON" : "OFF",
                  LED_FREQ, LED_DUTY, MAGNET_FREQ, MAGNET_DUTY,
                  WiFi.localIP().toString().c_str());
    lastDebug = millis();
  }
  #endif
  
  // Minimal delay - don't block web server
  delay(1);
}