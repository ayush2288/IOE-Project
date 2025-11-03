/* 
  ESP32 Noise & Air Monitor → Firebase Firestore (Direct Upload)
  - MQ-135 analog -> GPIO34
  - Mic analog    -> GPIO35
  - LED noise     -> GPIO18
  - LED air       -> GPIO19
  - Buzzer        -> GPIO23
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ---------------- WiFi Credentials ----------------
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// ---------------- Firebase Config ----------------
const char* FIREBASE_PROJECT_ID = "ioe-project-562ab";
const char* FIREBASE_API_KEY = "AIzaSyBgKXWMDbNQhaZTPCkrYvACKHTYzLMLLjk";
String FIRESTORE_URL = "https://firestore.googleapis.com/v1/projects/" + String(FIREBASE_PROJECT_ID) + "/databases/(default)/documents/sensorData";

// ---------------- Pins ----------------
#define MQ_PIN 34
#define MIC_PIN 35
#define LED_NOISE 18
#define LED_AIR 19
#define BUZZER 23

// ---------------- Constants ----------------
const int MIC_SAMPLES = 200;
const int MQ_SAMPLES = 10;
const int MQ_SAMPLE_DELAY_MS = 50;
const int ADC_MAX = 4095;

int noiseThreshold = 600;
int airThreshold = 700;

float mic_a = 0.05;
float mic_b = 20.0;

// ---------------- Sensor Reading Functions ----------------
int readMQ() {
  long sum = 0;
  for (int i = 0; i < MQ_SAMPLES; i++) {
    int v = analogRead(MQ_PIN);
    sum += v;
    delay(MQ_SAMPLE_DELAY_MS);
  }
  return sum / MQ_SAMPLES;
}

int readMicPeakToPeak() {
  int signalMax = 0;
  int signalMin = ADC_MAX;
  for (int i = 0; i < MIC_SAMPLES; i++) {
    int val = analogRead(MIC_PIN);
    if (val > signalMax) signalMax = val;
    if (val < signalMin) signalMin = val;
    delay(1);
  }
  return signalMax - signalMin;
}

float micReadingToDb(int reading) {
  return mic_a * reading + mic_b;
}

// ---------------- Firebase Upload Function ----------------
void sendToFirestore(float airValue, float noiseValue) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, reconnecting...");
    WiFi.begin(ssid, password);
    return;
  }

  HTTPClient http;
  http.begin(FIRESTORE_URL);
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<512> doc;
  JsonObject fields = doc.createNestedObject("fields");
  fields["air 1"]["doubleValue"] = airValue;
  fields["noise 1"]["doubleValue"] = noiseValue;
  fields["time"]["stringValue"] = String(__DATE__) + " " + String(__TIME__);

  String jsonBody;
  serializeJson(doc, jsonBody);

  Serial.println("Uploading to Firebase...");
  Serial.println(jsonBody);

  int httpResponseCode = http.POST(jsonBody);
  if (httpResponseCode > 0) {
    Serial.printf("Response code: %d\n", httpResponseCode);
    String response = http.getString();
    Serial.println(response);
  } else {
    Serial.printf("HTTP POST failed, code: %d\n", httpResponseCode);
  }

  http.end();
}

// ---------------- Setup ----------------
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(LED_NOISE, OUTPUT);
  pinMode(LED_AIR, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  digitalWrite(LED_NOISE, LOW);
  digitalWrite(LED_AIR, LOW);
  digitalWrite(BUZZER, LOW);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi!");
}

// ---------------- Main Loop ----------------
void loop() {
  int mqVal = readMQ();
  int micVal = readMicPeakToPeak();
  float micDb = micReadingToDb(micVal);

  bool noiseAlert = micVal >= noiseThreshold;
  bool airAlert = mqVal >= airThreshold;

  digitalWrite(LED_NOISE, noiseAlert);
  digitalWrite(LED_AIR, airAlert);

  if (noiseAlert || airAlert) {
    tone(BUZZER, 1000, 150);
  }

  Serial.printf("Air: %d | Noise: %d (%.1f dB)\n", mqVal, micVal, micDb);

  // Upload to Firebase
  sendToFirestore(mqVal, micVal);

  delay(60000); // Upload every minute
}
