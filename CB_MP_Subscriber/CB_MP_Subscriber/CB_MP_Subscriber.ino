#include <SPI.h>
#include <Ethernet2.h>
#include <PubSubClient.h>


// Needs changing for any different boards code is uploaded to.
byte mac[] = {0x90, 0xA2, 0xDA, 0x11, 0x3B, 0xC6};

// Set fallback network config in case of no DHCP server
IPAddress ip(192, 168, 0, 11);
IPAddress gateway(192, 168, 0, 254);
IPAddress subnet(255, 255, 255, 0);

// Create class instances for MQTT
EthernetClient ethernetClient;
PubSubClient client(ethernetClient);


/*
 * Pins are as follows
 * 
 * Heater     - I/O(!ADD PIN HERE!)   - ID = 0
 * Solenoid   - I/O(!ADD PIN HERE!)   - ID = 1
 * Steamer    - I/O(!ADD PIN HERE!)   - ID = 2
 * Exhaust    - PWM(!ADD PIN HERE!)   - ID = 3
 * Intake     - PWM(!ADD PIN HERE!)   - ID = 4
 * Lighting   - PWM(D3)               - ID = 5
 * Buzzer     - PWM(D6)               - ID = 6
 * 
 * 
 */
 
// Pin Constants defined here
const char *outputIdentifier[7];
const uint8_t pinID[7] = {0,0,0,0,0,3,6};


// Programme scope variables here
uint8_t pinStateID[7] = {0};
uint8_t pinPwmID[7] = {0,0,0,0,0,255,5};
uint8_t pinOverrideID[7] = {0};
uint8_t pinUpperThresholdID[7] = {0,0,0,0,0,30,0};

uint8_t pirAck = 0;
unsigned long ts = millis();
int lastButtonState = 0;
int currentButtonState = 0;
long lastDebounceTime = 0;
int debounceDelay = 50;


void setup() {
  // Init Serial for debugging and general client side messages
  Serial.begin(9600);
  Serial.println("Starting MQTT client on arduino ...");
  
  // Set MQTT server
  client.setServer("io.adafruit.com", 1883);
  client.setCallback(call_Back);

  /*!SET PINS HERE!*/
  // Set input and outputs
  pinMode(pinID[6], OUTPUT);
  pinMode(pinID[5], OUTPUT);
  
  // Set array of string values
  outputIdentifier[0] = "Heater";
  outputIdentifier[1] = "Solenoid";
  outputIdentifier[2] = "Steamer";
  outputIdentifier[3] = "Exhaust";
  outputIdentifier[4] = "Intake";
  outputIdentifier[5] = "Lighting";
  outputIdentifier[6] = "Buzzer";


  // Connect to network
  if (Ethernet.begin(mac) == 0){
    // Fallback if no DHCP
    Serial.println("Failed to configure ethernet using DHCP");
    Ethernet.begin(mac, ip);
  }
  
  // Delay to ensure everything set
  delay(1500);

  Serial.print("MQTT client is at: ");
  Serial.println(Ethernet.localIP());

}

void loop() {
  //Conect on first instance and then recon if any DC occurs
  if (!client.connected()){
    reconnect();
  }

   // Every 1.5s send data
  if (millis() > ts + 1500){
    ts = millis();
  }
  int buttonVal = check_Buttons();
  client.loop();
}

void call_Back(char* topic, byte* payload, unsigned int messLength){

  // Format received data and topic and print to serial
  String t = String(topic);
  char data[messLength+1];
  for (int i = 0; i < messLength; i++){
    data[i] = payload[i]; 
  }
  data[messLength] = '\0';
  Serial.print("message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.println(data);


  // Update threshold values
  if (t.indexOf("light-threshold") > 0){
    uint8_t id = 5;
    pinUpperThresholdID[id] = atoi(data);
  }


  // use light data
  if (t.indexOf("light-level") > 0){
    uint8_t id = 5;
    int light = atoi(data);
    // Only update if override not on
    if(pinOverrideID[id] != 1){
      if(light < pinUpperThresholdID[id]){
        pinStateID[id] = 1;
      }
      else{
        pinStateID[id] = 0;
      }
        control_Pin(pinStateID[id], &pinPwmID[id], pinID[id]);
    }
  }


  // Turn on buzzer until PIR ACK
  if (t.indexOf("pir-level") > 0){
    if(strcmp(data, "1") == 0 && pirAck == 0){
      // Set off Alarm
      uint8_t id = 6;
      pirAck = 1;
      pinStateID[id] = 1;
      control_Pin(pinStateID[id], &pinPwmID[id], pinID[id]);
    }
    if(strcmp(data, "0") == 0){
      // latch won't function until ACK occurs
      pirAck = 0;
    }
  }

  // Turn off buzzer on ACK receive
  if (t.indexOf("pir-ack") > 0){
    if(strcmp(data, "1") == 0){
      uint8_t id = 6;
      pinStateID[6] = 0;
      control_Pin(pinStateID[id], &pinPwmID[id], pinID[id]);
    }
  }

  // Adjust volume of Buzzer
  if (t.indexOf("pir-pwm") > 0){
    // Convert data to PWM value and set
    uint8_t id = 6;
    int pwmVal = atoi(data);
    update_PWM(&pinStateID[id], &pinPwmID[id], pwmVal, pinID[id]);
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.println("Attempting MQTT Connection...");

    // Connect Publisher to broker
    if (client.connect("CB_MP_Publisher", "cbraines", "7c3e3b474fce4a7bbfdd92230bc273b9")) {
      Serial.println("... connected");

      client.publish("cbraines/f/message-log", "Subscriber connected");

      // Subscribe to relevant feeds
      client.subscribe("cbraines/f/light-level");
      client.subscribe("cbraines/f/light-threshold");
      client.subscribe("cbraines/f/pir-level");
      client.subscribe("cbraines/f/pir-ack");
      client.subscribe("cbraines/f/pir-pwm");

      // Perform setup tasks
      
      /*!PRE PUBLISH ANYTHING NEEDED HERE!*/
      // Publish any startup values
    }
    // Attempt Reconnect
    else {
      Serial.print("Failed, RC = ");
      Serial.print(client.state());
      Serial.println(" trying again in ~5 seconds");

      /*!HARD DELAY USED HERE AS NO NEED PUB OCCURS ON THIS BOARD!*/
      delay(5000);
    }

  }
}

// Function to control whether pin turns on or off
void control_Pin(uint8_t state, uint8_t *currentPwm, uint8_t pin){

    uint8_t pwm;
    // Use state to acertain whether or not to turn pin on (with PWM) or off
    if(state == 0){
      pwm =  0;
    }
    else{
      pwm = *currentPwm;
    }

    // Physically switch pin
    switch(pin){
      case 0:
      case 1:
      case 2:
        digitalWrite(pin, state);
        break;
      case 3:
      case 4:
      case 5:
      case 6:
        analogWrite(pin, pwm);
        break;
    }

}

void update_PWM(uint8_t *pinState, uint8_t *currentPwm, uint8_t pwm, uint8_t pin){
  // If pin is on update the live and stored PWM, Else just update stored
  if(*pinState == 1){
    *currentPwm = pwm;
    analogWrite(pin, pwm);
  }
  else{
    *currentPwm = pwm;
  }
}

int check_Buttons(){
  int buttonVal = analogRead(A0);
  int returnVal;
  
  //convert to 0->7
  if (buttonVal <= 800 && buttonVal >= 709){
    // button 1 pressed
    returnVal = 0;
  }
  else if(buttonVal <= 708 && buttonVal >= 643){
    // button 2 pressed
    returnVal = 1;
  }
  else if(buttonVal <= 642 && buttonVal >= 550){
    // button 3 pressed
    returnVal = 2;
  }
  else if(buttonVal <= 490 && buttonVal >= 350){
    // button 4 pressed
    returnVal = 3;
  }
   else if(buttonVal <= 349 && buttonVal >= 205){
    // button 5 pressed
    returnVal = 4;
  }
  else if(buttonVal <= 204 && buttonVal >= 105){
    // button 6 pressed
    returnVal = 5;
  }
  else if(buttonVal <= 104 && buttonVal >= 30){
    // button 7 pressed
    returnVal = 6;
  }
  else{
    returnVal = -1;
  }


  if(returnVal != lastButtonState){
    lastDebounceTime = millis();
  }

  if((millis() - lastDebounceTime) > debounceDelay){
    if (returnVal != currentButtonState){
      currentButtonState = returnVal;
      if (currentButtonState != -1){
        override_Pin(returnVal);
      }
    }
  }

  lastButtonState = returnVal;
  return returnVal;
}

void override_Pin(uint8_t id){
  if(pinOverrideID[id] == 0){
    pinOverrideID[id] = 1;
    Serial.print("Override set as ON for ");
    Serial.println(outputIdentifier[id]);
    pinStateID[id] = 1;
    control_Pin(pinStateID[id], &pinPwmID[id], pinID[id]);
  }
  else{
    pinOverrideID[id] = 0;
    Serial.print("Override set as OFF for ");
    Serial.println(outputIdentifier[id]);

  }
}

