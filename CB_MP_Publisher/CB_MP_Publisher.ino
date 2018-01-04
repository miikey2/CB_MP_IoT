#include <SoftwareSerial.h>
#include <LiquidCrystal.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <Ethernet2.h>
#include <PubSubClient.h>


// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to.
const int rs = 7, en = 8 , d4 = 5, d5 = 4, d6 = 3, d7 = 6;
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
const int pirsensor = A4;
const int lightsensor = A2;
const int tempsensor = A3;
const int humidsensor = A0;
const int moisturesensor = A1;

// Programme scope variables declared here
uint8_t pirState = 0;
unsigned int maxLevel = 0;
unsigned int minLevel = 1023;
int light;
int pirVal;
int humidity;
int moisture;
float tempVal;
unsigned long ts3  = millis();
uint8_t count3 = 1;


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
  digitalWrite(pirsensor, LOW);
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
  
  unsigned long ts2 = millis();
  uint8_t count2 = 1;
  // Every 0.2 seconds, read different sensor values to stagger publishing
  while (millis() < (ts2 + 1000)){
  if ((count2 == 1) && (millis() < ts2 + 200)){
    count2++;
    read_PIR(&pirState);
  }
  else if ((count2 == 2) && (millis() > ts2 + 200) && (millis() < ts2 + 400)){
    count2++;
    send_Light(&maxLevel, &minLevel);
  }
  else if ((count2 == 3) && (millis() > ts2 + 400) && (millis() < ts2 + 600)){
    count2++;
    send_Temp();
  }
  else if ((count2 == 4) && (millis() > ts2 + 600) && (millis() < ts2 + 800)){
    count2++;
    send_Humidity();
  }
  else if ((count2 == 5) && (millis() > ts2 + 800) && (millis() < ts2 + 1000)){
    send_Moisture();
    count2 = 1;
  }
 }
 


// Every 5 seconds, print new sensor value to LCD
if (millis() > (ts3 + 2000)){

  switch(count3){
    
  case 1:
    lcd.clear();
    lcd.print("PIR state = ");
    lcd.print(pirVal, 1);
    count3++;
    ts3 = millis();
    break;
    
  case 2:
    lcd.clear();
    lcd.print("Light = ");
    lcd.print(light, 1);
    lcd.print("%");
    count3++;
    ts3 = millis();
    break;
    
  case 3:
    lcd.clear();
    lcd.print("Temp = ");
    lcd.print(tempVal, 1);
    lcd.print((char)223);
    lcd.print("C");
    count3++;
    ts3 = millis();
    break;
    
  case 4:
    lcd.clear();
    lcd.print("Humidity = ");
    lcd.print(humidity, 1);
    lcd.print("%");
    count3++;
    ts3 = millis();
    break;
    
  case 5:
  
    lcd.clear();
    lcd.print("Moisture = ");
    lcd.print(moisture, 1);
    lcd.print("%");
    count3 = 1;
    ts3 = millis();
    break;
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
    if (client.connect("CB_MP_Publisher", "mpearson", "passw0rd")) {
      Serial.println("... connected");

      client.publish("cbraines/f/message-log", "Publisher begun");

      /*!ADD FEEDS!*/
      /*!NEED TO KNOW STATES OF ANY CALIBRATION FEEDS!*/
      // Subscribe to relevant feeds
     // client.subscribe("/f/light-level"); /*!DON'T NEED FURTHER DOWN THE LINES!*/
     // client.subscribe("/f/temp-level"); 
     // client.subscribe("/f/humidity-level"); 
     // client.subscribe("/f/moisture-level"); 
     // client.subscribe("/f/pir-level"); 
     // client.subscribe("/f/recalibrate-ldr");

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
  
  unsigned long ts = millis();
  uint8_t count = 1;
  *maxL = 0;
  *minL = 1023;
  Serial.println("Begin calibration now:");
  while (millis() < (ts + 5000)){
    if ((count == 1) && (millis() < ts + 1000)){
      count++;
      Serial.println("5");
    }
    else if ((count == 2) && (millis() > ts + 1000) && (millis() < ts + 2000)){
      count++;
      Serial.println("4");
    }
    else if ((count == 3) && (millis() > ts + 2000) && (millis() < ts + 3000)){
      count++;
      Serial.println("3");
    }
    else if ((count == 4) && (millis() > ts + 3000) && (millis() < ts + 4000)){
      count++;
      Serial.println("2");
    }
    else if ((count == 5) && (millis() > ts + 4000) && (millis() < ts + 5000)){
      count++;
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
  client.publish("/f/message-log","LDR recalibrated");
}


// Function to read light and send via sendFeed function
void send_Light(uint16_t *maxL, uint16_t *minL){
  if (client.connected()){
    light = analogRead(lightsensor);

    // Remap light value
    light = map(light, *minL, *maxL, 0, 100);
    // Stop value from exceeding upper and lower bounds
    light = constrain(light, 0, 100);
    
    char feed[] = "/f/light-level";
    send_Data(feed, light);
  }
}

// Function for PIR that Transmits PIR state on toggle of state
void read_PIR(uint8_t *state){
  /* PIR holds input of 1 internally for ~8s whenever detecting so no interrupt needed */ 
  pirVal = digitalRead(pirsensor);

  // tx broker if latch has occured
  if (pirVal != *state){
      char feed[] = "/f/pir-level";
      send_Data(feed, pirVal);
      *state = pirVal;
  }
}

// Function to read temp and send via sendFeed function
void send_Temp(){
  if (client.connected()){
    
    int temperature = analogRead(tempsensor);

    float voltage = ((temperature * 5.0) / 1023.0);
    tempVal = ((voltage - 0.5) * 100.0);

    char feed[] = "/f/temp-level";
    send_Data(feed, tempVal);
  }
}

// Function to read humidity and send via sendFeed function
void send_Humidity(){
  if (client.connected()){
    humidity = analogRead(humidsensor);

    humidity = map(humidity, 0, 1023, 0, 100);
    char feed[] = "/f/humidity-level";
    send_Data(feed, humidity);
  }
}

// Function to read moisture and send via sendFeed function
void send_Moisture(){
  if (client.connected()){
    moisture = analogRead(moisturesensor);

    moisture = map(moisture, 0, 1023, 0, 100);
    char feed[] = "/f/moisture-level";
    send_Data(feed, moisture);
  }
}

// Function to handle all data sending
void send_Data(char *feed, int payload){

  // Convert payload to character array
  String payloadStr = String(payload, DEC);
  char payloadBuffer[8];
  payloadStr.toCharArray(payloadBuffer, 9);

  // Publish value
  client.publish(feed, payloadBuffer);
  
}

