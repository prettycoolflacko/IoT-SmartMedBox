# Smart Medicine Box with Scheduled Reminders (ESP32)

An intelligent ESP32-based medicine box system that monitors temperature, detects box opening/closing events, displays real-time information on an LCD, and provides scheduled medication reminders with alarm functionality. The system integrates with a remote server for schedule management and data logging.

---

## 🌟 Key Features

### Sensor Monitoring
- **Temperature Tracking**: Real-time temperature monitoring using DHT11 sensor
- **Box Status Detection**: Magnetic reed switch detects when the medicine box is opened or closed
- **Visual Feedback**: 16x2 I2C LCD displays temperature, time, and box status

### Smart Alarm System
- **Scheduled Reminders**: Fetches medication schedules from a remote server
- **Intelligent Alerts**: 
  - Activates alarm when it's time to take medication (with 5-second grace period)
  - Triggers warning if box is opened outside scheduled times (after 5-second delay)
  - Automatically silences when medication is taken (box opened during active schedule)
- **Multi-Modal Alerts**: LED indicator and buzzer for visual and audible notifications

### Cloud Integration
- **Remote Schedule Management**: Fetches medication schedules from REST API server
- **Real-time Data Upload**: Sends temperature, box status, and timestamps to server
- **NTP Time Synchronization**: Accurate timekeeping (WIB/UTC+7 timezone)

---

## 🛠️ Hardware Requirements

### Components
- ESP32 Development Board
- DHT11 Temperature & Humidity Sensor
- Reed Switch (Magnetic Sensor)
- Magnet (for reed switch)
- 16x2 I2C LCD Display (Address: 0x27)
- Buzzer (PWM-capable)
- LED Indicator
- Resistors (as needed)
- Breadboard and Jumper Wires
- Power Supply (USB or battery)

### Pin Configuration

| Component | GPIO Pin | Description |
|-----------|----------|-------------|
| DHT11 Data | GPIO 18 | Temperature sensor data |
| Reed Switch | GPIO 5 | Box open/close detection |
| LED | GPIO 13 | Visual alarm indicator |
| Buzzer | GPIO 25 | Audible alarm (PWM) |
| LCD SDA | GPIO 21 | I2C data (default) |
| LCD SCL | GPIO 22 | I2C clock (default) |

**Note**: You can modify pin assignments in `src/main.cpp` if needed.

---

## 📚 Software Dependencies

### PlatformIO Configuration
This project uses PlatformIO for build management. All dependencies are configured in `platformio.ini`.

### Required Libraries
- **DHT sensor library** - Temperature/humidity sensor support
- **Adafruit Unified Sensor** - Sensor abstraction layer
- **LiquidCrystal_I2C** - I2C LCD control
- **ArduinoJson** (v7.4.2+) - JSON parsing for API responses
- **HTTPClient** - REST API communication (built-in ESP32)
- **WiFi** - Network connectivity (built-in ESP32)

Libraries are automatically installed via PlatformIO. Manual installation:
```bash
pio lib install "DHT sensor library"
pio lib install "Adafruit Unified Sensor"
pio lib install "LiquidCrystal_I2C"
pio lib install "ArduinoJson"
```

---

## ⚙️ Configuration

### 1. Wi-Fi Settings
Edit the following in `src/main.cpp`:
```cpp
#define WIFI_SSID "Your_WiFi_SSID"
#define WIFI_PASSWORD "Your_WiFi_Password"
```

### 2. Server API Endpoint
Set your server URL (without exposing in code repositories):
```cpp
String API_URL = "http://YOUR_SERVER_IP:PORT";
```

**Important**: Use environment variables or a separate config file for production deployments.

### 3. Hardware Pin Customization
Adjust pin definitions if using different GPIO pins:
```cpp
#define DHT_PIN 18      // DHT11 data pin
#define REED_PIN 5      // Reed switch input
#define LED_PIN 13      // LED indicator
#define BUZZER_PIN 25   // Buzzer output
```

### 4. I2C LCD Address
Default address is `0x27`. Verify your LCD address using an I2C scanner if display doesn't work:
```cpp
LiquidCrystal_I2C lcd(0x27, 16, 2);
```

---

## 🚀 Building and Uploading

### Using PlatformIO CLI
```bash
# Build the project
pio run

# Upload to ESP32 (ensure board is connected)
pio run -t upload

# Monitor serial output
pio device monitor -b 115200

# Build, upload, and monitor in one command
pio run -t upload && pio device monitor
```

### Using PlatformIO IDE (VS Code)
1. Open the project folder in VS Code
2. Click the **Build** button (✓) in the PlatformIO toolbar
3. Click the **Upload** button (→) to flash the ESP32
4. Click the **Serial Monitor** button (🔌) to view output

### Upload Port Configuration
Default port is `/dev/ttyUSB0` (Linux). Modify in `platformio.ini` if different:
```ini
upload_port = /dev/ttyUSB0
monitor_port = /dev/ttyUSB0
```

---

## 📡 Server API Requirements

The ESP32 communicates with a REST API server. Your server should implement these endpoints:

### GET `/api/data`
Retrieves medication schedules.

**Response Format**:
```json
{
  "schedules": [
    { "time": "08:00" },
    { "time": "14:00" },
    { "time": "20:00" }
  ]
}
```

### POST `/api/sensor`
Receives sensor data from ESP32.

**Request Format**:
```json
{
  "temp": 25.5,
  "status": "CLOSED",
  "last_update": "14:30"
}
```

---

## 🔍 System Behavior

### Startup Sequence
1. Initializes hardware (LCD, sensors, pins)
2. Connects to Wi-Fi network
3. Synchronizes time with NTP server (pool.ntp.org)
4. Fetches initial medication schedules from server
5. Enters main monitoring loop

### Main Operation Loop

#### Sensor Reading (Every 2 seconds)
- Reads temperature from DHT11
- Updates LCD with temperature and current time
- Detects box open/close status via reed switch

#### Schedule Management (Every 15 seconds)
- Fetches updated medication schedules from server
- Updates internal schedule list

#### Alarm Logic
**Scenario A - Scheduled Medication Time**:
- System checks if current time matches any scheduled time
- After 5-second grace period, activates alarm (LED + buzzer)
- Displays "WAKTUNYA OBAT!" (Time for medication!)
- Alarm stops when box is opened

**Scenario B - Unauthorized Opening**:
- If box is opened outside scheduled times
- After 5-second delay, triggers warning alarm
- Displays "BOX DIBUKA!" (Box opened!)
- Stops when box is closed

#### Cloud Synchronization (Every 5 seconds or on status change)
- Uploads temperature, box status, and timestamp to server
- Only uploads when data changes or interval expires

### LCD Display Format
```
Line 1: T:25.5C      14:30
Line 2: Status: CLOSED
```

During alarms, Line 2 shows alarm messages.

---

## 🐛 Troubleshooting

### Wi-Fi Connection Issues
- **Symptom**: ESP32 stuck on "WiFi Connecting"
- **Solutions**:
  - Verify SSID and password are correct
  - Check Wi-Fi signal strength
  - Ensure router allows new device connections
  - Try 2.4GHz network (ESP32 doesn't support 5GHz)

### LCD Not Working
- **Symptom**: Blank screen or garbage characters
- **Solutions**:
  - Verify I2C address using I2C scanner sketch
  - Check SDA/SCL connections (GPIO 21/22 by default)
  - Adjust LCD contrast potentiometer
  - Test with a simple I2C scanner code first

### DHT11 Reading Errors
- **Symptom**: Temperature shows 0.0 or NaN
- **Solutions**:
  - Check data pin connection (GPIO 18)
  - Ensure DHT11 has proper power (3.3V or 5V)
  - Add 10kΩ pull-up resistor between data and VCC
  - Allow 2+ seconds between readings

### Reed Switch Logic Inverted
- **Current Logic**: `LOW` = CLOSED (magnet near), `HIGH` = OPEN (magnet away)
- **If your wiring is opposite**:
  - Option 1: Swap magnet position
  - Option 2: Change code logic: `if (isPinHigh == HIGH)` for CLOSED
  - Option 3: Use `INPUT_PULLUP` and adjust accordingly

### Server Connection Failures
- **Symptom**: "Error Fetching Data" or "Error Sending Data"
- **Solutions**:
  - Verify server is running and accessible
  - Check firewall rules on server
  - Test API endpoints using curl or Postman
  - Ensure ESP32 and server are on same network (or server is publicly accessible)
  - Check server logs for incoming requests

### Time Synchronization Issues
- **Symptom**: Time shows "--:--" or incorrect time
- **Solutions**:
  - Verify internet connection
  - Check NTP server accessibility (pool.ntp.org)
  - Adjust GMT offset if in different timezone
  - Wait longer for initial sync (can take 10-30 seconds)

### Serial Monitor Shows No Output
- **Solutions**:
  - Ensure baud rate is set to 115200
  - Check USB cable (use data cable, not charge-only)
  - Try different USB port
  - Install/update CH340/CP2102 drivers for ESP32

---

## 📂 Project Structure

```
ESP/
├── platformio.ini          # PlatformIO configuration
├── README.md              # This file
├── include/               # Header files (if any)
├── lib/                   # Custom libraries
├── src/
│   └── main.cpp          # Main application code
└── test/                  # Test files
```

---

## 🔒 Security Considerations

1. **Credentials**: Never commit Wi-Fi passwords or server IPs to version control
2. **API Security**: Implement authentication on your server endpoints
3. **Network**: Use HTTPS/TLS for production deployments
4. **Data Privacy**: Encrypt sensitive health data in transit and at rest

---

## 🚧 Future Enhancements

- [ ] HTTPS/TLS support for secure communications
- [ ] Multiple user profiles with different schedules
- [ ] Battery level monitoring for portable operation
- [ ] Medicine quantity tracking
- [ ] Mobile app integration
- [ ] E-mail/SMS notifications
- [ ] Historical data analytics dashboard
- [ ] OTA (Over-The-Air) firmware updates

---

## 📄 License

This project is provided as-is for educational and personal use.

---

## 👤 Author

Created as part of an IoT smart medicine management system project.

---

## 🤝 Contributing

Contributions, issues, and feature requests are welcome! Feel free to check issues page.

---

## 📞 Support

For questions or issues:
1. Check the Troubleshooting section above
2. Review PlatformIO documentation: https://docs.platformio.org
3. ESP32 documentation: https://docs.espressif.com

---

**Last Updated**: November 2025
- Firebase upload fails: check Wi‑Fi credentials, `API_KEY` and `FIREBASE_PROJECT_ID`, and ensure your Firebase rules allow the write (anonymous sign-up is used in the code). See serial monitor for error reasons.

## Next steps / Improvements

- Move sensitive config (API key, Wi‑Fi) to `platformio.ini` or a separate config header.
- Add debounce for reed switch if you see jitter when opening/closing.
- Add OTA updates or web UI.
- Add retries and exponential backoff for cloud uploads.

## License

This project is provided as-is. Feel free to reuse and adapt the code for personal projects.

---

Dokumentasi ringkas (Bahasa Indonesia)

- Proyek ini menggunakan ESP32 untuk membaca sensor DHT11, mendeteksi reed switch (magnet), menampilkan ke LCD I2C, dan mengirim data ke Firebase.
- Logika reed: `LOW` = TERTUTUP (magnet menempel), `HIGH` = TERBUKA (magnet menjauh).
- Sesuaikan kredensial Wi‑Fi dan Firebase di `src/main.cpp` sebelum mengunggah.

