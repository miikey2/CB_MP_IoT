#include <Wire.h>

//SDA = A4
//SCL = A5

unsigned long lastDebounce = 0;
const uint8_t debounceDelay = 50;

// Button Pins from L -> R
const uint8_t buttonPins[7] = {2,3,4,5,6,7,8};
uint8_t currentMask = 0;
uint8_t lastMask = 0;
uint8_t currentButtonState = 0;


void setup() {
  //begin I2C as master
  Wire.begin();
  Serial.begin(9600);

  // Set up pins
  for (int i = 0; i < 7; i++){
    pinMode(buttonPins[i], INPUT);
  }
  
}

void loop() {
  // put your main code here, to run repeatedly:
 check_Buttons();
}

//Multipin debounce
void check_Buttons(){

  // Reset current mask
  currentMask = 0;

  // Read pin and push left until value reads pins 8-7-6-5-4-3-2
  for (int i = 6; i >= 0; i--){
    currentMask = currentMask << 1;
    currentMask |= digitalRead(buttonPins[i]);
     
  }

  
  // Reset timer if disparity between masks
  if (currentMask != lastMask){
    lastDebounce = millis();
  }

  // Full debounce passed?
  if ((millis() - lastDebounce) > debounceDelay){
    // New state?
    if (currentMask != currentButtonState){
      currentButtonState = currentMask;
    
      if (currentMask != 0){
        uint8_t sendMask = currentMask;

        // Cycle mask until set bit found
        for (int i = 1; i < 8; i++){
          if(sendMask & 0x1 == 1){
            send_I2C(i);
            break;
          }
          sendMask = sendMask >> 1;
        }
      }
    }
  }
  
  lastMask = currentMask;
}

void send_I2C(int i){
  Wire.beginTransmission(2);
  Wire.write(i);
  Wire.endTransmission();
}

