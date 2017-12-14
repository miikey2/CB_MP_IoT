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
 * 
 * Things to ADD:
 * - Fix synchronisation between override buttons so they update the client buttons
 */


/*
 * Pins are as follows
 * 
 * Heater     - I/O(D8)               - ID = 0
 * Solenoid   - I/O(!ADD PIN HERE!)   - ID = 1
 * Steamer    - I/O(!ADD PIN HERE!)   - ID = 2
 * Exhaust    - PWM(!ADD PIN HERE!)   - ID = 3
 * Intake     - PWM(D9)               - ID = 4
 * Lighting   - PWM(D3)               - ID = 5
 * Buzzer     - PWM(D6)               - ID = 6
 * 
 * 
 */


/*!USE STRUCTURES INSTEAD OF ARRAYS?!*/
 
// Pin Constants defined here
const char *outputIdentifier[7];
const uint8_t pinID[7] = {8,0,0,0,5,3,6};


// Programme scope variables here
uint8_t pinStateID[7] = {0};
uint8_t pinPwmID[7] = {0,0,0,0,100,255,5};
uint8_t pinOverrideID[7] = {0};
uint8_t pinUpperThresholdID[7] = {20,0,0,0,25,30,0};


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
  client.setServer("brain.engineering", 1883);
  client.setCallback(call_Back);

  /*!SET PINS HERE!*/
  // Set input and outputs
  pinMode(pinID[6], OUTPUT);
  pinMode(pinID[5], OUTPUT);
  pinMode(pinID[4], OUTPUT);
  pinMode(pinID[0], OUTPUT);
  pinMode(A0, INPUT);
  
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

  /*
   * Temperature processing START
   * Heater and intake are symboliclly linked so must always be
   * Heater threshold < Intake Threshold 
   */

  // Store new heating threshold
  if (t.indexOf("temp-heater-threshold") > 0){
    uint8_t id = 0;
    uint8_t otherId = 4;
    pinUpperThresholdID[id] = atoi(data);

    // update Intake threshold if Heater changed
    if(pinUpperThresholdID[id] >= pinUpperThresholdID[otherId]){
      pinUpperThresholdID[otherId] = pinUpperThresholdID[id] + 2;
      char feed[] = "/f/temp-intake-threshold";
      sendData(feed, pinUpperThresholdID[otherId]);
    }
  }

  // Store new intake fan threshold
  if (t.indexOf("temp-intake-threshold") > 0){
    uint8_t id = 4;
    uint8_t otherId = 0;
    pinUpperThresholdID[id] = atoi(data);

    // update Heater threshold if Intake changed
    if(pinUpperThresholdID[id] <= pinUpperThresholdID[otherId]){
      pinUpperThresholdID[otherId] = pinUpperThresholdID[id] - 2;
      char feed[] = "/f/temp-heater-threshold";
      sendData(feed, pinUpperThresholdID[otherId]);
    }
  }


  // Intake PWM
  if (t.indexOf("temp-intake-pwm") > 0){
    uint8_t id = 4;
    int pwmVal = atoi(data);
    update_PWM(&pinStateID[id], &pinPwmID[id], pwmVal, pinID[id]);
  }

  // Intake Override
  if (t.indexOf("temp-intake-override") > 0){
    uint8_t id = 4;
    override_Pin(id);
  }

  // Heating Override
  if (t.indexOf("temp-heater-override") > 0){
    uint8_t id = 0;
    override_Pin(id);
  }

  // Temperature Level
  if (t.indexOf("temp-level") > 0){
    uint8_t id = 4;
    uint8_t otherId = 0;
    int temp = atoi(data);
    
    // Only update if override not on
    if(pinOverrideID[id] != 1 && pinOverrideID[otherId] != 1){
      if(temp < pinUpperThresholdID[otherId]){
        pinStateID[otherId] = 1;
        control_Pin(pinStateID[otherId], &pinPwmID[otherId], otherId);
        pinStateID[id] = 0;
        control_Pin(pinStateID[id], &pinPwmID[id], id);
      }
      else if(temp > pinUpperThresholdID[id]){
        pinStateID[id] = 1;
        control_Pin(pinStateID[id], &pinPwmID[id], id);
        pinStateID[otherId] = 0;
        control_Pin(pinStateID[otherId], &pinPwmID[otherId], otherId);
      }
      else{
        pinStateID[id] = 0;
        control_Pin(pinStateID[id], &pinPwmID[id], id);
        pinStateID[otherId] = 0;
        control_Pin(pinStateID[otherId], &pinPwmID[otherId], otherId);
      }
    }
  }  

  /*
   * Temperature processing END
   */


  /*
   * Light processing START
   */

  // Adjust Brightness of lights
  if (t.indexOf("light-pwm") > 0){
    // Convert data to PWM value and set
    uint8_t id = 5;
    int pwmVal = atoi(data);
    update_PWM(&pinStateID[id], &pinPwmID[id], pwmVal, pinID[id]);
  }

  // Update threshold values
  if (t.indexOf("light-threshold") > 0){
    uint8_t id = 5;
    pinUpperThresholdID[id] = atoi(data);
  }

  // Apply Light Override originating from client UI
  if (t.indexOf("light-override") > 0){
    uint8_t id = 5;
    override_Pin(id);
  }

  // Use light data
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
        control_Pin(pinStateID[id], &pinPwmID[id], id);
    }
  }
  
  /*
   * Light processing END
   */

  /*
   * PIR/Buzzer processing START
   */

  // Override PIR sensor/Buzzer output
  if (t.indexOf("pir-override") > 0){
    uint8_t id = 6;
    override_Pin(id);
  }
   
  // Turn on buzzer until PIR ACK
  if (t.indexOf("pir-level") > 0){
    if(strcmp(data, "1") == 0 && pirAck == 0){
      
      uint8_t id = 6;
      if (pinOverrideID[id] != 1){
        pirAck = 1;
        pinStateID[id] = 1;
        control_Pin(pinStateID[id], &pinPwmID[id], id);
      }
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
      control_Pin(pinStateID[id], &pinPwmID[id], id);
    }
  }

  // Adjust volume of Buzzer
  if (t.indexOf("pir-pwm") > 0){
    // Convert data to PWM value and set
    uint8_t id = 6;
    int pwmVal = atoi(data);
    update_PWM(&pinStateID[id], &pinPwmID[id], pwmVal, pinID[id]);
  }

  /*
   * PIR/Buzzer processing END
   */
  
}


void reconnect() {
  while (!client.connected()) {
    Serial.println("Attempting MQTT Connection...");

    // Connect Publisher to broker
    if (client.connect("CB_MP_Publisher", "cbraines", "Tp:5tF'<5dc_k@;<")) {
      Serial.println("... connected");

      client.publish("/f/message-log", "Subscriber connected");

      // Subscribe to relevant feeds
      client.subscribe("/f/light-level");
      client.subscribe("/f/light-threshold");
      client.subscribe("/f/light-override");
      client.subscribe("/f/light-pwm");
      
      client.subscribe("/f/pir-level");
      client.subscribe("/f/pir-override");
      client.subscribe("/f/pir-ack");
      client.subscribe("/f/pir-pwm");
      
      client.subscribe("/f/temp-level");
      client.subscribe("/f/temp-heater-threshold");
      client.subscribe("/f/temp-intake-threshold");
      client.subscribe("/f/temp-heater-override");
      client.subscribe("/f/temp-intake-override");
      client.subscribe("/f/temp-intake-pwm");

      
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
void control_Pin(uint8_t state, uint8_t *currentPwm, uint8_t id){

   
    uint8_t pwm;
    // Use state to acertain whether or not to turn pin on (with PWM) or off
    if(state == 0){
      pwm =  0;
    }
    else{
      pwm = *currentPwm;
    }
    uint8_t pin = pinID[id];
    
    // Physically switch pin
    switch(id){
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

    // PIR override prevents output, everything else forces output
    if (id == 6){
      control_Pin(0, &pinPwmID[id], id);
    }
    else{
      pinStateID[id] = 1;
      control_Pin(pinStateID[id], &pinPwmID[id], id);
    }
  }
  else{
    // Everything minus buzzer is sampled fast enough that setting to prior state is not required
    pinOverrideID[id] = 0;
    Serial.print("Override set as OFF for ");
    Serial.println(outputIdentifier[id]);
    // Set alarm back off if override pressed while buzzer occuring
    if (id == 6){
      control_Pin(pinStateID[id], &pinPwmID[id], id);
    }
  }
}

// function to handle all data sending
void sendData(char *feed, int payload){

  // Convert payload to character array
  String payloadStr = String(payload, DEC);
  char payloadBuffer[8];
  payloadStr.toCharArray(payloadBuffer, 9);

  // Publish value
  client.publish(feed, payloadBuffer);
  
}
