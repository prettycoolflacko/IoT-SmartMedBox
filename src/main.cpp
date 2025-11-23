#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>

// Provide the token generation process info.
#include "addons/TokenHelper.h"

// ==========================================
//      USER CONFIGURATION (EDIT THIS!)
// ==========================================

// 1. Wi-Fi Credentials
#define WIFI_SSID "YANGUTI"
#define WIFI_PASSWORD "fadaekalual"

// 2. Firebase Credentials
// Get these from Project Settings > General > Web App
#define API_KEY "AIzaSyBp81huxx0eq7glvtpeYcr6fJJQF1LMdRk"
#define FIREBASE_PROJECT_ID "smartmedicinebox-3f2df" 

// ==========================================
//      HARDWARE PIN DEFINITIONS
// ==========================================
#define DHT_PIN 18      
#define REED_PIN 5      
#define LED_PIN 13      
#define BUZZER_PIN 25   
#define DHT_TYPE DHT11

// ==========================================
//      GLOBAL OBJECTS & VARIABLES
// ==========================================
DHT dht(DHT_PIN, DHT_TYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2); 

// Firebase Objects
FirebaseData fbDO;
FirebaseAuth auth;
FirebaseConfig config;
bool signupOK = false;

// Timers
unsigned long lastDHTRead = 0;
const long intervalDHT = 2000; // Read sensor every 2s

unsigned long lastCloudUpload = 0;
const long intervalCloud = 15000; // Send to cloud every 15s (Save bandwidth/quota)

// Global variables to hold current state
float currentTemp = 0.0;
String currentBoxStatus = "UNKNOWN";
bool isAlarmActive = false;

void setup() {
  Serial.begin(115200);

  // 1. Init Hardware
  dht.begin();
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(REED_PIN, INPUT); 
  
  Wire.begin(); 
  lcd.init();
  lcd.backlight();

  // 2. Connect to Wi-Fi
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());

  lcd.setCursor(0, 1);
  lcd.print("WiFi OK!");
  delay(1000);

  // 3. Connect to Firebase
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Setup Firebase..");

  config.api_key = API_KEY;
  
  // Sign up anonymously so we can write data without login
  if (Firebase.signUp(&config, &auth, "", "")){
    Serial.println("Firebase Sign-up OK");
    signupOK = true;
  } else {
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  // Optimization settings
  config.token_status_callback = tokenStatusCallback; 
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  lcd.clear();
}

void loop() {
  // ==========================================
  // 1. LOGIC: ALARM (Real-time)
  // ==========================================
  bool reedStatus = digitalRead(REED_PIN); 
  
  // REED MODULE LOGIC: LOW = CLOSED (Magnet Near), HIGH = OPEN (Magnet Far)
  if (reedStatus == LOW) { 
    // CLOSED (SAFE) - Magnet menempel pada reed switch
    digitalWrite(LED_PIN, LOW);
    analogWrite(BUZZER_PIN, 0);
    currentBoxStatus = "CLOSED";
    isAlarmActive = false;
  } else { 
    // OPEN (WARNING) - Magnet jauh dari reed switch
    digitalWrite(LED_PIN, HIGH);
    analogWrite(BUZZER_PIN, 5); // Low volume
    currentBoxStatus = "OPEN";
    isAlarmActive = true;
  }

  // Update LCD Status (Bottom Line)
  lcd.setCursor(0, 1);
  lcd.print("Status: ");
  lcd.print(currentBoxStatus);
  lcd.print("   "); // Spaces to clear old text

  // ==========================================
  // 2. LOGIC: SENSOR READING (Every 2s)
  // ==========================================
  if (millis() - lastDHTRead >= intervalDHT) {
    lastDHTRead = millis();
    
    float t = dht.readTemperature();
    if (!isnan(t)) {
      currentTemp = t;
      // Update LCD Temp (Top Line)
      lcd.setCursor(0, 0);
      lcd.print("Temp: ");
      lcd.print(currentTemp, 1);
      lcd.print(" C   ");
    } else {
      lcd.setCursor(0, 0);
      lcd.print("Temp: Error     ");
    }
  }

  // ==========================================
  // 3. LOGIC: UPLOAD TO FIREBASE (Every 15s)
  // ==========================================
  if (Firebase.ready() && signupOK && (millis() - lastCloudUpload >= intervalCloud)) {
    lastCloudUpload = millis();
    
    Serial.println("Updating Firestore...");

    FirebaseJson content;
    // Kita set field yang ingin di-update
    content.set("fields/temperature/doubleValue", currentTemp);
    content.set("fields/status/stringValue", currentBoxStatus);

    // --- PERUBAHAN UTAMA DISINI ---
    // Alih-alih upload ke folder "sensor_logs" secara acak,
    // Kita tembak ke file spesifik: "sensor_logs/device_1"
    
    // Path: projects/{project_id}/databases/(default)/documents/{collection_id}/{document_id}
    String documentPath = "sensor_logs/device_1"; 

    // Gunakan patchDocument untuk update/menimpa data
    // Parameter terakhir adalah updateMask (kosongkan "" agar update semua field di content)
    if (Firebase.Firestore.patchDocument(&fbDO, FIREBASE_PROJECT_ID, "", documentPath.c_str(), content.raw(), "")) {
        Serial.println("Update Success! (Realtime)");
    } else {
        Serial.println("Update Failed");
        Serial.println(fbDO.errorReason());
    }
  }
}