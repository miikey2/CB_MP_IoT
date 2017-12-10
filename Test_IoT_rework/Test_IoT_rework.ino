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
 * NOTES FOR SELF!
 * - Structs hold common values and identifiers needed
 * - Structs hold a struct pointer to other symbolic structs
 * - Empty struct used as a pointer copier (May be an easier way with memcpy?)
 * 
 * 
 */


struct OperationPin {
  /*!ADD IDENTIFIER?!*/
  const uint8_t id;
  const uint8_t pin;
  uint8_t state;
  uint8_t pwm;
  uint8_t overrider;
  uint8_t threshold;
  const char feed[21];
  struct OperationPin* symLinkId;
};

// Global Scope Structs        
struct OperationPin Heater    = {1,   8,    0,    0,    0,    20,   "/f/temp-heater-"};
struct OperationPin Solenoid  = {2,   4,    0,    0,    0,    80,   "/f/moisture-"};
struct OperationPin Steamer   = {3,   0,    0,    0,    0,    0,    "/f/humidity-steamer-"};
struct OperationPin Exhaust   = {4,   0,    0,    0,    0,    0,    "/f/humidity-exhaust-"};
struct OperationPin Intake    = {5,   5,    0,    150,  0,    25,   "/f/temp-intake-"};
struct OperationPin Lighting  = {6,   3,    0,    255,  0,    30,   "/f/light-"};
struct OperationPin Buzzer    = {7,   6,    0,    5,    0,    0,    "/f/pir-"};
struct OperationPin Empty     = {0,   0,    0,    0,    0,    0,    ""};

// Global Scope Variables
uint8_t pirAck = 0;
unsigned long ts = millis();
uint8_t lastButtonState = 0;
uint8_t currentButtonState = 0;
long lastDebounceTime = 0;
const uint8_t debounceDelay = 50;

/*
 * ADD BUTTON FOR CLIENT TO SYNC THRESHOLDS, PWM.
 */
  
void setup() {
  // Init Serial for debugging and general client side messages
  Serial.begin(9600);

  /*
  strcpy(Heater.feed, "temp-heater-");
  strcpy(Solenoid.feed, "moisture-");
  strcpy(Steamer.feed, "humidity-steamer-");
  strcpy(Exhaust.feed, "humidity-exhaust-");
  strcpy(Intake.feed, "temp-intake-");
  strcpy(Lighting.feed, "light-");
  strcpy(Buzzer.feed, "pir-");
  */
  // Set pointer references up for symlinks
  Heater.symLinkId = &Intake;
  Solenoid.symLinkId = NULL;
  Steamer.symLinkId = &Exhaust;
  Exhaust.symLinkId = &Steamer;
  Intake.symLinkId = &Heater;
  Lighting.symLinkId = NULL;
  Buzzer.symLinkId = NULL;
    
  Serial.println(F("Starting MQTT client on arduino ..."));
  
  // Set MQTT server
  client.setServer("brain.engineering", 1883);
  client.setCallback(call_Back);

  // Set input and outputs
  pinMode(Heater.pin, OUTPUT);
  pinMode(Intake.pin, OUTPUT);
  pinMode(Lighting.pin, OUTPUT);
  pinMode(Buzzer.pin, OUTPUT);
  pinMode(A0, INPUT);

  // Connect to network
  if (Ethernet.begin(mac) == 0){
    // Fallback if no DHCP
    Serial.println(F("Failed to configure ethernet using DHCP"));
    Ethernet.begin(mac, ip);
  }
  
  // Delay to ensure everything set
  delay(1500);

  Serial.print(F("MQTT client is at: "));
  Serial.println(Ethernet.localIP());
}


void loop() {
  //Conect on first instance and then recon if any DC occurs
  if (!client.connected()){
    reconnect();
  }

  /*!NEEDED ON PUBLISHER?!*/ 
  // Every 1.5s send data
  if (millis() > ts + 1500){
    ts = millis();
  }
//  int buttonVal = check_Buttons();
  client.loop();
}


void reconnect() {
  while (!client.connected()) {
    Serial.println(F("Attempting MQTT Connection..."));

    // Connect Publisher to broker
    if (client.connect("CB_MP_Publisher", "cbraines", "Tp:5tF'<5dc_k@;<")) {
      Serial.println(F("... connected"));

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

      client.subscribe("/f/moisture-level");
      client.subscribe("/f/moisture-threshold");
      client.subscribe("/f/moisture-override");
      
      // Perform setup tasks
      
      /*!PRE PUBLISH ANYTHING NEEDED HERE!*/
      // Publish any startup values
    }
    // Attempt Reconnect
    else {
      Serial.print(F("Failed, RC = "));
      Serial.print(client.state());
      Serial.println(F(" trying again in ~5 seconds"));

      /*!HARD DELAY USED HERE AS NO NEED PUB OCCURS ON THIS BOARD!*/
      delay(5000);
    }

  }
}

void call_Back(char* topic, byte* payload, unsigned int messLength){

  
 
  // Format received data and topic and print to serial
  String t = String(topic);
  char data[messLength+1];
  for (int i = 0; i < messLength; i++){
    data[i] = payload[i]; 
  }
  data[messLength] = '\0';
  Serial.print(F("message arrived ["));
  Serial.print(topic);
  Serial.print(F("] "));
  Serial.println(data);

  //Sub-sorting running to prevent needless checks

  /*
  * Temperature processing START
  * Heater and intake are symboliclly linked so must always be
  * Heater threshold < Intake Threshold 
  */
  
  if(t.indexOf("temp") > 0){
    /*!NEED TO FULLY TEST ALL IS WORKING PROPERLY!*/
    // Store new heating threshold
    if (t.indexOf("temp-heater-threshold") > 0){
      Heater.threshold = atoi(data);
  
      // Ensure Heating thresh < Intake
      if(Heater.threshold >= Intake.threshold){
        Intake.threshold = Heater.threshold + 2;
        
        // Update client feed
        char feed[] = "/f/temp-intake-threshold";
        send_Data(feed, Intake.threshold);
      }
    }
  
    // Store new intake threshold
    if (t.indexOf("temp-intake-threshold") > 0){
      Intake.threshold = atoi(data);
  
      // Ensure Heating thresh > Intake
      if(Intake.threshold <= Heater.threshold){
        Heater.threshold = Intake.threshold - 2;
        
        // Update client feed
        char feed[] = "/f/temp-heater-threshold";
        send_Data(feed, Heater.threshold);
      }
    }
  
    // Updates current Intake Fan speed
    if (t.indexOf("temp-intake-pwm") > 0){
      uint8_t pwm = atoi(data);
      update_Pwm(&Intake, pwm);
    }
  
    // Applies/removes override for intake
    if (t.indexOf("temp-intake-override") > 0){
      if(Heater.overrider != 1 || strcmp(data, "OFF") != 0){
        override_Pin(&Intake);
      }
    }
  
    // Applies/removes override for heater
    if (t.indexOf("temp-heater-override") > 0){
      if(Intake.overrider != 1 || strcmp(data, "OFF") != 0){
        override_Pin(&Heater);
      }
    }
  
    // Auto switches between Heater and intake as long as no override on
    if (t.indexOf("temp-level") > 0){
      uint8_t temp = atoi(data);
  
      if(Heater.overrider != 1 && Intake.overrider != 1){
        if(temp <= Heater.threshold){
          Heater.state = 1;
          control_Pin(&Heater);
          Intake.state = 0;
          control_Pin(&Intake);
        }
        else if(temp >= Intake.threshold){
          Intake.state = 1;
          control_Pin(&Intake);
          Heater.state = 0;
          control_Pin(&Heater);
        }
        else{
          Intake.state = 0;
          control_Pin(&Intake);
          Heater.state = 0;
          control_Pin(&Heater);
        }
      }
    }
  }
  
  /*
   * Temperature processing END
   */
  

  /*
   * Light processing START
   */
   
  if (t.indexOf("light") > 0){
    // Adjust PWM
    if (t.indexOf("light-pwm") > 0){
      uint8_t pwm = atoi(data);
      update_Pwm(&Lighting, pwm);
    }

    // Set Lighting Threshold
    if (t.indexOf("light-threshold") > 0){
      Lighting.threshold = atoi(data);
    }

    // Override Lighting Auto
    if (t.indexOf("light-override") > 0){
      override_Pin(&Lighting);
    }

    // Auto control lighting
    if (t.indexOf("light-level") > 0){
      uint8_t light = atoi(data);
      if(Lighting.overrider != 1){
        if(light < Lighting.threshold){
          Lighting.state = 1;
        }
        else{
          Lighting.state = 0;
        }
        control_Pin(&Lighting);
      }
    }
  }
  
  /*
   * Light processing END
   */


  /*
   * Moisture processing START
   */

  if(t.indexOf("moisture") > 0){

    if(t.indexOf("moisture-threshold") > 0){
      Solenoid.threshold = atoi(data);
    }

    if(t.indexOf("moisture-override") > 0){
      override_Pin(&Solenoid);
    }

    if(t.indexOf("moisture-level") > 0){
      uint8_t moisture = atoi(data);
      if(Solenoid.overrider != 1){
        if(moisture <= Solenoid.threshold){
          Solenoid.state = 1;
        }
        else{
          Solenoid.state = 0;
        }
        control_Pin(&Solenoid);
      }
    }

  }

  /*
   * Moisture processing END
   */


  /*
   * PIR/Buzzer processing START
   */

  if (t.indexOf("pir") > 0){
    if (t.indexOf("pir-override") > 0){
      
    }
  
    if (t.indexOf("pir-level") > 0){
      
    }
  
    if (t.indexOf("pir-ack") > 0){
      
    }
  
    if (t.indexOf("pir-pwm") > 0){
      
    }  
  }
  
  /*
   * PIR/Buzzer processing END
   */
  
}

void control_Pin(struct OperationPin *Holder){

  uint8_t pwm;
  if(Holder->state == 0){
    pwm = 0;
  }
  else{
    pwm = Holder->pwm;
  }

  switch(Holder->id){
    case 0:
    case 1:
      case 2:
        digitalWrite(Holder->pin, Holder->state);
        break;
      case 3:
      case 4:
      case 5:
      case 6:
        analogWrite(Holder->pin, pwm);
        break;
  }
}

void update_Pwm(struct OperationPin *Holder, uint8_t pwm){
  // If pin is on, update the live and stored PWM, else just stored
  if(Holder->state == 1){
    Holder->pwm = pwm;
    analogWrite(Holder->pin, Holder->pwm);
  }
  else{
    Holder->pwm;
  }
}

void override_Pin(struct OperationPin *Holder){
  
  //If symlink present then update main and disable/turn off sym
  if(Holder->symLinkId != NULL && Holder->overrider == 0){
    Empty.symLinkId = Holder->symLinkId;
    Holder->overrider = 1;
    Holder->state = 1;
    update_Symbolic_Override(Empty.symLinkId);
    control_Pin(Holder);
    control_Pin(Empty.symLinkId);
  }
  else if(Holder->symLinkId == NULL && Holder->overrider == 0){
    Holder->overrider = 1;
    Holder->state = 1;
    control_Pin(Holder);
  }
  else{
    Holder->overrider = 0;
    Holder->state = 0;
    control_Pin(Holder);
  }
}

void update_Symbolic_Override(struct OperationPin *Holder){
  // Set state as 0 & ensure overrider is 0 to prevent both overrides at once
  Holder->state = 0;
  Holder->overrider = 0;
  
  //Copy struct feed to be used in send_Data
  char passFeed[30] = {};
  strcpy(passFeed, Holder->feed);
  strcat(passFeed, "override");
  send_Data(passFeed, 0);
}

void send_Data(char *feed, int payload){

  // Send overrider feed back
  if(strstr(feed, "override") != NULL){
    client.publish(feed, "OFF");
  }
  // Send standard payload
  else{
    // Convert payload to character array
    String payloadStr = String(payload, DEC);
    char payloadBuffer[8];
    payloadStr.toCharArray(payloadBuffer, 9);
  
    // Publish value
    client.publish(feed, payloadBuffer);
  }
   
}


