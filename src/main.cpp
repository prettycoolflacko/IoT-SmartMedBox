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
//      USER CONFIGURATION
// ==========================================
// NOTE: WIFI_SSID and PASSWORD are removed. 
// They are now handled dynamically by WiFiManager.

// REPLACE WITH YOUR HTTPS DOMAIN
String API_URL = "https://flacko.fyuko.dev"; 
  
// Time Configuration (WIB = UTC+7)
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
const long intervalCloud = 5000; // Upload every 5 seconds if there are changes

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

// --- NEW LOGIC VARIABLES (Mr. P's Requirements) ---
// Box Open Timing
unsigned long boxOpenStartTime = 0;
unsigned long lastBoxOpenBuzzerToggle = 0;
bool boxOpenBuzzerState = false;

// Schedule Grace Period (1 minute after schedule)
bool scheduleGracePeriodActive = false;
unsigned long scheduleGracePeriodStart = 0;
const long scheduleGracePeriodDuration = 60000; // 1 minute

// Delays for LED and Buzzer
const long ledDelayAfterSchedule = 5000;    // 5 seconds
const long buzzerDelayAfterSchedule = 10000; // 10 seconds
const long ledDelayBoxOpen = 5000;           // 5 seconds
const long buzzerDelayBoxOpen = 10000;       // 10 seconds
const long buzzerToggleInterval = 5000;      // 5 seconds ON/OFF       

// ==========================================
//      HELPER FUNCTIONS
// ==========================================

// 1. Get Current Time (HH:MM)
String getLocalTimeStr() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) return "--:--";
  char timeStringBuff[6]; 
  strftime(timeStringBuff, sizeof(timeStringBuff), "%H:%M", &timeinfo);
  return String(timeStringBuff);
}

// 2. Fetch Schedules (USING HTTP CLIENT)
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
      
      // Parsing JSON using ArduinoJson
      // Doc capacity adjusted (1024 is enough for ~10 schedules)
      DynamicJsonDocument doc(2048); 
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        alarmSchedules.clear();
        JsonArray schedules = doc["schedules"];
        
        for (JsonObject s : schedules) {
          String timeStr = s["time"].as<String>();
          // Simple validation
          if (timeStr.length() == 5 && timeStr.indexOf(":") > 0) {
             alarmSchedules.push_back(timeStr);
             Serial.print(">> Schedule Found: "); Serial.println(timeStr);
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

// 3. Upload Status (USING HTTP CLIENT)
void uploadStatus(float temp, String status, String timeStr) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = API_URL + "/api/sensor";
    
    http.setUserAgent("ESP32");
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    
    // Create JSON String manually or use library
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
    Serial.println("Waiting for NTP Server...");
    delay(1000); 
  }
  Serial.println("Time Synchronized!");

  lcd.clear();
  
  // Fetch initial schedules
  fetchSchedules();
}

void loop() {
  // Update Time
  String currentTime = getLocalTimeStr();
  
  // =======================================================
  // 1. UPDATE SCHEDULE FROM VPS (Every 15 Seconds)
  // =======================================================
  if (millis() - lastScheduleFetch >= intervalFetch) {
    lastScheduleFetch = millis();
    fetchSchedules();
  }

  // =======================================================
  // 2. CHECK IF IT'S TIME TO TAKE MEDICINE?
  // =======================================================
  if (currentTime != lastTriggeredTime) { 
    for (String schedTime : alarmSchedules) {
      if (currentTime == schedTime) {
        isScheduleActive = true; 
        scheduleStartTime = millis(); 
        lastTriggeredTime = currentTime; 
        
        // Start 1-minute grace period
        scheduleGracePeriodActive = true;
        scheduleGracePeriodStart = millis();
        
        Serial.print("!!! MEDICINE TIME ARRIVED (");
        Serial.print(schedTime);
        Serial.println(") !!!");
        Serial.println("Grace period started - box open logic disabled for 1 minute");
      }
    }
  }
  
  // Check if grace period has ended
  if (scheduleGracePeriodActive) {
    if (millis() - scheduleGracePeriodStart >= scheduleGracePeriodDuration) {
      scheduleGracePeriodActive = false;
      Serial.println("Grace period ended - box open logic re-enabled");
    }
  }

  // =======================================================
  // 3. READ BOX STATUS
  // =======================================================
  // NOTE: Reed Switch logic may need to be reversed depending on wiring (INPUT_PULLUP vs external resistor)
  // Original code uses digitalRead(REED_PIN) == LOW for CLOSED.
  bool isPinHigh = digitalRead(REED_PIN); 
  
  if (isPinHigh == LOW) { // CLOSED 
    currentBoxStatus = "CLOSED";
    boxOpenStartTime = 0; // Reset timer when box is closed
    boxOpenBuzzerState = false; // Reset buzzer toggle state
  } else { // OPEN
    currentBoxStatus = "OPEN";
    
    // Start tracking how long box has been open
    if (boxOpenStartTime == 0) {
      boxOpenStartTime = millis();
    }
    
    if (isScheduleActive) {
      isScheduleActive = false; 
      scheduleGracePeriodActive = false; // End grace period when box opened
      Serial.println("Medicine Taken. Alarm Off.");
    }
  }

  // =======================================================
  // 4. LED LOGIC (Mr. P's Requirements)
  // =======================================================
  bool ledOn = false;
  String lcdMsg = "Status: " + currentBoxStatus;
  
  // LED Case 1: Box opened for more than 5 seconds (but NOT during grace period)
  if (currentBoxStatus == "OPEN" && !scheduleGracePeriodActive) {
    if (boxOpenStartTime > 0 && (millis() - boxOpenStartTime >= ledDelayBoxOpen)) {
      ledOn = true;
      lcdMsg = "BOX OPENED!";
    }
  }
  
  // LED Case 2: Temperature > 40°C and box is closed
  if (currentTemp > 40.0 && currentBoxStatus == "CLOSED") {
    ledOn = true;
    lcdMsg = "HIGH TEMP!";
  }
  
  // LED Case 3: Schedule arrived, 5 seconds passed, box not opened
  if (isScheduleActive) {
    long timePassed = millis() - scheduleStartTime;
    if (timePassed >= ledDelayAfterSchedule) {
      ledOn = true;
      lcdMsg = "MEDICINE TIME!";
    } else {
      lcdMsg = "Get ready...";
    }
  }

  // =======================================================
  // 5. BUZZER LOGIC (Mr. P's Requirements)
  // =======================================================
  bool buzzerOn = false;
  
  // Buzzer Case 1: Box opened for more than 10 seconds, toggle every 5s (but NOT during grace period)
  if (currentBoxStatus == "OPEN" && !scheduleGracePeriodActive) {
    if (boxOpenStartTime > 0 && (millis() - boxOpenStartTime >= buzzerDelayBoxOpen)) {
      // Toggle buzzer every 5 seconds
      if (millis() - lastBoxOpenBuzzerToggle >= buzzerToggleInterval) {
        boxOpenBuzzerState = !boxOpenBuzzerState;
        lastBoxOpenBuzzerToggle = millis();
      }
      buzzerOn = boxOpenBuzzerState;
    }
  }
  
  // Buzzer Case 2: Temperature > 40°C and box is closed
  // Turns OFF when: box opens OR temperature drops below 40°C
  if (currentTemp > 40.0 && currentBoxStatus == "CLOSED") {
    buzzerOn = true;
  }
  
  // Buzzer Case 3: Schedule arrived, 10 seconds passed, box not opened
  if (isScheduleActive) {
    long timePassed = millis() - scheduleStartTime;
    if (timePassed >= buzzerDelayAfterSchedule) {
      buzzerOn = true;
    }
  }

  // =======================================================
  // 6. HARDWARE EXECUTION
  // =======================================================
  if (ledOn) {
    digitalWrite(LED_PIN, HIGH);
  } else {
    digitalWrite(LED_PIN, LOW);
  }
  
  if (buzzerOn) {
    analogWrite(BUZZER_PIN, 150); 
  } else {
    analogWrite(BUZZER_PIN, 0);
  }

  // =======================================================
  // 7. LCD DISPLAY (Updates every 2 seconds with temperature)
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
  // 8. UPLOAD TO VPS
  // =======================================================
  bool shouldUpload = false;
  if (currentBoxStatus != lastSentStatus) shouldUpload = true;
  else if (millis() - lastCloudUpload >= intervalCloud) shouldUpload = true;

  if (shouldUpload) {
    uploadStatus(currentTemp, currentBoxStatus, currentTime);
  }
}