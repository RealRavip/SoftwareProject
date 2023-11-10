#include <Adafruit_Fingerprint.h>
#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <HTTPClient.h>
#define NULL (void *)0
#include <PubSubClient.h>

//#define WIFI_STA_NAME "Room806IoT"
//#define WIFI_STA_PASS "15205094"
const char* ssid = "Room806IoT";
const char* password = "15205094";
#define mqtt_server "test.mosquitto.org"

WiFiClient Room806FingerprintReader;
PubSubClient client(Room806FingerprintReader);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE  (2048)
char msg[MSG_BUFFER_SIZE];
int value = 0;
String Sensor_id = "1234";

#define LED 2
int buzzer = 26;


#if (defined(__AVR__) || defined(ESP8266)) && !defined(__AVR_ATmega2560__)
// For UNO and others without hardware serial, we must use software serial...
// pin #2 is IN from sensor (GREEN wire)
// pin #3 is OUT from arduino  (WHITE wire)
// Set up the serial port to use softwareserial..
SoftwareSerial mySerial(2, 3);

#else
// On Leonardo/M0/etc, others with hardware serial, use hardware serial!
// #0 is green wire, #1 is white
#define mySerial Serial2

#endif


Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);
LiquidCrystal_I2C lcd(0x27, 16, 2);

uint8_t id;


void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  lcd.setCursor(0,0);
  lcd.print("Connecting to");
  lcd.setCursor(0,1);
  lcd.print("Wifi");
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());
  lcd.clear();
  lcd.print("Wifi connected");
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  if (strcmp(topic,"esp/lcd/output")==0){
    StaticJsonDocument<96> doc;
    
    DeserializationError error = deserializeJson(doc, payload, length);

    if (error) {
      Serial.print("Failed to parse JSON: ");
      Serial.println(error.c_str());
      return;
    }
    
    const char* first = doc["first"];
    const char* second = doc["second"]; // Extracting the first sensor ID

    String first_String = String(first);
    String second_String = String(second);
     
    if ( first_String == "Access Granted" ){
      Serial.print("Access Granted");
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Access Granted");
      lcd.setCursor(0,1);
      lcd.print(second_String);
      digitalWrite(buzzer, LOW);
      delay(300);
      digitalWrite(buzzer, HIGH);
      delay(5000);
    }
    if (first_String == "Access Denied") {
      Serial.print("Access Denied");
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Access Denied");
      lcd.setCursor(0,1);
      lcd.print(second_String);
      delay(3000);      
    }
  }
  if (strcmp(topic,"esp32/output")==0){
    uint8_t templete[2400];
    uint8_t Sensors_id[100];
    bool exist = false;
    // Parse the received JSON message
    StaticJsonDocument<2400> doc;
    DeserializationError error = deserializeJson(doc, payload, length);

    if (error) {
      Serial.print("Failed to parse JSON: ");
      Serial.println(error.c_str());
      return;
    }
    {
    const char* action = doc["action"];
    int key_id = doc["key_id"];
    const char* sensor_id = doc["sensor_ids"][0]; // Extracting the first sensor ID
    int sync_id = doc["sync_id"];
    String action_String = String(action);
    int key_id_int = String(key_id).toInt();
    JsonArray payload1 = doc["payload"];
    for (int i = 0; i < payload1.size(); i++) {
      templete[i] = payload1[i];
    }

    JsonArray sensor_ids = doc["sensor_ids"];
    for (int j = 0; j < sensor_ids.size(); j++) {
      if(sensor_ids[j] == Sensor_id){
        exist = true;
      }
    }

    if( exist == true){
      if(action_String == "UPDATE_FINGERPRINT"){
        write_template_data_to_sensor(templete, key_id);
        HTTP_Sync(sync_id);
      }
      if(action_String == "DELETE_FINGERPRINT"){
        deleteFingerprint(key_id);
        HTTP_Sync(sync_id);
      }
    }
    }    
  }

}


void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Connecting to");
    lcd.setCursor(0,1);
    lcd.print("MQTT");
    // Attempt to connect
    // If you do not want to use a username and password, change next line to
    if (client.connect("FingerReader")) {
    //if (client.connect("Client", mqtt_user, mqtt_password)) {
      client.subscribe("esp32/output");
      client.subscribe("esp/lcd/output");
      Serial.println("connected");
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("MQTT connected");
    } else {
      Serial.print("failed, rc=");
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("MQTT connecting");
      lcd.setCursor(0,1);
      lcd.print("failed");  
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup()
{
  Serial.begin(9600);
  while (!Serial);  
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Init...");

  Serial.println("\n\nAdafruit finger detect test");
  pinMode(buzzer, OUTPUT);
  digitalWrite(buzzer, HIGH);

  // set the data rate for the sensor serial port
  finger.begin(57600);
  delay(5);
  if (finger.verifyPassword()) {
    Serial.println("Found fingerprint sensor!");
    lcd.clear();
    lcd.print("Found ");
    lcd.setCursor(0,1);
    lcd.print("fingerprint sensor!");
  } else {
    Serial.println("Did not find fingerprint sensor :(");
    lcd.clear();
    lcd.print("Did not find ");
    lcd.setCursor(0,1);
    lcd.print("fingerprint sensor :(");
    while (1) { delay(1); }
  }

//  Serial.println(F("Reading sensor parameters"));
//  finger.getParameters();
//  Serial.print(F("Status: 0x")); Serial.println(finger.status_reg, HEX);
//  Serial.print(F("Sys ID: 0x")); Serial.println(finger.system_id, HEX);
//  Serial.print(F("Capacity: ")); Serial.println(finger.capacity);
//  Serial.print(F("Security level: ")); Serial.println(finger.security_level);
//  Serial.print(F("Device address: ")); Serial.println(finger.device_addr, HEX);
//  Serial.print(F("Packet len: ")); Serial.println(finger.packet_len);
//  Serial.print(F("Baud rate: ")); Serial.println(finger.baud_rate);
  
  finger.getTemplateCount();
  lcd.clear();
  if (finger.templateCount == 0) {
    Serial.print("Sensor doesn't contain any fingerprint data. Please run the 'enroll' example.");
  }
  else {
    Serial.println("Waiting for valid finger...");
      Serial.print("Sensor contains "); Serial.print(finger.templateCount); Serial.println(" templates");
  }
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  client.setBufferSize(1024);
  HTTP_Put_detail();
  

}

void loop()                     // run over and over again
{
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
//  lcd.clear();
//  lcd.setCursor(0,0);
//  lcd.print("Waiting for");
//  lcd.setCursor(0,1);
//  lcd.print("Finger or Card");
//  Serial.println("No finger detected");
  getFingerprintID();
  delay(50); 
}

void write_template_data_to_sensor(uint8_t templete[], int id) {
  int template_buf_size=512; //usually hobby grade sensors have 512 byte template data, watch datasheet to know the info
  
  uint8_t fingerTemplate[512]; //this is where you need to store your template data 

  Serial.print("Writing template against ID #"); Serial.println(id);

  if (finger.write_template_to_sensor(template_buf_size,templete)) { //telling the sensor to download the template data to it's char buffer from upper computer (this microcontroller's "fingerTemplate" buffer)
    Serial.println("now writing to sensor...");
  } else {
    Serial.println("writing to sensor failed");
    return;
  }

  Serial.print("ID "); Serial.println(id);
  if (finger.storeModel(id) == FINGERPRINT_OK) { //saving the template against the ID you entered or manually set
    Serial.print("Successfully stored against ID#");Serial.println(id);
  } else {
    Serial.println("Storing error");
    return ;
  }
}

void show_from_saved(uint16_t id, String msg_id) {
  Serial.println("------------------------------------");
  Serial.print("Attempting to load #"); Serial.println(id);
  uint8_t p = finger.loadModel(id);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.print("Template "); Serial.print(id); Serial.println(" loaded");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return;
    default:
      Serial.print("Unknown error "); Serial.println(p);
      return ;
  }

  // OK success!

  Serial.print("Attempting to get #"); Serial.println(id);
  p = finger.getModel();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.print("Template "); Serial.print(id); Serial.println(" transferring:");
      break;
    default:
      Serial.print("Unknown error "); Serial.println(p);
      return;
  }
  
  uint8_t f_buf[512];
  uint8_t fingerTemplate[512];
  int i = 0;
    if (finger.get_template_buffer(512, f_buf) == FINGERPRINT_OK) { //read the template data from sensor and save it to buffer f_buf
    Serial.println("Template data (comma sperated HEX):");
    for (int k = 0; k < (512/finger.packet_len); k++) { //printing out the template data in seperate rows, where row-length = packet_length
      for (int l = 0; l < finger.packet_len; l++) {
        Serial.print("0x");
        Serial.print(f_buf[(k * finger.packet_len) + l], HEX);
        Serial.print(",");
        fingerTemplate[i] = f_buf[(k * finger.packet_len) + l];
        i++;
      }
      Serial.println("");
    }
  }
  for(int i = 0; i < 512;i++){
    Serial.print( fingerTemplate[i]);
    Serial.print(",");
    
  }
//  HTTP_Post(fingerTemplate, msg_id);
}

void HTTP_Post(int id) {
  String url = "http://homeassistant.local:1880/endpoint/verify-access";
  Serial.println();
  Serial.println("Get content from " + url);
  
  StaticJsonDocument<100> JSONbuffer;
  JSONbuffer["sensorId"] = Sensor_id;
  JSONbuffer["key"] = String(id);
  JSONbuffer["type"] = "FINGERPRINT";



  String requestBody;
  serializeJson(JSONbuffer, requestBody);
  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json"); 
  int httpCode = http.POST(requestBody);
  if (httpCode == 200) {
    String content = http.getString();
    Serial.println("Content ---------");
    Serial.println(content);
    Serial.println("-----------------");
    Serial.println("Unlock");
  } else {
    Serial.println("Fail. error code " + String(httpCode));
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Access Denied");
    Serial.println("Lock");
  }
  Serial.println("END");
}


void HTTP_Put_detail() {
  String url = "http://homeassistant.local:1880/endpoint/sensors";
  Serial.println();
  Serial.println("Get content from " + url);
  {
  StaticJsonDocument<2048> JSONbuffer;
  JSONbuffer["id"] = "1234";
  JSONbuffer["name"] = "806_room_Fingerprint_sensor";
  JSONbuffer["type"] = "FINGERPRINT";
  



  String requestBody;
  serializeJson(JSONbuffer, requestBody);
  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");  
  int httpCode = http.PUT(requestBody);
  if (httpCode == 200) {
    String content = http.getString();
    Serial.println("Content ---------");
    Serial.println(content);
    Serial.println("-----------------");
  } else {
    Serial.println("Fail. error code " + String(httpCode));
  }
  Serial.println("END");
  }
}

void HTTP_Sync( int sync) {
  String url = "http://homeassistant.local:1880/endpoint/fingerprint/ack";
  Serial.println();
  Serial.println("Get content from " + url);
  
  StaticJsonDocument<100> JSONbuffer;
  JSONbuffer["sensorId"] = "1234";
  JSONbuffer["syncId"] = sync;  


  String requestBody;
  serializeJson(JSONbuffer, requestBody);
  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.POST(requestBody);
  if (httpCode == 200) {
    String content = http.getString();
  } else {
    Serial.println("Fail. error code " + String(httpCode));
  }
  Serial.println("END");
}

uint8_t getFingerprintID() {
  uint8_t p = finger.getImage();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Waiting for");
      lcd.setCursor(0,1);
      lcd.print("Finger or Card");
      Serial.println("No finger detected");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK success!
  lcd.clear();
  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK converted!
  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    Serial.println("Found a print match!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_NOTFOUND) {
    Serial.println("Did not find a match");
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Access Denied");
    Serial.println("Lock");
    delay(3000);
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }

  // found a match!
  Serial.print("Found ID #"); Serial.print(finger.fingerID);
  Serial.print(" with confidence of "); Serial.println(finger.confidence);
  HTTP_Post(finger.fingerID);
  return finger.fingerID;
}

// returns -1 if failed, otherwise returns ID #
int getFingerprintIDez() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK)  return -1;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK)  return -1;

  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK)  return -1;

  // found a match!
  Serial.print("Found ID #"); Serial.print(finger.fingerID);
  Serial.print(" with confidence of "); Serial.println(finger.confidence);
  return finger.fingerID;
}

uint8_t deleteFingerprint(uint8_t id) {
  uint8_t p = -1;

  p = finger.deleteModel(id);

  if (p == FINGERPRINT_OK) {
    Serial.println("Deleted!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("Could not delete in that location");
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("Error writing to flash");
  } else {
    Serial.print("Unknown error: 0x"); Serial.println(p, HEX);
  }

  return p;
}
