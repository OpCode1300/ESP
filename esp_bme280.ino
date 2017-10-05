#include "config.h"

#ifdef ESP32
  #include <WiFi.h>
#else
  #include <ESP8266WiFi.h>
#endif

#include <Wire.h>
#include <PubSubClient.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>


// In case you have more than one sensor, make each one a different number here
#define humidity_topic "sensor/" sensor_number "/humidity/percentRelative"
#define temperature_c_topic "sensor/" sensor_number "/temperature/degreeCelsius"
#define temperature_f_topic "sensor/" sensor_number "/temperature/degreeFahrenheit"
#define barometer_hpa_topic "sensor/" sensor_number "/barometer/hectoPascal"
#define barometer_inhg_topic "sensor/" sensor_number "/barometer/inchHg"

WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_BME280 bme; // I2C

void setup() {

  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(100000);
  // Setup the two LED ports to use for signaling status
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);

  // Start sensor
  if (!bme.begin()) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }
}


void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(480);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client")) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

bool checkBound(float newValue, float prevValue, float maxDiff) {
  return newValue < prevValue - maxDiff || newValue > prevValue + maxDiff;
}

long lastMsg = 0;
long lastForceMsg = 0;
bool forceMsg = false;
float temp = 0.0;
float hum = 0.0;
float baro = 0.0;
float diff = 1.0;


void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 1000) {
    lastMsg = now;

    // MQTT broker could go away and come back at any time
    // so doing a forced publish to make sure something shows up
    // within the first 5 minutes after a reset
    if (now - lastForceMsg > force_pub) {
      lastForceMsg = now;
      forceMsg = true;
      Serial.println("Forcing update...");
    }

    float newTemp = bme.readTemperature();
    float newHum = bme.readHumidity();
    float newBaro = bme.readPressure() / 100.0F;

    if (checkBound(newTemp, temp, diff) || forceMsg) {
      temp = newTemp;
      float temp_c = temp; // Celsius
      float temp_f = temp * 1.8F + 32.0F; // Fahrenheit
      Serial.print("New temperature:");
      Serial.print(String(temp_c) + " degC   ");
      Serial.println(String(temp_f) + " degF");
      client.publish(temperature_c_topic, String(temp_c).c_str(), true);
      client.publish(temperature_f_topic, String(temp_f).c_str(), true);
    }

    if (checkBound(newHum, hum, diff) || forceMsg) {
      hum = newHum;
      Serial.print("New humidity:");
      Serial.println(String(hum) + " %");
      client.publish(humidity_topic, String(hum).c_str(), true);
    }

    if (checkBound(newBaro, baro, diff) || forceMsg) {
      baro = newBaro;
      float baro_hpa = baro + baro_corr_hpa; // hPa corrected to sea level
      float baro_inhg = baro_hpa / 33.8639F; // inHg corrected to sea level
      Serial.print("New barometer:");
      Serial.print(String(baro_hpa) + " hPa   ");
      Serial.println(String(baro_inhg) + " inHg");
      client.publish(barometer_hpa_topic, String(baro_hpa).c_str(), true);
      client.publish(barometer_inhg_topic, String(baro_inhg).c_str(), true);
    }

    forceMsg = false;
  }
}
