#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>  // To send HTTP requests to MagicMirror.
#include <AutoConnect.h>        // AutoConnect by Hieromon Ikasamo 1.4.2 (https://hieromon.github.io/AutoConnect/index.html)

// Led pins.
int pinWorkingLed = D5;
int pinLevelOkLed = D8;
int pinLevelWarningLed = D7;
int pinLevelAlertLed = D6;

// HC-SR04 ultrasonic sensor pins.
int pinSensorTrigger = D1;
int pinSensorEcho = D2;

// Network.
const char* WIFI_AP_SSID = "SmartWater";             // The name of the wireless network to create if cannot connect using the previously saved credentials.
const char* WIFI_AP_PASSWORD = "SuperSecret";        // The password required to connect to the wireless network used to configure the network parameters.

const char* MAGIC_MIRROR_ENDPOINT = "http://192.168.0.196:8080/smart-water";  // The IP address and port number of the MagicMirror device on the network.

#define UPDATE_INTERVAL_IN_SECONDS 5                // The frequency of measuring data and sending updates in seconds.
#define MEASURE_COUNT 3                             // The number of measurements used for averaging.
#define MIN_OK_DISTANCE_IN_CM 50                    // The minimum value for OK water level. Above this value the green led will turn on.
#define MIN_WARNING_DISTANCE_IN_CM 30               // The minimum value for warning water level. Above this value the yellow led will turn on.
#define MAX_VALID_DISTANCE_IN_CM 200                // The maxmimum value that is accepted as a valid value. Above this the measured values are ignored.

// -------- Config ends --------

#define SOUND_SPEED 0.034 // cm/microSeconds

AutoConnect autoConnect;

enum Level {
  UNKNOWN_LEVEL,
  OK_LEVEL,
  WARNING_LEVEL,
  ALERT_LEVEL
};

void setup() {
  Serial.begin(115200);
  setupLeds();
  setupSensor();
  setupNetwork();
}

void setupLeds() {
  pinMode(pinWorkingLed, OUTPUT);
  pinMode(pinLevelOkLed, OUTPUT);
  pinMode(pinLevelWarningLed, OUTPUT);
  pinMode(pinLevelAlertLed, OUTPUT);

  turnLedOn(pinWorkingLed);
  delay(500);
  turnLedOn(pinLevelOkLed);
  delay(500);
  turnLedOn(pinLevelWarningLed);
  delay(500);
  turnLedOn(pinLevelAlertLed);
  delay(1000);
  turnLedOff(pinLevelOkLed);
  turnLedOff(pinLevelWarningLed);
  turnLedOff(pinLevelAlertLed);  
}

void setupSensor() {
  pinMode(pinSensorTrigger, OUTPUT);
  pinMode(pinSensorEcho, INPUT);
}


void setupNetwork() {
  Serial.println("Connecting to network...");
  
  autoConnect.config(WIFI_AP_SSID, WIFI_AP_PASSWORD);

  if (autoConnect.begin()) {
    Serial.println("WiFi connected: " + WiFi.localIP().toString());
  }
}

void loop() {
  long distanceInCm = measureDistanceInCmAverage();
  if (isValidDistance(distanceInCm)) {
    Serial.println(distanceInCm);
    setLevel(distanceInCm);
    sendToMagicMirror(distanceInCm);

    delay(UPDATE_INTERVAL_IN_SECONDS * 1000);
  }
}

void turnLedOn(int ledPin) {
  digitalWrite(ledPin, HIGH);
}

void turnLedOff(int ledPin) {
  digitalWrite(ledPin, LOW);
}

long measureDistanceInCmAverage() {
  int sum = 0;
  for (int i = 0; i < MEASURE_COUNT; i++) {
    sum += measureDistanceInCmOnce();
    delay(100);
  }
  long averageDistanceInCm = sum / MEASURE_COUNT;
  return averageDistanceInCm;
}

long measureDistanceInCmOnce() {
  // Clear the trigger pin.
  digitalWrite(pinSensorTrigger, LOW);
  delayMicroseconds(2);
  // Send out a 10 micro second signal.
  digitalWrite(pinSensorTrigger, HIGH);
  delayMicroseconds(10);
  digitalWrite(pinSensorTrigger, LOW);
  // Wait for the bounced signal.
  long durationInMicroSeconds = pulseIn(pinSensorEcho, HIGH);
  // Calculate the distance.
  long distanceInCm = durationInMicroSeconds * SOUND_SPEED/2;  
  return distanceInCm;
}

bool isValidDistance(long distanceInCm) {
  return distanceInCm <= MAX_VALID_DISTANCE_IN_CM;
}

void setLevel(long distanceInCm) {
  if (distanceInCm > MIN_OK_DISTANCE_IN_CM) {
    setLevelLeds(OK_LEVEL);
  }
  else if (distanceInCm > MIN_WARNING_DISTANCE_IN_CM) {
    setLevelLeds(WARNING_LEVEL);
  }
  else {
    setLevelLeds(ALERT_LEVEL);
  }
}

void setLevelLeds(Level level) {
  switch (level) {
    case OK_LEVEL:
      turnLedOn(pinLevelOkLed);
      turnLedOff(pinLevelWarningLed);
      turnLedOff(pinLevelAlertLed);  
      break;
    case WARNING_LEVEL:
      turnLedOff(pinLevelOkLed);
      turnLedOn(pinLevelWarningLed);
      turnLedOff(pinLevelAlertLed);      
      break;
    case ALERT_LEVEL:
      turnLedOff(pinLevelOkLed);
      turnLedOff(pinLevelWarningLed);
      turnLedOn(pinLevelAlertLed);       
      break;
    default:
      turnLedOn(pinLevelOkLed);
      turnLedOn(pinLevelWarningLed);
      turnLedOn(pinLevelAlertLed);        
      break;
  }
}

void sendToMagicMirror(long distanceInCm) {
  String requestBody = "{ \"distanceInCm\": " + String(distanceInCm) + " }";
  Serial.println("MagicMirror: HTTP request body: " + requestBody);

  WiFiClient wifiClient;
  HTTPClient http;
  http.begin(wifiClient, MAGIC_MIRROR_ENDPOINT);
  http.addHeader("Content-Type", "application/json");
  
  int statusCode = http.POST(requestBody);

  Serial.printf("MagicMirror: Received HTTP status code: %d\r\n", statusCode);
  if (statusCode != HTTP_CODE_OK) {
    String responseBody = http.getString();
    Serial.println("MagicMirror: Received HTTP response body: " + responseBody);
  }

  http.end();
}
