#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <WiFi.h>
#include <HTTPClient.h> // PENGGANTI FIREBASE
#include <ArduinoJson.h> // WAJIB INSTALL: Library "ArduinoJson" by Benoit Blanchon
#include "time.h"
#include <vector>
#include <WiFiManager.h> // NEW LIBRARY

// ==========================================
//      KONFIGURASI USER
// ==========================================
// NOTE: WIFI_SSID and PASSWORD are removed. 
// They are now handled dynamically by WiFiManager.

// GANTI DENGAN DOMAIN HTTPS ANDA
String API_URL = "https://flacko.fyuko.dev"; 
  
// Konfigurasi Waktu (WIB = UTC+7)
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 25200; 
const int   daylightOffset_sec = 0; 

// Hardware Pins
#define DHT_PIN 18      
#define REED_PIN 5      
#define LED_PIN 13      
#define BUZZER_PIN 25   
#define DHT_TYPE DHT11

// ==========================================
//      GLOBAL OBJECTS
// ==========================================
DHT dht(DHT_PIN, DHT_TYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2); 

// Timers
unsigned long lastDHTRead = 0;
const long intervalDHT = 2000;
unsigned long lastCloudUpload = 0;
const long intervalCloud = 5000; // Upload tiap 5 detik jika ada perubahan

// Schedule Timers
unsigned long lastScheduleFetch = 0;
const long intervalFetch = 15000; 

// Status Variables
float currentTemp = 0.0;
String currentBoxStatus = "UNKNOWN";
String lastSentStatus = "UNKNOWN";

// --- ALARM LOGIC VARIABLES ---
unsigned long doorOpenStartTime = 0; 
const long alarmDelay = 5000;       

// Schedule Logic
std::vector<String> alarmSchedules; 
bool isScheduleActive = false;       
unsigned long scheduleStartTime = 0; 
String lastTriggeredTime = "";

// --- NEW LOGIC VARIABLES ---
// Temperature alarm logic
unsigned long lastTempBuzzerToggle = 0;
bool tempBuzzerState = false;
const long tempBuzzerInterval = 5000; // 5 seconds on/off interval

// Schedule LED logic
unsigned long scheduleLedStartTime = 0;
bool scheduleLedActive = false;
const long scheduleLedDuration = 20000; // 20 seconds

// Schedule buzzer delay logic
unsigned long scheduleBuzzerDelayStart = 0;
unsigned long scheduleBuzzerStartTime = 0;
bool scheduleBuzzerDelayActive = false;
bool scheduleBuzzerActive = false;
const long scheduleBuzzerDelay = 15000; // 15 seconds delay
const long scheduleBuzzerDuration = 15000; // 15 seconds buzzer       

// ==========================================
//      HELPER FUNCTIONS
// ==========================================

// 1. Ambil Jam Saat Ini (HH:MM)
String getLocalTimeStr() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) return "--:--";
  char timeStringBuff[6]; 
  strftime(timeStringBuff, sizeof(timeStringBuff), "%H:%M", &timeinfo);
  return String(timeStringBuff);
}

// 2. Fetch Schedules (MENGGUNAKAN HTTP CLIENT)
void fetchSchedules() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = API_URL + "/api/data";
    
    // Add User-Agent to prevent 403 errors on some servers
    http.setUserAgent("ESP32");
    http.begin(url);
    
    int httpResponseCode = http.GET();
    
    if (httpResponseCode == 200) {
      String payload = http.getString();
      
      // Parsing JSON menggunakan ArduinoJson
      // Kapasitas doc disesuaikan (1024 cukup untuk ~10 jadwal)
      DynamicJsonDocument doc(2048); 
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        alarmSchedules.clear();
        JsonArray schedules = doc["schedules"];
        
        for (JsonObject s : schedules) {
          String timeStr = s["time"].as<String>();
          // Validasi sederhana
          if (timeStr.length() == 5 && timeStr.indexOf(":") > 0) {
             alarmSchedules.push_back(timeStr);
             Serial.print(">> Jadwal Ditemukan: "); Serial.println(timeStr);
          }
        }
      } else {
        Serial.print("JSON Parse Error: "); Serial.println(error.c_str());
      }
    } else {
      Serial.print("Error Fetching Data: "); Serial.println(httpResponseCode);
    }
    http.end();
  }
}

// 3. Upload Status (MENGGUNAKAN HTTP CLIENT)
void uploadStatus(float temp, String status, String timeStr) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = API_URL + "/api/sensor";
    
    http.setUserAgent("ESP32");
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    
    // Buat JSON String manual atau pakai library
    String jsonPayload = "{\"temp\": " + String(temp, 1) + 
                         ", \"status\": \"" + status + "\"" +
                         ", \"last_update\": \"" + timeStr + "\"}";
                         
    int httpResponseCode = http.POST(jsonPayload);
    
    if (httpResponseCode > 0) {
       lastCloudUpload = millis();
       lastSentStatus = status;
       Serial.println("Data sent to VPS successfully");
    } else {
       Serial.print("Error Sending Data: "); Serial.println(httpResponseCode);
    }
    http.end();
  }
}

void setup() {
  Serial.begin(115200);

  // Init Hardware
  dht.begin();
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(REED_PIN, INPUT); 
  
  digitalWrite(LED_PIN, LOW);
  analogWrite(BUZZER_PIN, 0);

  Wire.begin(); 
  lcd.init();
  lcd.backlight();    

  // ==========================================
  // WIFI MANAGER SETUP
  // ==========================================
  WiFiManager wm;
  
  // Uncomment line below if you want to erase saved wifi for testing
  // wm.resetSettings(); 

  lcd.setCursor(0, 0); 
  lcd.print("WiFi Setup Mode");
  lcd.setCursor(0, 1);
  lcd.print("Connect to AP");

  // This creates an Access Point named "SmartMedicine-Setup"
  // If connection fails, it pauses here until you connect via phone
  bool res = wm.autoConnect("SmartMedicine-Setup"); 

  if(!res) {
      Serial.println("Failed to connect");
      // ESP.restart();
  } else {
      Serial.println("connected...yeey :)");
      lcd.clear();
      lcd.setCursor(0, 0); 
      lcd.print("WiFi Connected!");
      delay(2000);
  }

  // Sync Time
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Sync Time...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  while(!getLocalTime(&timeinfo)){ 
    Serial.println("Menunggu NTP Server...");
    delay(1000); 
  }
  Serial.println("Waktu Tersinkron!");

  lcd.clear();
  
  // Ambil jadwal awal
  fetchSchedules();
}

void loop() {
  // Update Jam
  String currentTime = getLocalTimeStr();
  
  // =======================================================
  // 1. UPDATE JADWAL DARI VPS (Tiap 15 Detik)
  // =======================================================
  if (millis() - lastScheduleFetch >= intervalFetch) {
    lastScheduleFetch = millis();
    fetchSchedules();
  }

  // =======================================================
  // 2. CEK APAKAH SEKARANG WAKTUNYA MINUM OBAT?
  // =======================================================
  if (currentTime != lastTriggeredTime) { 
    for (String schedTime : alarmSchedules) {
      if (currentTime == schedTime) {
        isScheduleActive = true; 
        scheduleStartTime = millis(); 
        lastTriggeredTime = currentTime; 
        
        // NEW LOGIC: Activate LED for 20 seconds
        scheduleLedActive = true;
        scheduleLedStartTime = millis();
        
        // NEW LOGIC: Start 15 second delay before buzzer
        scheduleBuzzerDelayActive = true;
        scheduleBuzzerDelayStart = millis();
        scheduleBuzzerActive = false;
        
        Serial.print("!!! WAKTU OBAT TIBA (");
        Serial.print(schedTime);
        Serial.println(") !!!");
      }
    }
  }

  // =======================================================
  // 3. BACA STATUS KOTAK
  // =======================================================
  // NOTE: Logika Reed Switch mungkin perlu dibalik tergantung wiring (INPUT_PULLUP vs resistor eksternal)
  // Kode asli Anda menggunakan digitalRead(REED_PIN) == LOW untuk CLOSED.
  bool isPinHigh = digitalRead(REED_PIN); 
  
  if (isPinHigh == LOW) { // TERTUTUP 
    currentBoxStatus = "CLOSED";
  } else { // TERBUKA
    currentBoxStatus = "OPEN";
    if (isScheduleActive) {
      isScheduleActive = false; 
      Serial.println("Obat Diambil. Alarm Mati.");
    }
    
    // NEW LOGIC: Turn off temperature alarm when box is opened
    tempBuzzerState = false;
    lastTempBuzzerToggle = millis();
  }

  // =======================================================
  // 4. NEW LOGIC: TEMPERATURE ALARM (Above 40°C)
  // =======================================================
  bool tempBuzzerOn = false;
  if (currentTemp > 40.0 && currentBoxStatus == "CLOSED") {
    // Toggle buzzer every 5 seconds
    if (millis() - lastTempBuzzerToggle >= tempBuzzerInterval) {
      tempBuzzerState = !tempBuzzerState;
      lastTempBuzzerToggle = millis();
    }
    tempBuzzerOn = tempBuzzerState;
  }

  // =======================================================
  // 5. NEW LOGIC: SCHEDULE LED (20 seconds)
  // =======================================================
  bool scheduleLedOn = false;
  if (scheduleLedActive) {
    if (millis() - scheduleLedStartTime >= scheduleLedDuration) {
      scheduleLedActive = false;
    } else {
      scheduleLedOn = true;
    }
  }

  // =======================================================
  // 6. NEW LOGIC: SCHEDULE BUZZER (15s delay + 15s on)
  // =======================================================
  bool scheduleBuzzerOn = false;
  if (scheduleBuzzerDelayActive) {
    if (millis() - scheduleBuzzerDelayStart >= scheduleBuzzerDelay) {
      scheduleBuzzerDelayActive = false;
      scheduleBuzzerActive = true;
      scheduleBuzzerStartTime = millis();
    }
  }
  
  if (scheduleBuzzerActive) {
    if (millis() - scheduleBuzzerStartTime >= scheduleBuzzerDuration) {
      scheduleBuzzerActive = false;
    } else {
      scheduleBuzzerOn = true;
    }
  }

  // =======================================================
  // 7. LOGIKA ALARM PINTAR (OLD LOGIC)
  // =======================================================
  bool buzzerOn = false;
  String lcdMsg = "Status: " + currentBoxStatus;

  // -- SKENARIO A: ALARM JADWAL --
  if (isScheduleActive) {
    long timePassed = millis() - scheduleStartTime;
    if (timePassed > alarmDelay) {
      buzzerOn = true;
      lcdMsg = "WAKTUNYA OBAT!";
    } else {
      lcdMsg = "Siap-siap...";
    }
  } 
  // -- SKENARIO B: KOTAK DIBUKA PAKSA --
  else if (currentBoxStatus == "OPEN") {
    if (doorOpenStartTime == 0) doorOpenStartTime = millis();
    if (millis() - doorOpenStartTime > alarmDelay) {
      buzzerOn = true;
      lcdMsg = "BOX DIBUKA!";
    }
  } else {
    doorOpenStartTime = 0;
  }

  // =======================================================
  // 8. EKSEKUSI HARDWARE (Combined Old + New Logic)
  // =======================================================
  // Combine all buzzer conditions (old logic OR new logic)
  bool finalBuzzerOn = buzzerOn || tempBuzzerOn || scheduleBuzzerOn;
  
  // Combine all LED conditions (old logic OR new schedule LED)
  bool finalLedOn = buzzerOn || scheduleLedOn;
  
  if (finalBuzzerOn) {
    analogWrite(BUZZER_PIN, 150); 
  } else {
    analogWrite(BUZZER_PIN, 0);
  }
  
  if (finalLedOn) {
    digitalWrite(LED_PIN, HIGH);
  } else {
    digitalWrite(LED_PIN, LOW);
  }
  
  // Update LCD message for high temperature
  if (currentTemp > 40.0 && currentBoxStatus == "CLOSED") {
    lcdMsg = "HIGH TEMP!";
  }

  // =======================================================
  // 9. TAMPILAN LCD
  // =======================================================
  if (millis() - lastDHTRead >= intervalDHT) {
    lastDHTRead = millis();
    float t = dht.readTemperature();
    if (!isnan(t)) currentTemp = t;

    lcd.setCursor(0, 0);
    lcd.print("T:"); lcd.print(currentTemp, 1); lcd.print("C ");
    lcd.setCursor(11, 0); lcd.print(currentTime); 
    
    lcd.setCursor(0, 1);
    lcd.print(lcdMsg);
    lcd.print("     "); 
  }

  // =======================================================
  // 10. UPLOAD KE VPS
  // =======================================================
  bool shouldUpload = false;
  if (currentBoxStatus != lastSentStatus) shouldUpload = true;
  else if (millis() - lastCloudUpload >= intervalCloud) shouldUpload = true;

  if (shouldUpload) {
    uploadStatus(currentTemp, currentBoxStatus, currentTime);
  }
}