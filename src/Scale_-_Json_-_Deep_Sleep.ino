
#include "HX711.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"

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
#define TIME_TO_SLEEP  3200        /* Time ESP32 will go to sleep (in seconds) */

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

//===========SETUP=============================================

void setup() {

  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);


  //Wifi Connection
  WiFi.begin("Vodafone-C5F922-EXT", WIFI_PASSWORD );
  Serial.println("Connecting to WiFi..");

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("Connected to the WiFi");

  delay(1000);

  //===============
  //    Scale
  //===============

  scale.begin(DOUT, CLK);
  scale.set_scale(calibration_factor);
  delay(500);



  //===============
  //    BROKER
  //===============

  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);

  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
    delay(1000);

    if (client.connect("GasMeasure", mqttUser, mqttPassword )) {
      Serial.println("connected");
      delay(1000);
    } else {
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(2000);
    }
  }

  client.publish("home/gasscale", "Hello from GasMeasure");
  client.subscribe("home/gasscale");
  Serial.println("Setup done");
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
    delay(1000);
  }

  while (!client.connected()) {
    if (client.connect("GasMeasure", mqttUser, mqttPassword )) {
      Serial.println("connected");
      delay(1000);
    } else {
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(2000);
    }
  }

  Serial.print("Reading: ");
  Serial.print(scale.get_units(), 1); //scale.get_units() returns a float
  Serial.print(" kg"); //You can change this to kg but you'll need to refactor the calibration_factor
  //mensagem = scale.get_units();
  Serial.println();
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    char msgString[8];
    dtostrf(mensagem, 5, 2, msgString);

    // save the last time we send a message
    previousMillis = currentMillis;

    //Send the message to the Broker
    sprintf(msgString, "%ld", scale.get_units());
    client.subscribe(MQTT_SENSOR_TOPIC);
    client.publish(MQTT_SENSOR_TOPIC, msgString);
    Serial.println("Mensagem enviada com sucesso...");
    delay(1000);

    // Go to Deep Sleep Mode
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Seconds");
    esp_deep_sleep_start();

  }

}
