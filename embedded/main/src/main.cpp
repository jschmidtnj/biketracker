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
#include "Adafruit_FONA.h"

// For SIM7000 shield with ESP32
#define FONA_PWRKEY 18
#define FONA_RST 5
#define FONA_TX 16 // ESP32 hardware serial RX2 (GPIO16)
#define FONA_RX 17 // ESP32 hardware serial TX2 (GPIO17)

#define BAUD_RATE 115200

// For ESP32 hardware serial
#include <HardwareSerial.h>
HardwareSerial fonaSS(1);

// Use this one for LTE CAT-M/NB-IoT modules (like SIM7000)
// Notice how we don't include the reset pin because it's reserved for emergencies on the LTE module!
Adafruit_FONA_LTE fona = Adafruit_FONA_LTE();

uint8_t readline(char *buff, uint8_t maxbuff, uint16_t timeout = 0);
uint8_t type;
char replybuffer[255]; // this is a large buffer for replies
char imei[16] = {0}; // MUST use a 16 character buffer for IMEI!

// Power on the module
void powerOn() {
  digitalWrite(FONA_PWRKEY, LOW);
  // See spec sheets for your particular module
  delay(100); // For SIM7000

  digitalWrite(FONA_PWRKEY, HIGH);
}

void setup() {
  // disable the radios
  WiFi.mode(WIFI_OFF);
  btStop();
  while (!Serial);

  pinMode(FONA_RST, OUTPUT);
  digitalWrite(FONA_RST, HIGH); // Default state

  pinMode(FONA_PWRKEY, OUTPUT);

  // Turn on the module by pulsing PWRKEY low for a little bit
  // This amount of time depends on the specific module that's used
  powerOn(); // See function definition at the very end of the sketch

  Serial.begin(BAUD_RATE);
  Serial.println(F("ESP32 Basic Test"));
  Serial.println(F("Initializing....(May take several seconds)"));

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

  // Optionally configure HTTP gets to follow redirects over SSL.
  // Default is not to follow SSL redirects, however if you uncomment
  // the following line then redirects over SSL will be followed.
  fona.setHTTPSRedirect(true);
  if (!fona.HTTP_ssl(true)) {
    Serial.println("unable to set https");
  }

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
}

void checkGPS() {
  // get gps status current
  int8_t gps_stat = fona.GPSstatus();
  while (gps_stat < 2) {
    if (gps_stat < 0)
      Serial.println("Failed to query gps data");
    if (gps_stat == 0) Serial.println("GPS off");
    if (gps_stat == 1) Serial.println("No fix");
    if (gps_stat == 2) Serial.println("2D fix");
    if (gps_stat == 3) Serial.println("3D fix");
    delay(500);
    gps_stat = fona.GPSstatus();
  }
}

void getLocation() {
  float latitude, longitude, speed_kph, heading, altitude, second;
  uint16_t year;
  uint8_t month, day, hour, minute;

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

void getSensorData() {
  float temperature = analogRead(A0) * 1.23; // Change this to suit your needs
  Serial.print("temp: "); Serial.println(temperature);
  uint16_t vbat;
  if (! fona.getBattVoltage(&vbat)) {
    Serial.println(F("Failed to read Batt"));
  } else {
    Serial.print(F("VBat = ")); Serial.print(vbat); Serial.println(F(" mV"));
  }

  if ( (type != SIM7500A) && (type != SIM7500E) ) {
    if (! fona.getBattPercent(&vbat)) {
      Serial.println(F("Failed to read Batt"));
    } else {
      Serial.print(F("VPct = ")); Serial.print(vbat); Serial.println(F("%"));
    }
  }
}

void postData() {
  uint16_t statuscode;
  int16_t length;
  char url[] = "google.com";
  char data[] = "{\"data\":\"test\"}";
  if (!fona.HTTP_POST_start(url, F("text/plain"), (uint8_t *) data, strlen(data), &statuscode, (uint16_t *)&length)) {
    Serial.println("Failed post data!");
    return;
  }
  while (length > 0) {
    while (fona.available()) {
      char c = fona.read();
      Serial.write(c);
      length--;
      if (! length) break;
    }
  }
  Serial.println(F("\n****"));
  fona.HTTP_POST_end();
}

void loop() {
  checkGPS();
  getLocation();
  getTime();
  getSensorData();
  delay(500);
}
