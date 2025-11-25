#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <WiFi.h>
#include <HTTPClient.h> // Masih pakai HTTP biasa sesuai request
#include <ArduinoJson.h>
#include "time.h"
#include <vector>

// ==========================================
//      KONFIGURASI USER
// ==========================================
#define WIFI_SSID "Gembong Center"
#define WIFI_PASSWORD "Pawpatrol#321"

// GANTI IP VPS ANDA (HTTP)
String API_URL = "http://147.139.136.133:3000"; 

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 25200; 
const int   daylightOffset_sec = 0; 

// Hardware Pins
#define DHT_PIN 18      
#define REED_PIN 5      
#define LED_PIN 13      
#define BUZZER_PIN 25   
#define DHT_TYPE DHT11

// Global Objects
DHT dht(DHT_PIN, DHT_TYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2); 

// Timers Global
unsigned long lastDHTRead = 0;
unsigned long lastCloudUpload = 0;
unsigned long lastScheduleFetch = 0;
const long intervalFetch = 15000; 

// --- LOGIC VARIABLES ---
float currentTemp = 0.0;
String currentBoxStatus = "UNKNOWN";
String lastSentStatus = "UNKNOWN";
String displayedTime = "--:--";

// Schedule Logic
std::vector<String> alarmSchedules; 
bool isScheduleActive = false;       
unsigned long scheduleTriggerMillis = 0; // Waktu saat jadwal dimulai
String lastTriggeredTime = "";       

// High Temp Logic
unsigned long lastHighTempBeep = 0;       

// ==========================================
//      HELPER FUNCTIONS
// ==========================================

String getLocalTimeStr() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) return "--:--";
  char timeStringBuff[6]; 
  strftime(timeStringBuff, sizeof(timeStringBuff), "%H:%M", &timeinfo);
  return String(timeStringBuff);
}

void fetchSchedules() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(API_URL + "/api/data");
    int code = http.GET();
    if (code == 200) {
      DynamicJsonDocument doc(2048); 
      deserializeJson(doc, http.getString());
      alarmSchedules.clear();
      JsonArray schedules = doc["schedules"];
      for (JsonObject s : schedules) {
        String t = s["time"].as<String>();
        if (t.length() == 5) alarmSchedules.push_back(t);
      }
    }
    http.end();
  }
}

void uploadStatus(float temp, String status, String timeStr) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(API_URL + "/api/sensor");
    http.addHeader("Content-Type", "application/json");
    String payload = "{\"temp\": " + String(temp, 1) + 
                     ", \"status\": \"" + status + "\"" +
                     ", \"last_update\": \"" + timeStr + "\"}";
    http.POST(payload);
    http.end();
    lastCloudUpload = millis();
    lastSentStatus = status;
  }
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(REED_PIN, INPUT); 
  
  Wire.begin(); lcd.init(); lcd.backlight();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lcd.setCursor(0,0); lcd.print("WiFi Connect...");
  while (WiFi.status() != WL_CONNECTED) delay(500);
  
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  fetchSchedules();
  lcd.clear();
}

void loop() {
  String currentTime = getLocalTimeStr();
  unsigned long now = millis();

  // 1. BACA SENSOR & STATUS PINTU
  if (now - lastDHTRead > 2000) {
    float t = dht.readTemperature();
    if (!isnan(t)) currentTemp = t;
    lastDHTRead = now;
  }
  
  // Logic Reed Switch (Low = Closed, High = Open)
  bool isClosed = (digitalRead(REED_PIN) == LOW);
  String detectedStatus = isClosed ? "CLOSED" : "OPEN";

  // Jika pintu dibuka, reset jadwal & alarm
  if (!isClosed) {
      if (isScheduleActive) {
          isScheduleActive = false;
          Serial.println("Box Opened -> Alarm Reset");
      }
      // Matikan output fisik
      digitalWrite(LED_PIN, LOW);
      analogWrite(BUZZER_PIN, 0);
  }

  // 2. CEK JADWAL (Hanya jika pintu tertutup)
  if (isClosed && currentTime != lastTriggeredTime) {
    for (String s : alarmSchedules) {
      if (currentTime == s) {
        isScheduleActive = true;
        scheduleTriggerMillis = now; // Mulai stopwatch
        lastTriggeredTime = currentTime;
        Serial.println("ALARM STARTED!");
      }
    }
  }

  // 3. LOGIKA UTAMA (RULES 1-4)
  bool buzzerState = false;
  bool ledState = false;
  
  // RULE 1: HIGH TEMP WARNING (> 40C)
  // Logic: Kalau suhu > 40, override semua logic lain.
  if (currentTemp > 40.0 && isClosed) {
      // Bunyi setiap 5 detik
      if (now - lastHighTempBeep >= 5000) {
          // Bip pendek (200ms)
          analogWrite(BUZZER_PIN, 200);
          delay(200); 
          analogWrite(BUZZER_PIN, 0);
          lastHighTempBeep = now;
      }
      detectedStatus = "DANGER"; // Kirim status Bahaya ke App
  }
  else if (isScheduleActive && isClosed) {
      // --- LOGIKA JADWAL BERJALAN ---
      unsigned long elapsed = now - scheduleTriggerMillis;

      // RULE 3: LED ON for 20 seconds
      if (elapsed < 20000) {
          ledState = true;
      }

      // RULE 4: Delay 15s, then Buzzer ON for 15s (Total 30s mark)
      if (elapsed >= 15000 && elapsed < 30000) {
          buzzerState = true;
      }

      // RULE 2: Warning if not opened in 20s
      if (elapsed >= 20000) {
          detectedStatus = "LATE"; // Status khusus agar App memunculkan Warning
      }

      // Matikan jadwal otomatis setelah 30 detik (agar tidak loop selamanya)
      if (elapsed >= 30000) {
          isScheduleActive = false;
      }
  }

  // 4. EKSEKUSI HARDWARE
  // (Kecuali kondisi High Temp yang punya logic bip sendiri di atas)
  if (currentTemp <= 40.0) {
      digitalWrite(LED_PIN, ledState ? HIGH : LOW);
      analogWrite(BUZZER_PIN, buzzerState ? 150 : 0);
  }

  // 5. UPDATE SERVER
  // Jika status berubah (misal dari CLOSED jadi LATE), kirim segera
  if (detectedStatus != lastSentStatus || now - lastCloudUpload > 5000) {
      uploadStatus(currentTemp, detectedStatus, currentTime);
      // Agar tidak spam upload terus menerus saat status LATE
      if (detectedStatus == "LATE" && lastSentStatus == "LATE") {
          lastCloudUpload = now; 
      }
  }

  // 6. LCD DISPLAY
  lcd.setCursor(0, 0); lcd.print("T:"); lcd.print(currentTemp, 1); lcd.print("C ");
  lcd.setCursor(11,0); lcd.print(currentTime);
  lcd.setCursor(0, 1); lcd.print(detectedStatus); lcd.print("      ");
}