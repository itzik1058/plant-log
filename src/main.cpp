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

#define SAMPLE_DELAY 10 // seconds

#define NTP_SERVER "pool.ntp.org"

void setup();
void loop();
void setupWiFi();
void setupFirebase();
time_t now();
void logMoisture(uint16_t moisture);

RTC_DATA_ATTR size_t wakeup;
FirebaseData fbData;
FirebaseAuth fbAuth;
FirebaseConfig fbConfig;
FirebaseJson fbJson;

void setup()
{
  Serial.begin(9600);

  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  Serial.printf("Wakeup %d caused by %d\n", wakeup, wakeup_reason);

  setupWiFi();
  setupFirebase();
  configTime(0, 0, NTP_SERVER);

  uint16_t moisture = analogRead(SENSOR_PIN);
  int8_t moisturePct = map(moisture, AIR_MOISTURE, WATER_MOISTURE, 0, 100);
  Serial.printf("Moisture value %d (%d%%)\n", moisture, moisturePct);
  logMoisture(moisture);

  esp_sleep_enable_timer_wakeup(SAMPLE_DELAY * 1000000);
  esp_deep_sleep_start();
}

void loop() {}

void setupWiFi()
{
  WiFi.begin(WIFI_SSID, WIFI_PWD);
  Serial.printf("Connecting to WiFi (SSID %s)..", WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(1000);
  }
  Serial.println();
  Serial.print("Connected with IP ");
  Serial.println(WiFi.localIP());
}

void setupFirebase()
{
  Firebase.reconnectWiFi(true);
  fbConfig.api_key = FIREBASE_API_KEY;
  fbConfig.database_url = FIREBASE_DATABASE_URL;
  fbConfig.token_status_callback = tokenStatusCallback;
  if (Firebase.signUp(&fbConfig, &fbAuth, "", ""))
  {
    Serial.print("Authentication successful with UID ");
    Serial.println(fbAuth.token.uid.c_str());
  }
  else
  {
    Serial.print("Authentication failed: ");
    Serial.println(fbConfig.signer.signupError.message.c_str());
  }
  Firebase.begin(&fbConfig, &fbAuth);
}

time_t now()
{
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    return 0;
  }
  time(&now);
  return now;
}

void logMoisture(uint16_t moisture)
{
  if (!Firebase.ready()) {
    Serial.println("Firebase is not ready");
    return;
  }
  time_t timestamp = now();
  String path = DEVICE_NAME "/" + String(timestamp);
  fbJson.set("/moisture", moisture);
  fbJson.set("/timestamp", timestamp);
  String json;
  fbJson.toString(json, true);
  Serial.println(json);
  if (!Firebase.RTDB.setJSON(&fbData, path, &fbJson))
  {
    Serial.print("Logging failed: ");
    Serial.println(fbData.errorReason());
  }
}