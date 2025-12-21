#include <Arduino.h>

// Base frequency and trimmer Ranges  
#define BASE_FREQ  79.8f          
#define MIN_FREQUENCY_OFFSET 0.6f
#define MAX_FREQUENCY_OFFSET 5.0f
#define MIN_BRIGHTNESS 2          
#define MAX_BRIGHTNESS 10.0f
#define DEBOUNCE_US 50000        

const int LED_strip = 3;        
const int EMagnet = 9;          
const int EMagnet2 = 10;        
const int ButtonSW = 8;         

boolean led_on = true;
boolean mode_changed = true;
volatile byte mode = 1;         
volatile boolean button_pressed = false;

float frequency_offset = MIN_FREQUENCY_OFFSET;
float duty_eMagnet = 20.0f;
float frequency_eMagnet = BASE_FREQ;  
float duty_led = 7.0f;  
float frequency_led = frequency_eMagnet + frequency_offset; 

unsigned long lastmillis = 0;
unsigned long minutes = 60000UL * 15;
unsigned long last_interrupt_time = 0;

// Function prototypes (must be declared before use)
void eMagnet_on();
void eMagnet_off();

void IRAM_ATTR buttonISR() {
  unsigned long interrupt_time = micros();
  if (interrupt_time - last_interrupt_time > DEBOUNCE_US) {
    button_pressed = true;
    last_interrupt_time = interrupt_time;
  }
}

void setup() {
  pinMode(ButtonSW, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ButtonSW), buttonISR, FALLING);
  
  ledcAttach(LED_strip, frequency_led, 8);
  ledcAttach(EMagnet, frequency_eMagnet, 8);
  ledcAttach(EMagnet2, frequency_eMagnet, 8);
  
  lastmillis = millis();
}

void loop() {
  // Handle timeout
  if(millis() - lastmillis > minutes) {
    mode = 0;
    ledcWrite(EMagnet, 0);
    ledcWrite(EMagnet2, 0);
    lastmillis = millis();
  }
  
  // Handle button press from ISR (non-blocking)
  if (button_pressed) {
    noInterrupts();
    byte current_mode = mode;  // Copy volatile safely
    current_mode++;
    if (current_mode > 3) current_mode = 1;
    mode = current_mode;       // Write back safely
    mode_changed = true;
    button_pressed = false;
    interrupts();
  }
  
  // Handle mode changes
  if (mode_changed) {
    if (mode == 1) {
      frequency_eMagnet = BASE_FREQ;
      ledcAttach(EMagnet, frequency_eMagnet, 8);
      ledcAttach(EMagnet2, frequency_eMagnet, 8);
      eMagnet_on();    
      led_on = true;
      lastmillis = millis();
    } else if (mode == 2) {
      eMagnet_off(); 
    } else if (mode == 3) {
      led_on = false;
    }    
    mode_changed = false; 
  }

  // LED control
  if (led_on) {
    duty_led = MAX_BRIGHTNESS;
    frequency_led = frequency_eMagnet + frequency_offset;
    ledcAttach(LED_strip, frequency_led, 8);
    ledcWrite(LED_strip, round(duty_led * 255 / 100));
  } else {
    ledcWrite(LED_strip, 0);
  }
}

void eMagnet_on() {
  ledcWrite(EMagnet, round(duty_eMagnet * 255 / 100));
  ledcWrite(EMagnet2, round(duty_eMagnet * 255 / 100));
}

void eMagnet_off() {
  ledcWrite(EMagnet, 0);  
  ledcWrite(EMagnet2, 0); 
}
