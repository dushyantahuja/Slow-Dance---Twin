#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ElegantOTA.h>

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

// Frequencies in Hz
const float LED_FREQ = 80.5;
const float MAGNET_FREQ = 79.8;

// Duty cycles (0.0 to 1.0)
const float LED_DUTY = 0.5;
const float MAGNET_DUTY = 0.5;

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

const unsigned long DEBOUNCE_DELAY = 300;  // 300ms debounce

// Web server
WebServer server(80);

void IRAM_ATTR onLedTimer() {
  if (!deviceEnabled) return;  // Don't toggle if disabled
  
  ledState = !ledState;
  digitalWrite(LED_PIN, ledState);
  
  // Restart timer with next period
  timerRestart(ledTimer);
  if (ledState) {
    timerAlarm(ledTimer, ledHighUs, false, 0);
  } else {
    timerAlarm(ledTimer, ledLowUs, false, 0);
  }
}

void IRAM_ATTR onMagnetTimer() {
  if (!deviceEnabled) return;  // Don't toggle if disabled
  
  magnetState = !magnetState;
  digitalWrite(MAGNET_PIN, magnetState);
  digitalWrite(MAGNET2_PIN, magnetState);  // Both magnets in sync
  
  // Restart timer with next period
  timerRestart(magnetTimer);
  if (magnetState) {
    timerAlarm(magnetTimer, magnetHighUs, false, 0);
  } else {
    timerAlarm(magnetTimer, magnetLowUs, false, 0);
  }
}

void IRAM_ATTR onButtonPress() {
  unsigned long currentTime = millis();
  // Strong debounce: ignore if pressed within last 300ms
  if (currentTime - lastButtonTime > DEBOUNCE_DELAY) {
    buttonPressed = true;
    lastButtonTime = currentTime;
  }
}

void setup() {
  #ifdef DEBUG
  Serial.begin(115200);
  delay(1000);
  #endif
  
  DEBUG_PRINT("\nESP32 Precise PWM with OTA Updates");
  
  // Configure GPIO pins
  pinMode(LED_PIN, OUTPUT);
  pinMode(MAGNET_PIN, OUTPUT);
  pinMode(MAGNET2_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Attach button interrupt (triggers on falling edge when button pressed)
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), onButtonPress, FALLING);
  
  // Test: Turn on both magnets directly first
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
  
  // Calculate periods in microseconds
  float ledPeriodUs = 1000000.0f / LED_FREQ;
  float magnetPeriodUs = 1000000.0f / MAGNET_FREQ;
  
  // Calculate high and low times
  ledHighUs = (uint32_t)(ledPeriodUs * LED_DUTY);
  ledLowUs = (uint32_t)(ledPeriodUs * (1.0f - LED_DUTY));
  magnetHighUs = (uint32_t)(magnetPeriodUs * MAGNET_DUTY);
  magnetLowUs = (uint32_t)(magnetPeriodUs * (1.0f - MAGNET_DUTY));
  
  // Calculate actual frequencies
  float actualLedFreq = 1000000.0f / (ledHighUs + ledLowUs);
  float actualMagnetFreq = 1000000.0f / (magnetHighUs + magnetLowUs);
  
  #ifdef DEBUG
  Serial.printf("\nLED Configuration:\n");
  Serial.printf("  Target: %.2f Hz, %.0f%% duty\n", LED_FREQ, LED_DUTY * 100);
  Serial.printf("  Actual: %.2f Hz\n", actualLedFreq);
  Serial.printf("  Period: %d us high, %d us low\n\n", ledHighUs, ledLowUs);
  
  Serial.printf("Magnet Configuration:\n");
  Serial.printf("  Target: %.2f Hz, %.0f%% duty\n", MAGNET_FREQ, MAGNET_DUTY * 100);
  Serial.printf("  Actual: %.2f Hz\n", actualMagnetFreq);
  Serial.printf("  Period: %d us high, %d us low\n\n", magnetHighUs, magnetLowUs);
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
    Serial.print("OTA URL: http://");
    Serial.print(WiFi.localIP());
    Serial.println("/update");
    #endif
  } else {
    DEBUG_PRINT("\nWiFi connection failed - continuing without OTA");
  }
  
  // Initialize ElegantOTA
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", 
      "<html><body><h1>ESP32 Slow Motion Controller</h1>"
      "<p>Device is running!</p>"
      "<p><a href='/update'>Go to OTA Update</a></p>"
      "</body></html>");
  });
  
  ElegantOTA.begin(&server);
  server.begin();
  DEBUG_PRINT("Web server started");
  
  DEBUG_PRINT("\nCreating timers...");
  
  // Create timers with 1MHz frequency (1us resolution)
  ledTimer = timerBegin(1000000);
  magnetTimer = timerBegin(1000000);
  
  if (ledTimer == NULL || magnetTimer == NULL) {
    DEBUG_PRINT("ERROR: Failed to create timers!");
    return;
  }
  
  DEBUG_PRINT("Attaching interrupts...");
  
  // Attach interrupt handlers
  timerAttachInterrupt(ledTimer, &onLedTimer);
  timerAttachInterrupt(magnetTimer, &onMagnetTimer);
  
  DEBUG_PRINT("Starting timers...");
  
  // Start with LOW period
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
  ElegantOTA.loop();  // Only needed for debug progress
  #endif
  
  // Handle button press
  if (buttonPressed) {
    buttonPressed = false;
    
    // Double-check button is still pressed (hardware debounce)
    delay(50);
    if (digitalRead(BUTTON_PIN) == LOW) {
      deviceEnabled = !deviceEnabled;
      
      if (deviceEnabled) {
        DEBUG_PRINT("\n>>> DEVICE ENABLED <<<");
        // Reset states and let timers resume
        ledState = false;
        magnetState = false;
        digitalWrite(LED_PIN, LOW);
        digitalWrite(MAGNET_PIN, LOW);
        digitalWrite(MAGNET2_PIN, LOW);
        
        // Restart timers
        timerRestart(ledTimer);
        timerRestart(magnetTimer);
        timerAlarm(ledTimer, ledLowUs, false, 0);
        timerAlarm(magnetTimer, magnetLowUs, false, 0);
      } else {
        DEBUG_PRINT("\n>>> DEVICE DISABLED <<<");
        // Turn everything off immediately
        digitalWrite(LED_PIN, LOW);
        digitalWrite(MAGNET_PIN, LOW);
        digitalWrite(MAGNET2_PIN, LOW);
        ledState = false;
        magnetState = false;
      }
      
      // Wait for button release
      while (digitalRead(BUTTON_PIN) == LOW) {
        delay(10);
      }
      delay(50);  // Extra delay after release
    }
  }
  
  // Debug output every 5 seconds
  #ifdef DEBUG
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 5000) {
    Serial.printf("Status: %s | LED: %d, Magnet: %d | IP: %s\n", 
                  deviceEnabled ? "ENABLED" : "DISABLED", 
                  ledState, magnetState,
                  WiFi.localIP().toString().c_str());
    lastDebug = millis();
  }
  #endif
  
  delay(10);
}