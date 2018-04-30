#include <Wire.h>
#include <PubSubClient.h>
#include <Adafruit_Sensor.h>
#include "config.h"


#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif

WiFiClient espClient;
PubSubClient client(espClient);

#ifdef BME280
#include <Adafruit_BME280.h>
Adafruit_BME280 bme;
// In case you have more than one sensor, make each one a different number here
#define humidity_topic "sensor/" sensor_number "/humidity/percentRelative"
#define temperature_c_topic "sensor/" sensor_number "/temperature/degreeCelsius"
#define temperature_f_topic "sensor/" sensor_number "/temperature/degreeFahrenheit"
#define barometer_hpa_topic "sensor/" sensor_number "/barometer/hectoPascal"
#define barometer_inhg_topic "sensor/" sensor_number "/barometer/inchHg"
#endif

#ifdef TSL2561
#include <Adafruit_TSL2561_U.h>
Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);
#define luminosity_topic "sensor/" sensor_number "/luxmeter/lux"
#endif

#ifdef CCS811
#include <Adafruit_CCS811.h>
Adafruit_CCS811 ccs;
#define air_quality_topic_voc "sensor/" sensor_number "/air/VOC"
#define air_quality_topic_co2 "sensor/" sensor_number "/air/ppmCO2"
#endif

void setup() {

  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(100000);
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);

#ifdef BME280
  // Start BME280
  if (!bme.begin()) {
    Serial.println("Ooops, no BME280 detected ... Check your wiring or I2C ADDR!");
    while (1);
  }
#endif

#ifdef TSL2561
  if (!tsl.begin())
  {
    /* There was a problem detecting the TSL2561 ... check your connections */
    Serial.print("Ooops, no TSL2561 detected ... Check your wiring or I2C ADDR!");
    while (1);
  }
  /* Display some basic information on this sensor */
  displaySensorDetails();

  /* Setup the sensor gain and integration time */
  configureSensor();
#endif

#ifdef CCS811
  if (!ccs.begin()) {
    Serial.println("Ooops, no CCS811 detected ... Check your wiring or I2C ADDR!");
    while (1);
  }

  //calibrate temperature sensor
  while (!ccs.available());
  float temp = ccs.calculateTemperature();
  ccs.setTempOffset(temp - 25.0);
#endif
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
    if (client.connect(esp_client_id)) {
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

#ifdef TSL2561
void configureSensor(void)
{
  /* You can also manually set the gain or enable auto-gain support */
  // tsl.setGain(TSL2561_GAIN_1X);      /* No gain ... use in bright light to avoid sensor saturation */
  // tsl.setGain(TSL2561_GAIN_16X);     /* 16x gain ... use in low light to boost sensitivity */
  tsl.enableAutoRange(true);            /* Auto-gain ... switches automatically between 1x and 16x */

  /* Changing the integration time gives you better sensor resolution (402ms = 16-bit data) */
  //tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS);      /* fast but low resolution */
  tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_101MS);  /* medium resolution and speed   */
  // tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_402MS);  /* 16-bit data but slowest conversions */

  /* Update these values depending on what you've set above! */
  Serial.println("------------------------------------");
  Serial.print  ("Gain:         "); Serial.println("Auto");
  Serial.print  ("Timing:       "); Serial.println("13 ms");
  Serial.println("------------------------------------");
}
#endif

#ifdef TSL2561
void displaySensorDetails(void)
{
  sensor_t sensor;
  tsl.getSensor(&sensor);
  Serial.println("------------------------------------");
  Serial.print  ("Sensor:       "); Serial.println(sensor.name);
  Serial.print  ("Driver Ver:   "); Serial.println(sensor.version);
  Serial.print  ("Unique ID:    "); Serial.println(sensor.sensor_id);
  Serial.print  ("Max Value:    "); Serial.print(sensor.max_value); Serial.println(" lux");
  Serial.print  ("Min Value:    "); Serial.print(sensor.min_value); Serial.println(" lux");
  Serial.print  ("Resolution:   "); Serial.print(sensor.resolution); Serial.println(" lux");
  Serial.println("------------------------------------");
  Serial.println("");
  delay(500);
}
#endif

long lastMsg = 0;
long lastForceMsg = 0;
bool forceMsg = false;
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
#ifdef BME280
    update_temp(forceMsg);
    update_hum(forceMsg);
    update_baro(forceMsg);
#endif

#ifdef TSL2561
    update_lux(forceMsg);
#endif

#ifdef CCS811
    update_co2(forceMsg);
    update_voc(forceMsg);
#endif

    forceMsg = false;
  }

  delay(500); //end of loop delay
}

#ifdef BME280
float temp = 0.0;
void update_temp(bool forced_update) {
  float newTemp = bme.readTemperature();
  if (checkBound(newTemp, temp, diff) || forced_update) {
    temp = newTemp;
    float temp_c = temp; // Celsius
    float temp_f = temp * 1.8F + 32.0F; // Fahrenheit
    Serial.print("New temperature:");
    Serial.print(String(temp_c) + " degC   ");
    Serial.println(String(temp_f) + " degF");
    client.publish(temperature_c_topic, String(temp_c).c_str(), true);
    client.publish(temperature_f_topic, String(temp_f).c_str(), true);
  }
}
#endif

#ifdef BME280
float hum = 0.0;
void update_hum(bool forced_update) {
  float newHum = bme.readHumidity();
  if (checkBound(newHum, hum, diff) || forced_update) {
    hum = newHum;
    Serial.print("New humidity:");
    Serial.println(String(hum) + " %");
    client.publish(humidity_topic, String(hum).c_str(), true);
  }
}
#endif

#ifdef BME280
float baro = 0.0;
void update_baro(bool forced_update) {
  float newBaro = bme.readPressure() / 100.0F;
  if (checkBound(newBaro, baro, diff) || forced_update) {
    baro = newBaro;
    float baro_hpa = baro + baro_corr_hpa; // hPa corrected to sea level
    float baro_inhg = baro_hpa / 33.8639F; // inHg corrected to sea level
    Serial.print("New barometer:");
    Serial.print(String(baro_hpa) + " hPa   ");
    Serial.println(String(baro_inhg) + " inHg");
    client.publish(barometer_hpa_topic, String(baro_hpa).c_str(), true);
    client.publish(barometer_inhg_topic, String(baro_inhg).c_str(), true);
  }
}
#endif

#ifdef TSL2561
float lux = 0.0;
void update_lux(bool forced_update) {

  /* Get a new sensor event */
  sensors_event_t event;
  tsl.getEvent(&event);
  float newLux = event.light;
  if (checkBound(newLux, lux, diff) || forced_update) {
    lux = newLux;
    Serial.print("New lightmeter:");
    Serial.print(String(lux) + " lux   ");
    client.publish(luminosity_topic, String(lux).c_str(), true);
  }
}
#endif

#ifdef CCS811
float co2 = 0.0;
void update_co2(bool forced_update) {
  float newCo2 = ccs.geteCO2();
  if (checkBound(newCo2, co2, diff) || forced_update) {
    co2 = newCo2;
    Serial.print("New CO2:");
    Serial.print(String(co2) + " ppm   ");
    client.publish(air_quality_topic_co2, String(co2).c_str(), true);
  }
}
#endif

#ifdef CCS811
float voc = 0.0;
void update_voc(bool forced_update) {
  float newVoc = ccs.geteCO2();
  if (checkBound(newVoc, voc, diff) || forced_update) {
    voc = newVoc;
    Serial.print("New VOC:");
    Serial.print(String(co2) + " ppm   ");
    client.publish(air_quality_topic_voc, String(voc).c_str(), true);
  }
}
#endif
