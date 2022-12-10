#include "secrets.h"
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

#define SENSOR_PIN GPIO_NUM_34

#define AIR_MOISTURE 2610
#define WATER_MOISTURE 900

#define SAMPLE_DELAY 1000
#define BUFFER_SIZE 16

#define NTP_SERVER "pool.ntp.org"

void setup();
void loop();
int8_t mapMoisture(uint16_t moisture);
void setupWiFi();
void setupFirebase();
time_t now();

uint16_t moistureBuffer[BUFFER_SIZE];
size_t moistureSample = 0;

FirebaseData fbData;
FirebaseAuth fbAuth;
FirebaseConfig fbConfig;
FirebaseJson fbJson;

void setup() {
  Serial.begin(9600);
  uint16_t moisture = analogRead(SENSOR_PIN);
  memset(moistureBuffer, moisture, BUFFER_SIZE);
  setupWiFi();
  setupFirebase();
  configTime(0, 0, NTP_SERVER);
}

void loop() {
  uint16_t moisture = analogRead(SENSOR_PIN);
  moistureBuffer[moistureSample] = moisture;
  moistureSample = ++moistureSample % BUFFER_SIZE;
  int8_t moisturePct = mapMoisture(moisture);
  uint16_t movingAverage = 0;
  for (int i = 0; i < BUFFER_SIZE; i++) movingAverage += moistureBuffer[i];
  movingAverage /= BUFFER_SIZE;
  int8_t movingAveragePct = mapMoisture(movingAverage);
  Serial.printf("Moisture value %d (%d%%) MA%d %d (%d%%)\n",
                moisture, moisturePct, BUFFER_SIZE, movingAverage, movingAveragePct);
  if (Firebase.ready()) {
    time_t timestamp = now();
    String timestamp_str = String(timestamp);
    String path = String("/") + DEVICE_NAME + String("/") + timestamp_str;
    fbJson.set("/moisture", moisture);
    fbJson.set("/timestamp", timestamp);
    String json;
    fbJson.toString(json, true);
    Serial.println(json);
    if (!Firebase.RTDB.setJSON(&fbData, path, &fbJson)) {
      Serial.print("Logging failed: ");
      Serial.println(fbData.errorReason());
    }
  }
  delay(SAMPLE_DELAY);
}

int8_t mapMoisture(uint16_t moisture) {
  return map(moisture, AIR_MOISTURE, WATER_MOISTURE, 0, 100);
}

void setupWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PWD);
  Serial.printf("Connecting to WiFi (SSID %s)..", WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println();
  Serial.print("Connected with IP ");
  Serial.println(WiFi.localIP());
}

void setupFirebase() {
  Firebase.reconnectWiFi(true);
  fbConfig.api_key = FIREBASE_API_KEY;
  fbConfig.database_url = FIREBASE_DATABASE_URL;
  fbConfig.token_status_callback = tokenStatusCallback;
  if (Firebase.signUp(&fbConfig, &fbAuth, "", "")) {
    Serial.print("Authentication successful with UID ");
    Serial.println(fbAuth.token.uid.c_str());
  } else {
    Serial.print("Authentication failed: ");
    Serial.println(fbConfig.signer.signupError.message.c_str());
  }
  Firebase.begin(&fbConfig, &fbAuth);
}

time_t now() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return 0;
  }
  time(&now);
  return now;
}