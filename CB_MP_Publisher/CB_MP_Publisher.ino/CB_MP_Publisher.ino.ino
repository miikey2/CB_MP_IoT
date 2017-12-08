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

// Pin Constants defined here
const char pirPin = 2;

// Programme scope variables declared here
uint8_t pirState = 0;
unsigned int maxLevel = 0;
unsigned int minLevel = 1023;
unsigned long ts = millis();



void setup() {
  // Init Serial for debugging and general client side messages
  Serial.begin(9600);
  Serial.println("Starting MQTT client on arduino ...");
  
  // Set MQTT server
  client.setServer("io.adafruit.com", 1883);
  client.setCallback(call_Back);

  /*!SET PINS HERE!*/
  // Set input and outputs
  pinMode(pirPin, INPUT);

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

  /*!CHANGE TO TIER TAKING DIFFERENT DATA EVERY 0.2 Seconds?!*/
  // Every 1.5s send data
  if (millis() > ts + 1500){
    ts = millis();
    sendLight(&maxLevel, &minLevel);
    readPIR(&pirState);
  }
  
  client.loop();

}

/*!SUBSCRIBE TO OVERRIDE FEEDS TO PREVENT DATA SENDING?!*/
// Function to handle recieving messages from broker
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

  // Check calibration has been pressed
  if (t.indexOf("recalibrate-ldr") > 0){
    if (strcmp(data, "1") == 0){
      calibrate_Ldr(&maxLevel, &minLevel);
    }
  }

}

void reconnect() {
  while (!client.connected()) {
    Serial.println("Attempting MQTT Connection...");

    // Connect Publisher to broker
    if (client.connect("CB_MP_Publisher", "cbraines", "7c3e3b474fce4a7bbfdd92230bc273b9")) {
      Serial.println("... connected");

      client.publish("cbraines/f/message-log", "Publisher begun");

      /*!ADD FEEDS!*/
      /*!NEED TO KNOW STATES OF ANY CALIBRATION FEEDS!*/
      // Subscribe to relevant feeds
      client.subscribe("cbraines/f/light-level");     /*!DON'T NEED FURTHER DOWN THE LINES!*/
      client.subscribe("cbraines/f/recalibrate-ldr");

      // Perform setup tasks
      calibrate_Ldr(&maxLevel, &minLevel);
      
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

// Re-calibration function
void calibrate_Ldr(uint16_t *maxL, uint16_t *minL){
  
  unsigned long timestamp = millis();
  uint8_t i = 1;
  *maxL = 0;
  *minL = 1023;
  Serial.println("Begin calibration now:");
  while (millis() < (timestamp + 5000)){
    if ((i == 1) && (millis() < timestamp + 1000)){
      i++;
      Serial.println("5");
    }
    else if ((i == 2) && (millis() > timestamp + 1000) && (millis() < timestamp + 2000)){
      i++;
      Serial.println("4");
    }
    else if ((i == 3) && (millis() > timestamp + 2000) && (millis() < timestamp + 3000)){
      i++;
      Serial.println("3");
    }
    else if ((i == 4) && (millis() > timestamp + 3000) && (millis() < timestamp + 4000)){
      i++;
      Serial.println("2");
    }
    else if ((i == 5) && (millis() > timestamp + 4000) && (millis() < timestamp + 5000)){
      i++;
      Serial.println("1");
    }
    int light = analogRead(A0);
    if (light > *maxL){
      *maxL= light;
    }
    if (light < *minL){
      *minL = light;
    }
  }
  client.publish("cbraines/f/message-log","LDR recalibrated");
}

// Function to read light and send via sendFeed fnc
void sendLight(uint16_t *maxL, uint16_t *minL){
  if (client.connected()){
    int light = analogRead(A0);

    // Remap light value
    light = map(light, *minL, *maxL, 0, 100);
    // Stop value from exceeding upper and lower bounds
    light = constrain(light, 0, 100);
 
    char feed[] = "cbraines/f/light-level";
    sendData(feed, light);
  }
}

// funciton for PIR that Transmits PIR state on toggle of state
void readPIR(uint8_t *state){
  /* PIR holds input of 1 internally for ~8s whenever detecting so no interrupt needed */
  int pirVal;
  pirVal = digitalRead(pirPin);

  // tx broker if latch has occured
  if (pirVal != *state){
      char feed[] = "cbraines/f/pir-level";
      sendData(feed, pirVal);
      *state = pirVal;
  }
  
}

// function to handle all data sending
void sendData(char* feed, int payload){

  // Convert payload to character array
  String payloadStr = String(payload, DEC);
  char payloadBuffer[8];
  payloadStr.toCharArray(payloadBuffer, 9);

  // Publish value
  client.publish(feed, payloadBuffer);
  
}

