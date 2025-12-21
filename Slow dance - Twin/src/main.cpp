#include <Arduino.h>

#include <PWM.h>                  // PWM Frequency library available at https://code.google.com/archive/p/arduino-pwm-frequency-library/downloads
//#include <SerialCommand.h>

//SerialCommand sCmd;

//#define DEBUG
//uncomment to check serial monitor and see LED heartbeat
//Tactile Switch needs to be pressed longer in debug mode to change mode

//Base frequency and trimmer Ranges  
#define BASE_FREQ  79.8;          // 80 is on the spot for many flowers. Feel free to play with this +/-5Hz   //Set to 79.8 so that light banding is not too serious while videoing with shutter speed at 1/80 sec when light strobes at 80Hz
#define MIN_FREQUENCY_OFFSET 0.6
#define MAX_FREQUENCY_OFFSET 5.0
#define MIN_BRIGHTNESS 2          // allows light to be off to reveal the full oscillating effect
#define MAX_BRIGHTNESS 10.0       // too high and flickering will occur

const byte LED_strip = 3;        // pin for LED strip control
const byte EMagnet = 9;           // pin for Electromagnet control
const byte EMagnet2 = 10;           // pin for Electromagnet control
//const byte LED = 13;              // pin for on-board LED
const byte ButtonSW = 8;          // pin for mode selection button

boolean led_on = true;
boolean mode_changed = true;

byte mode = 3; //toggle it by button SW
//mode 1 = normal slow motion mode (power on)
//mode 2 = magnet off
//mode 3 = completely off

byte buttonState = 0;             // current state of the button
byte lastButtonState = 0;         // previous state of the button

float frequency_offset = MIN_FREQUENCY_OFFSET;
float duty_eMagnet = 20;          // 15; be carefull not to overheat the magnet with too high duty cycle. Better adjust force through magnet position
float frequency_eMagnet = BASE_FREQ;  
float duty_led = 7;  
float frequency_led = frequency_eMagnet+frequency_offset; 

int lastBrightnessValue = 0;

unsigned long lastmillis, minutes = 60000 * 15; // switch off after 15 minutes.

// Function Declarations
void freqUp();
void freqDn();
void eMagnet_on();
void eMagnet_off();

//**********************************************************************************************************************************************************
void setup()
{
  //Serial.begin(9600);

  //sCmd.addCommand("UP",    freqUp);
  //sCmd.addCommand("DN",    freqDn);
  
  //pinMode(LED, OUTPUT);      // Heart Beat LED
  pinMode(ButtonSW, INPUT_PULLUP); // Mode button
    
  //initialize all timers except for 0, to save time keeping functions
  InitTimersSafe(); 

  //sets the frequency for the specified pin
  bool success = SetPinFrequencySafe(LED_strip, frequency_led);
  //Serial.println(success);
  bool success2 = SetPinFrequencySafe(EMagnet, frequency_eMagnet);
  //Serial.println(success2);
  bool success3 = SetPinFrequencySafe(EMagnet2, frequency_eMagnet);
  //Serial.println(success3);
  
  lastmillis = millis();
}



//**********************************************************************************************************************************************************
void loop()
{     
  //sCmd.readSerial();
  if(millis() - lastmillis > minutes) {   // Switch off magnet after 15 minutes
    mode = 0;
    eMagnet_off();
    lastmillis = millis();
  }
  if (mode_changed == true)
  {
    if (mode == 1)  //normal slow motion mode (power on)
    {   
      frequency_eMagnet = BASE_FREQ;
      eMagnet_on();    
      led_on = true;
      lastmillis = millis();
    }
    else if (mode == 2)  // magnet off
    {
      eMagnet_off(); 
    }
    else if (mode == 3)  // completely off
    { 
      led_on = false;
    }    
    mode_changed = false; 
  }


  //frequency_offset = MAX_FREQUENCY_OFFSET; //Speed: 0.1 .. 5 Hz


  if (led_on == true){
    duty_led = MAX_BRIGHTNESS;  //Brightness: duty_led 2..20
    frequency_led = frequency_eMagnet+frequency_offset;
    SetPinFrequencySafe(LED_strip, frequency_led);
  }
  else {
    duty_led = 0;      
    pwmWrite(LED_strip, 0);
  }
  /*{
    duty_led = MAX_BRIGHTNESS;  //Brightness: duty_led 2..20
    frequency_led = frequency_eMagnet+frequency_offset;
    
    SetPinFrequencySafe(LED_strip, frequency_led);
    
    if (lastBrightnessValue < round(duty_led*255/100))  //previously dimmer - gradually bright it
    {
      for (int i=lastBrightnessValue; i<round(duty_led*255/100); i++)
      {
        pwmWrite(LED_strip, i);
        delay(30);
      }
    } 
    else if (lastBrightnessValue > round(duty_led*255/100)) //previously brighter - gradually dim it
    {
      for (int i=lastBrightnessValue; i>round(duty_led*255/100); i--)
      {
        pwmWrite(LED_strip, i);
        delay(30);      
      }
    }
    else  //no change in brightness
      pwmWrite(LED_strip, round(duty_led*255/100));   

    lastBrightnessValue = round(duty_led*255/100);
  }
  else
  {
    //gradually dim off
    for (int i=round(duty_led*255/100); i>0; i--)
    {
      pwmWrite(LED_strip, i);
      delay(30);
    }
      
    duty_led = 0;      
    pwmWrite(LED_strip, 0);
    lastBrightnessValue = 0;
  }*/


  #ifdef DEBUG
    //Heartbeat on-board LED
    digitalWrite(LED, HIGH); // LED on
    delay(300);
    digitalWrite(LED, LOW); // LED off
    delay(300); 
    digitalWrite(LED, HIGH); // LED on
    delay(200);
    digitalWrite(LED, LOW); // LED off
    delay(1200); 
    
    //serial print current parameters
    Serial.print("Frequency Offset: "); 
    Serial.print(frequency_offset);
    Serial.print("  Force: ");
    Serial.print(duty_eMagnet);
    Serial.print("  Freq Mag: ");
    Serial.print(frequency_eMagnet);
    Serial.print("  Freq LED: ");
    Serial.print(frequency_led);
    Serial.print("  Brightness: ");
    Serial.println(duty_led);
  #endif

    
  // read the button SW
  buttonState = digitalRead(ButtonSW);

  // compare the buttonState to its previous state
  if (buttonState != lastButtonState) 
  {
    // if the state has changed, increment the counter
    if (buttonState == LOW) 
    {    
      mode++;

      if (mode > 3)
        mode = 1; //rotary menu
      
      mode_changed = true ;      
    }

    // delay a little bit for button debouncing
    // delay(50);
    
  }
  lastButtonState = buttonState;
}

/*
void freqUp(){
  frequency_offset+=0.1;
  SetPinFrequencySafe(EMagnet, frequency_eMagnet);
  SetPinFrequencySafe(EMagnet2, frequency_eMagnet);
  Serial.println(frequency_offset);
}

void freqDn(){
  frequency_offset-=0.1;
  SetPinFrequencySafe(EMagnet, frequency_eMagnet);
  SetPinFrequencySafe(EMagnet2, frequency_eMagnet);
  Serial.println(frequency_offset);
}
*/

//**********************************************************************************************************************************************************
void eMagnet_on() 
{
  pwmWrite(EMagnet, round(duty_eMagnet*255/100));
  pwmWrite(EMagnet2, round(duty_eMagnet*255/100));
}



//**********************************************************************************************************************************************************
void eMagnet_off() 
{
  pwmWrite(EMagnet, 0);  
  pwmWrite(EMagnet2, 0); 
}
