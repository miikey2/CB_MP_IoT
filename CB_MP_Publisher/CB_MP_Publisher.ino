#include <LiquidCrystal.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <Ethernet2.h>
#include <PubSubClient.h>


// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to.
const int rs = 10, en = 9 , d4 = 5, d5 = 4, d6 = 3, d7 = 7;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// Needs changing for any different boards code is uploaded to.
byte mac[] = {0x90, 0xA2, 0xDA, 0x11, 0x3C, 0x9A};

// Set fallback network config in case of no DHCP server
IPAddress ip(192, 168, 0, 11);
IPAddress gateway(192, 168, 0, 254);
IPAddress subnet(255, 255, 255, 0);

// Create class instances for MQTT
EthernetClient ethernetClient;
PubSubClient client(ethernetClient);

// Pin Constants defined here
const char pirsensor = 2;
const int lightsensor = A2;
const int tempsensor = A3;
const int humidsensor = A4;
const int moisturesensor = A5;

// Programme scope variables declared here
uint8_t pirState = 0;
unsigned int maxLevel = 0;
unsigned int minLevel = 1023;
int light;
int PIR;
int temperature;
int humidity;
int moisture;
float tempval;
int count = 1;
int count2 = 1;
unsigned long ts2 = millis();


  void setup() {
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
    
  // Init Serial for debugging and general client side messages
  Serial.begin(9600);
  Serial.println("Starting MQTT client on arduino ...");

  // Set MQTT server
  client.setServer("brain.engineering", 1883);
  client.setCallback(call_Back);

  /*!SET PINS HERE!*/ 
  // Set input and outputs
  pinMode(pirsensor, INPUT);
  pinMode(lightsensor, INPUT);
  pinMode(tempsensor, INPUT);
  pinMode(humidsensor, INPUT);
  pinMode(moisturesensor, INPUT);


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
  // Connect on first instance and then reconnect if any DC occurs
  if (!client.connected()){
    reconnect();
  }
  
  unsigned long ts = millis();
  // Every 0.2 seconds, read different sensor values to stagger publishing
  while (millis() < (ts + 1000)){
  if ((count == 1) && (millis() < ts + 200)){
    count++;
    readPIR(&pirState);
  }
  else if ((count == 2) && (millis() > ts + 200) && (millis() < ts + 400)){
    count++;
    sendLight(&maxLevel, &minLevel);
  }
  else if ((count == 3) && (millis() > ts + 400) && (millis() < ts + 600)){
    count++;
    sendTemp();
  }
  else if ((count == 4) && (millis() > ts + 600) && (millis() < ts + 800)){
    count++;
    sendHumidity();
  }
  else if ((count == 5) && (millis() > ts + 800) && (millis() < ts + 1000)){
    sendMoisture();
    count = 1;
  }
 }
 

// Every 5 seconds, print new sensor value to LCD
while (millis() > (ts2 + 5000)){
  if (count2 == 1){
    lcd.clear();
    lcd.print("PIR state = ");
    lcd.print(PIR, 1);
    count2++;
    ts2 = millis();
  }
  else if (count2 == 2){
    lcd.clear();
    lcd.print("Light = ");
    lcd.print(light, 1);
    lcd.print("%");
    count2++;
    ts2 = millis();
  }
  else if (count2 == 3){
    lcd.clear();
    lcd.print("Temp = ");
    lcd.print(tempval, 1);
    lcd.print((char)223);
    lcd.print("C");
    count2++;
    ts2 = millis();
  }
  else if (count2 == 4){
    lcd.clear();
    lcd.print("Humidity = ");
    lcd.print(humidity, 1);
    lcd.print("%");
    count2++;
    ts2 = millis();
  }
  else if (count2 == 5){
    lcd.clear();
    lcd.print("Moisture = ");
    lcd.print(moisture, 1);
    lcd.print("%");
    count2 = 1;
    ts2 = millis();
  }
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
    /*!CHANGE BROKER AND FEEDS TO brain.engineering and /f/... !*/
    if (client.connect("CB_MP_Publisher", "cbraines", "Tp:5tF'<5dc_k@;<")) {
      Serial.println("... connected");

      client.publish("cbraines/f/message-log", "Publisher begun");

      /*!ADD FEEDS!*/
      /*!NEED TO KNOW STATES OF ANY CALIBRATION FEEDS!*/
      // Subscribe to relevant feeds
      client.subscribe("/mpearson/f/light-level"); /*!DON'T NEED FURTHER DOWN THE LINES!*/
      client.subscribe("/mpearson/f/temp-level"); 
      client.subscribe("/mpearson/f/humidity-level"); 
      client.subscribe("/mpearson/f/moisture-level"); 
      client.subscribe("/mpearson/f/pir-level"); 
      client.subscribe("/mpearson/f/recalibrate-ldr");

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
  client.publish("mpearson/f/message-log","LDR recalibrated");
}


// Function to read light and send via sendFeed function
void sendLight(uint16_t *maxL, uint16_t *minL){
  if (client.connected()){
    light = analogRead(lightsensor);

    // Remap light value
    light = map(light, *minL, *maxL, 0, 100);
    // Stop value from exceeding upper and lower bounds
    light = constrain(light, 0, 100);
    
    char feed[] = "/mpearson/f/light-level";
    sendData(feed, light);
  }
}

// Function for PIR that Transmits PIR state on toggle of state
void readPIR(uint8_t *state){
  /* PIR holds input of 1 internally for ~8s whenever detecting so no interrupt needed */ 
  PIR = digitalRead(pirsensor);
      
  // tx broker if latch has occured
  if (PIR != *state){
      char feed[] = "/mpearson/f/pir-level";
      sendData(feed, PIR);
      *state = PIR;
  }
}

// Function to read temp and send via sendFeed function
void sendTemp(){
  if (client.connected()){
    
    temperature = analogRead(tempsensor);

    float voltage = ((temperature * 5.0) / 1023.0);
    tempval = ((voltage - 0.5) * 100.0);

    char feed[] = "/mpearson/f/temp-level";
    sendData(feed, tempval);
  }
}

// Function to read humidity and send via sendFeed function
void sendHumidity(){
  if (client.connected()){
    humidity = analogRead(humidsensor);

    humidity = constrain(humidity, 0, 100);
    char feed[] = "/mpearson/f/humidity-level";
    sendData(feed, humidity);
  }
}

// Function to read moisture and send via sendFeed function
void sendMoisture(){
  if (client.connected()){
    moisture = analogRead(moisturesensor);

    moisture = constrain(moisture, 0, 100);
    char feed[] = "/mpearson/f/moisture-level";
    sendData(feed, moisture);
  }
}

// Function to handle all data sending
void sendData(char *feed, int payload){

  // Convert payload to character array
  String payloadStr = String(payload, DEC);
  char payloadBuffer[8];
  payloadStr.toCharArray(payloadBuffer, 9);

  // Publish value
  client.publish(feed, payloadBuffer);
  
}

