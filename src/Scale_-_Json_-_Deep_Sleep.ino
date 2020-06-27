
#include "HX711.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"

//#include "SSD1306.h" // alias for `#include "SSD1306Wire.h"'
//#include <ArduinoJson.h>

//===============
//    Scale
//===============

#define calibration_factor -32690.0 //This value is obtained using the SparkFun_HX711_Calibration sketch

#define DOUT  13
#define CLK  12

float mensagem ;

HX711 scale;

//==================
//    Deep Sleep
//==================

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  600        /* Time ESP32 will go to sleep (in seconds) */

//==================
//    MQTT Broker
//==================

const char* mqttServer = "192.168.1.71";
const int mqttPort = 1883;
const char* mqttUser = MQTT_USER;
const char* mqttPassword = MQTT_PASSWORD;
const char* MQTT_SENSOR_TOPIC = "home/gasscale"; //Topic

//Set intervals
unsigned long previousMillis = 0;
const long interval = 30000;


//==================
//      WIFI
//==================

WiFiClient espClient;
PubSubClient client(espClient);
//WiFi.setHostname("gasmeasure")

// Initialize the OLED display using Wire library
//SSD1306 display(0x3c, 5, 4);



//===========SETUP=============================================

void setup() {

  Serial.begin(115200);
  // Set WiFi to station mode and disconnect from an AP if it was previously connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  Serial.println("Setup done");

  //==================
  //      DISPLAY
  //==================

  display.init();
  //display.flipScreenVertically();
  display.setColor(WHITE);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  Serial.begin(9600);

  //Wifi Connection
  WiFi.begin("Vodafone-C5F922-EXT", WIFI_PASSWORD );  //Replace with your credentials
  Serial.println("Connecting to WiFi..");

  while (WiFi.status() != WL_CONNECTED) {

    Serial.print(".");
    display.drawString(64, 20, String("Connecting to WiFi"));
    display.setFont(ArialMT_Plain_10);
    display.display();
    delay(1000);
    display.clear();

  }

  display.setFont(ArialMT_Plain_10);
  Serial.println("Connected to the WiFi");
  display.drawString(64, 20, String("Connected to WiFi"));
  display.display();
  delay(1000);
  display.clear();

  //===============
  //    Scale
  //===============

  scale.begin(DOUT, CLK);
  scale.set_scale(calibration_factor); //This value is obtained by using the SparkFun_HX711_Calibration sketch
  //scale.tare(); //Assuming there is no weight on the scale at start up, reset the scale to 0

  display.setFont(ArialMT_Plain_16);
  display.drawString(64, 20, String("Starting Readings"));
  //display.drawString(String(sensorValue));
  display.display();
  delay(500);
  display.clear();


  //===============
  //    BROKER
  //===============

  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);

  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
    display.drawString(64, 20, String("Connecting to broker..."));
    display.display();
    delay(1000);
    display.clear();

    if (client.connect("GasMeasure", mqttUser, mqttPassword )) {

      Serial.println("connected");
      display.drawString(64, 20, String("Connected"));
      display.display();
      delay(1000);
      display.clear();

    } else {

      Serial.print("failed with state ");
      Serial.print(client.state());
      display.drawString(64, 20, String("Failed"));
      display.display();
      delay(2000);
      display.clear();

    }
  }

  client.publish("home/gasscale", "Hello from GasMeasure");
  client.subscribe("home/gasscale");

}

//===========CALLBACK=============================================

void callback(char* topic, byte* payload, unsigned int length) {

  Serial.print("Message arrived in topic: ");
  Serial.println(topic);

  Serial.print("Message:");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }

  Serial.println();
  Serial.println("-----------------------");

}


//=========== LOOP ===================================

void loop() {

  while (WiFi.status() != WL_CONNECTED) {

    Serial.print(".");
    display.drawString(64, 20, String("Connecting to WiFi"));
    display.setFont(ArialMT_Plain_10);
    display.display();
    delay(1000);
    display.clear();

  }

  while (!client.connected()) {

    if (client.connect("GasMeasure", mqttUser, mqttPassword )) {

      Serial.println("connected");
      display.drawString(64, 20, String("Connected"));
      display.display();
      delay(1000);
      display.clear();

    } else {

      Serial.print("failed with state ");
      Serial.print(client.state());
      display.drawString(64, 20, String("Failed"));
      display.display();
      delay(2000);
      display.clear();
    }
  }



  display.clear();
  Serial.print("Reading: ");
  Serial.print(scale.get_units(), 1); //scale.get_units() returns a float
  Serial.print(" kg"); //You can change this to kg but you'll need to refactor the calibration_factor
  mensagem = scale.get_units();
  Serial.println();

  //Display the weight in the OLED

  display.drawString(64, 20, String(mensagem));
  display.setFont(ArialMT_Plain_24);
  display.display();
  delay(10);

  unsigned long currentMillis = millis();

  //Send value to the Broker

  if (currentMillis - previousMillis >= interval) {

    // save the last time we send a message
    previousMillis = currentMillis;

    //Send the message to the Broker
    char msgString[8];
    dtostrf(mensagem, 5, 2, msgString);

    StaticJsonBuffer<300> JSONbuffer;
    JsonObject& JSONencoder = JSONbuffer.createObject();
   
    JSONencoder["value"] = msgString;
    JSONencoder["time"] = "Temperature";
    JsonArray& values = JSONencoder.createNestedArray("values");
   
    values.add(20);
    values.add(21);
    values.add(23);
   
    char JSONmessageBuffer[100];
    JSONencoder.printTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));
    
    client.subscribe(MQTT_SENSOR_TOPIC);
    client.publish(MQTT_SENSOR_TOPIC, JSONmessageBuffer);
    Serial.println("Mensagem enviada com sucesso...");
    Serial.print("Weight: ");
    Serial.println(JSONmessageBuffer);
    Serial.println("==================================");
    Serial.println();

    // Go to Deep Sleep Mode
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Seconds");
    esp_deep_sleep_start();

  }

}
