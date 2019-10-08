/*  Make the following connections:
  - 3V3 (ESP32) --> 5V (shield's logic voltage pin)
  - GND (ESP32) --> GND (shield)
  - RX2 (ESP32) --> 10 (shield's TX)
  - TX2 (ESP32) --> 11 (shield's RX)
  - D5 (ESP32) --> 7 (shield's RST)
  - D18 (ESP32) --> 6 (shield's PWRKEY)
  - Optional: SCL (GPIO22) and SDA (GPIO21) if you want to use the temperature sensor
*/

/******* ORIGINAL ADAFRUIT FONA LIBRARY TEXT *******/
/***************************************************
  This is an example for our Adafruit FONA Cellular Module

  Designed specifically to work with the Adafruit FONA
  ----> http://www.adafruit.com/products/1946
  ----> http://www.adafruit.com/products/1963
  ----> http://www.adafruit.com/products/2468
  ----> http://www.adafruit.com/products/2542

  These cellular modules use TTL Serial to communicate, 2 pins are
  required to interface
  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!
 ****************************************************/

#include <Wire.h>
#include <WiFi.h>
#include <HardwareSerial.h> // For ESP32 hardware serial
#include "Adafruit_INA260.h"
#include "Adafruit_FONA.h"
#include "Adafruit_Sensor.h"
#include "Adafruit_BME280.h"
#include "./config.h"

// For SIM7000 shield with ESP32
#define FONA_PWRKEY 18
#define FONA_RST 5
#define FONA_TX 16 // ESP32 hardware serial RX2 (GPIO16)
#define FONA_RX 17 // ESP32 hardware serial TX2 (GPIO17)
#define BAUD_RATE 115200

// battery info
const float max_battery_voltage = 4.2;
const float min_battery_voltage = 3.7;

// time intervals
const int min_publish_interval = 1000; // ms
const int publish_interval = 1000 * 5 * 60; // ms
int next_publish, last_publish, last_poll = 0;

// Create the BMP280 temperature sensor object
Adafruit_BME280 temp_sensor;

// create power sensor object
Adafruit_INA260 power_sensor = Adafruit_INA260();

HardwareSerial fonaSS(1);

// Use this one for LTE CAT-M/NB-IoT modules (like SIM7000)
// Notice how we don't include the reset pin because it's reserved for emergencies on the LTE module!
Adafruit_FONA_LTE fona = Adafruit_FONA_LTE();

uint8_t type;
char replybuffer[255]; // this is a large buffer for replies
char latBuff[12], longBuff[12], locBuff[60], speedBuff[12],
     headBuff[12], altBuff[12], altBuff2[12], pressureBuff[12],
     tempBuff[12], humidityBuff[12], weatherBuff[50], currentBuff[12],
     voltageBuff[12], powerBuff[12], battBuff[12], batteryBuff[60];
char imei[16] = {0}; // MUST use a 16 character buffer for IMEI!
float latitude, longitude, speed_kph, heading, altitude, second,
  temperature, altitude2, pressure, humidity, voltage, current,
  power, battery;
uint16_t year;
uint8_t month, day, hour, minute;

// Power on the module
void powerOn() {
  digitalWrite(FONA_PWRKEY, LOW);
  // See spec sheets for your particular module
  delay(100); // For SIM7000
  digitalWrite(FONA_PWRKEY, HIGH);
}

void moduleSetup() {
  // Note: The SIM7000A baud rate seems to reset after being power cycled (SIMCom firmware thing)
  // SIM7000 takes about 3s to turn on but SIM7500 takes about 15s
  // Press reset button if the module is still turning on and the board doesn't find it.
  // When the module is on it should communicate right after pressing reset
  
  // Start at default SIM7000 shield baud rate
  fonaSS.begin(115200, SERIAL_8N1, FONA_TX, FONA_RX); // baud rate, protocol, ESP32 RX pin, ESP32 TX pin

  Serial.println(F("Configuring to 9600 baud"));
  fonaSS.println("AT+IPR=9600"); // Set baud rate
  delay(100); // Short pause to let the command run
  fonaSS.begin(9600, SERIAL_8N1, FONA_TX, FONA_RX); // Switch to 9600
  if (! fona.begin(fonaSS)) {
    Serial.println(F("Couldn't find FONA"));
    while (1); // Don't proceed if it couldn't find the device
  }

  type = fona.type();
  Serial.println(F("FONA is OK"));
  Serial.print(F("Found "));
  switch (type) {
    case SIM7000A:
      Serial.println(F("SIM7000A (American)")); break;
    case SIM7000C:
      Serial.println(F("SIM7000C (Chinese)")); break;
    case SIM7000E:
      Serial.println(F("SIM7000E (European)")); break;
    case SIM7000G:
      Serial.println(F("SIM7000G (Global)")); break;
    default:
      Serial.println(F("???")); break;
  }

  // Print module IMEI number.
  uint8_t imeiLen = fona.getIMEI(imei);
  if (imeiLen > 0) {
    Serial.print("Module IMEI: "); Serial.println(imei);
  }

  // Set modem to full functionality
  fona.setFunctionality(1); // AT+CFUN=1
  fona.setNetworkSettings(F("hologram")); // For Hologram SIM card

  /*
  // Other examples of some things you can set:
  fona.setPreferredMode(38); // Use LTE only, not 2G
  fona.setPreferredLTEMode(1); // Use LTE CAT-M only, not NB-IoT
  fona.enableRTC(true);

  fona.enableSleepMode(true);
  fona.set_eDRX(1, 4, "0010");
  fona.enablePSM(true);

  fona.setNetLED(false); // Disable network status LED
  */
  // Set the network status LED blinking pattern while connected to a network (see AT+SLEDS command)
  fona.setNetLED(true, 2, 64, 3000); // on/off, mode, timer_on, timer_off
  while (!fona.enableGPS(true)) {
    Serial.println("Failed to turn on gps");
    delay(1000);
  }
  while (!fona.enableGPRS(true)) {
    Serial.println("Failed to turn on data");
    delay(1000);
  }
  // Optionally configure HTTP gets to follow redirects over SSL.
  // Default is not to follow SSL redirects, however if you uncomment
  // the following line then redirects over SSL will be followed.
  fona.setHTTPSRedirect(true);
}

void initializeSensors() {
  Serial.println("initializing the temp sensor...");
  if (!temp_sensor.begin()) {
    Serial.println("could not find valid temp sensor!");
    while (1);
  }
  Serial.println("initializing the power sensor...");
  if (!power_sensor.begin()) {
    Serial.println("could not find valid power sensor!");
    while (1);
  }
  power_sensor.setAveragingCount(INA260_COUNT_4);
  power_sensor.setVoltageConversionTime(INA260_TIME_140_us);
  power_sensor.setCurrentConversionTime(INA260_TIME_140_us);
}

int8_t checkGPS() {
  // get gps status current
  int8_t gps_stat = fona.GPSstatus();
  String message;
  if (gps_stat < 0)
    message = "Failed to query gps data";
  if (gps_stat == 0) message = "GPS off";
  if (gps_stat == 1) message = "No GPS fix";
  if (gps_stat == 2) message = "2D GPS fix";
  if (gps_stat == 3) message = "3D GPS fix";
  Serial.println(message);
  const char* message_char = message.c_str();
  if (gps_stat <= 2 && !fona.MQTT_publish(ERROR_TOPIC, message_char, strlen(message_char), 1, 0))
    Serial.println("Failed to publish error message");
  return gps_stat;
}

void getLocation() {
  if (fona.getGPS(&latitude, &longitude, &speed_kph, &heading, &altitude, &year, &month, &day, &hour, &minute, &second)) {
    Serial.println(F("---------------------"));
    Serial.print(F("Latitude: ")); Serial.println(latitude, 6);
    Serial.print(F("Longitude: ")); Serial.println(longitude, 6);
    Serial.print(F("Speed: ")); Serial.println(speed_kph);
    Serial.print(F("Heading: ")); Serial.println(heading);
    Serial.print(F("Altitude: ")); Serial.println(altitude);
    Serial.print(F("Year: ")); Serial.println(year);
    Serial.print(F("Month: ")); Serial.println(month);
    Serial.print(F("Day: ")); Serial.println(day);
    Serial.print(F("Hour: ")); Serial.println(hour);
    Serial.print(F("Minute: ")); Serial.println(minute);
    Serial.print(F("Second: ")); Serial.println(second);
    Serial.println(F("---------------------"));
  } else {
    Serial.println("could not get location");
  }
}

void getTime() {
  char buffer[23];
  fona.getTime(buffer, 23);  // make sure replybuffer is at least 23 bytes!
  Serial.print(F("Time = ")); Serial.println(buffer);
}

void getWeatherSensorData() {
  // Measure temperature
  temperature = temp_sensor.readTemperature();
  Serial.print("Temperature = ");
  Serial.print(temperature);
  Serial.println(" *C");

  pressure = temp_sensor.readPressure() / 100.0F;
  Serial.print("Pressure = ");
  Serial.print(pressure);
  Serial.println(" hPa");

  // altitude
  altitude2 = temp_sensor.readAltitude(SENSORS_PRESSURE_SEALEVELHPA);
  Serial.print("Real altitude = ");
  Serial.print(altitude2);
  Serial.println(" meters");

  // humidity
  humidity = temp_sensor.readHumidity();
  Serial.print("Humidity = ");
  Serial.print(humidity);
  Serial.println(" %");
}

void getPowerSensorData() {
  // get power readings
  // voltage
  voltage = power_sensor.readBusVoltage();
  Serial.print("Voltage = ");
  Serial.println(voltage);

  // current
  current = power_sensor.readCurrent();
  Serial.print("Current = ");
  Serial.println(current);

  // power
  power = power_sensor.readPower();
  Serial.print("Power = ");
  Serial.println(power);

  // battery
  battery = (voltage - min_battery_voltage) / (max_battery_voltage - min_battery_voltage) * 100;
  Serial.print("Battery = ");
  Serial.println(battery);
  Serial.println(" %");
}

void connectMQTT() {
  // If not already connected, connect to MQTT
  if (!fona.MQTT_connectionStatus()) {
    // Set up MQTT parameters (see MQTT app note for explanation of parameter values)
    fona.MQTT_setParameter("URL", MQTT_SERVER, MQTT_PORT);
    // Set up MQTT username and password if necessary
    fona.MQTT_setParameter("USERNAME", MQTT_USERNAME);
    fona.MQTT_setParameter("PASSWORD", MQTT_PASSWORD);
    // fona.MQTT_setParameter("KEEPTIME", "30"); // Time to connect to server, 60s by default
    
    Serial.println(F("Connecting to MQTT broker..."));
    if (!fona.MQTT_connect(true)) {
      Serial.println("Failed to connect to MQTT broker!");
    }
    // Note the command below may error out if you're already subscribed to the topic!
    fona.MQTT_subscribe(COMMAND_TOPIC, 1); // Topic name, QoS
  }
}

void getData() {
  // getWeatherSensorData();
  getPowerSensorData();
  getLocation();
}

void publishLocationData() {
  // Format the floating point numbers
  dtostrf(latitude, 1, 6, latBuff); // float_val, min_width, digits_after_decimal, char_buffer
  dtostrf(longitude, 1, 6, longBuff);
  dtostrf(speed_kph, 1, 0, speedBuff);
  dtostrf(heading, 1, 0, headBuff);
  dtostrf(altitude, 1, 1, altBuff);
  // Construct a combined, comma-separated location array
  sprintf(locBuff, "%s,%s,%s,%s,%s", speedBuff, latBuff, longBuff, altBuff, headBuff); // This could look like "10,33.123456,-85.123456,120.5,50"
  // Parameters for MQTT_publish: Topic, message (0-512 bytes), message length, QoS (0-2), retain (0-1)
  if (!fona.MQTT_publish(GPS_TOPIC, locBuff, strlen(locBuff), 1, 0))
    Serial.println(F("Failed to publish location")); // Send GPS location
}

void publishWeatherSensorData() {
  // Format the floating point numbers
  dtostrf(temperature, 1, 2, tempBuff); // float_val, min_width, digits_after_decimal, char_buffer
  dtostrf(pressure, 1, 2, pressureBuff); // float_val, min_width, digits_after_decimal, char_buffer
  dtostrf(altitude2, 1, 2, altBuff2); // float_val, min_width, digits_after_decimal, char_buffer
  dtostrf(humidity, 1, 2, humidityBuff); // float_val, min_width, digits_after_decimal, char_buffer
  sprintf(weatherBuff, "%s,%s,%s,%s,%s", tempBuff, pressureBuff, altBuff2, humidityBuff);
  if (!fona.MQTT_publish(WEATHER_TOPIC, weatherBuff, strlen(weatherBuff), 1, 0))
    Serial.println("Failed to publish humidity");
}

void publishPowerSensorData() {
  // Format the floating point numbers
  dtostrf(voltage, 1, 2, voltageBuff); // float_val, min_width, digits_after_decimal, char_buffer
  dtostrf(current, 1, 2, currentBuff); // float_val, min_width, digits_after_decimal, char_buffer
  dtostrf(current, 1, 2, currentBuff); // float_val, min_width, digits_after_decimal, char_buffer
  dtostrf(battery, 1, 2, battBuff); // float_val, min_width, digits_after_decimal, char_buffer
  sprintf(batteryBuff, "%s,%s,%s,%s", voltageBuff, currentBuff, powerBuff, battBuff);
  if (!fona.MQTT_publish(BATTERY_TOPIC, batteryBuff, strlen(batteryBuff), 1, 0))
    Serial.println("Failed to publish battery level");
}

void publishData() {
  // Now publish all the GPS, voltage and temperature data to their topics
  getData();
  if (checkGPS() >= (int8_t)2) {
    publishLocationData();
  }
  publishWeatherSensorData();
  publishPowerSensorData();
  last_publish = millis();
}

void setup() {
  // disable the radios
  WiFi.mode(WIFI_OFF);
  btStop();
  while (!Serial);
  Serial.begin(BAUD_RATE);
  Serial.println("ESP32");
  Serial.println("Initializing....(May take several seconds)");
  initializeSensors();

  pinMode(FONA_RST, OUTPUT);
  digitalWrite(FONA_RST, HIGH); // Default state

  pinMode(FONA_PWRKEY, OUTPUT);

  // Turn on the module by pulsing PWRKEY low for a little bit
  // This amount of time depends on the specific module that's used
  powerOn(); // See function definition at the very end of the sketch
  moduleSetup();
  getData(); // get data on startup
}

void handleSubscribe() {
  if (fona.available()) {
    uint8_t i = 0;
    while (fona.available()) {
      replybuffer[i] = fona.read();
      i++;
    }
    Serial.print(replybuffer);
    delay(100); // Make sure it prints and also allow other stuff to run properly

    // Format: +SMSUB: "topic_name","message"
    String reply = String(replybuffer);
    Serial.println(reply);

    if (reply.indexOf("+SMSUB: ") != -1) {
      Serial.println(F("*** Received MQTT message! ***"));
      // Chop off the "SMSUB: " part plus the beginning quote
      // After this, reply should be: "topic_name","message"
      reply = reply.substring(9);
      uint8_t idx = reply.indexOf("\",\""); // Search for second quote
      String topic = reply.substring(1, idx); // Grab only the text (without quotes)
      String message = reply.substring(idx+3, reply.length()-3);
      Serial.print(F("Topic: ")); Serial.println(topic);
      Serial.print(F("Message: ")); Serial.println(message);
      // Do something with the message
      int current_time = millis();
      if (message == "connect") {
        if (next_publish - current_time > min_publish_interval) {
          next_publish = current_time + min_publish_interval;
        } else {
          Serial.println("next connect output already queued");
        }
      } else if (message == "poll") {
        if (next_publish - current_time < publish_interval) {
          Serial.println("next poll output already queued");
        } else if (current_time - last_publish < publish_interval) {
          next_publish = current_time + min_publish_interval;
        } else {
          next_publish = current_time;
        }
      } else {
        Serial.println("invalid topic given");
      }
    }
  }
}

void loop() {
  if (millis() > next_publish) {
    connectMQTT();
    getTime();
    publishData();
  }
  handleSubscribe();
}
