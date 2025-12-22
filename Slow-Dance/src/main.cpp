#include <Arduino.h>

// GPIO pins
const int LED_PIN = 27;
const int MAGNET_PIN = 14;

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

void IRAM_ATTR onLedTimer() {
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
  magnetState = !magnetState;
  digitalWrite(MAGNET_PIN, magnetState);
  
  // Restart timer with next period
  timerRestart(magnetTimer);
  if (magnetState) {
    timerAlarm(magnetTimer, magnetHighUs, false, 0);
  } else {
    timerAlarm(magnetTimer, magnetLowUs, false, 0);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\nESP32 Precise PWM with Hardware Timers");
  
  // Configure GPIO pins
  pinMode(LED_PIN, OUTPUT);
  pinMode(MAGNET_PIN, OUTPUT);
  
  // Test: Turn on magnet directly first
  Serial.println("Testing magnet pin directly...");
  digitalWrite(MAGNET_PIN, HIGH);
  delay(1000);
  digitalWrite(MAGNET_PIN, LOW);
  delay(500);
  Serial.println("Direct test complete - did magnet turn on?");
  
  digitalWrite(LED_PIN, LOW);
  digitalWrite(MAGNET_PIN, LOW);
  
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
  
  Serial.printf("\nLED Configuration:\n");
  Serial.printf("  Target: %.2f Hz, %.0f%% duty\n", LED_FREQ, LED_DUTY * 100);
  Serial.printf("  Actual: %.2f Hz\n", actualLedFreq);
  Serial.printf("  Period: %d us high, %d us low\n\n", ledHighUs, ledLowUs);
  
  Serial.printf("Magnet Configuration:\n");
  Serial.printf("  Target: %.2f Hz, %.0f%% duty\n", MAGNET_FREQ, MAGNET_DUTY * 100);
  Serial.printf("  Actual: %.2f Hz\n", actualMagnetFreq);
  Serial.printf("  Period: %d us high, %d us low\n\n", magnetHighUs, magnetLowUs);
  
  Serial.println("Creating timers...");
  
  // Create timers with 1MHz frequency (1us resolution)
  ledTimer = timerBegin(1000000);
  magnetTimer = timerBegin(1000000);
  
  if (ledTimer == NULL || magnetTimer == NULL) {
    Serial.println("ERROR: Failed to create timers!");
    return;
  }
  
  Serial.println("Attaching interrupts...");
  
  // Attach interrupt handlers
  timerAttachInterrupt(ledTimer, &onLedTimer);
  timerAttachInterrupt(magnetTimer, &onMagnetTimer);
  
  Serial.println("Starting timers...");
  
  // Start with LOW period
  timerAlarm(ledTimer, ledLowUs, false, 0);
  timerAlarm(magnetTimer, magnetLowUs, false, 0);
  
  Serial.println("PWM running!");
}

void loop() {
  // Debug output every 2 seconds
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 2000) {
    Serial.printf("LED state: %d, Magnet state: %d\n", ledState, magnetState);
    Serial.printf("LED pin: %d, Magnet pin: %d\n", 
                  digitalRead(LED_PIN), digitalRead(MAGNET_PIN));
    lastDebug = millis();
  }
  
  delay(100);
}