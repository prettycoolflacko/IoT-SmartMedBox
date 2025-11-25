# Smart Medicine Box (ESP32)

An ESP32-based smart medicine box project that reads temperature (DHT11), detects box open/close with a reed switch (magnet), displays status on a 16x2 I2C LCD, sounds a buzzer and lights an LED for alarms, and uploads logs to Firebase Firestore.

This README provides wiring, configuration, build and troubleshooting steps.

---

## Key features

- Reads temperature using a DHT11 sensor and shows it on a 16x2 I2C LCD.
- Detects box open/close status using a reed switch (magnet).
- Local alarm: LED indicator + buzzer when box is open.
- Periodically uploads logs (temperature, status, timestamp) to Firebase Firestore.

## Important Reed Switch Logic

- NOTE: In this project the reed switch is wired so that:
	- `LOW` = CLOSED (magnet present / box closed)
	- `HIGH` = OPEN (magnet absent / box open)

	This is the opposite of some examples. If your sensor behaves differently, swap the logic or wiring accordingly.

## Hardware

- ESP32 (or compatible board)
- DHT11 temperature sensor
- Reed switch (magnet)
- 16x2 I2C LCD (LiquidCrystal_I2C)
- Buzzer (PWM-capable pin)
- LED (indicator)
- Jumper wires, breadboard, magnet

### Pin mapping (as used in `src/main.cpp`)

- DHT11 data: GPIO 18 (`DHT_PIN`)
- Reed switch input: GPIO 5 (`REED_PIN`)
- LED indicator: GPIO 13 (`LED_PIN`)
- Buzzer (PWM): GPIO 25 (`BUZZER_PIN`)
- LCD: I2C (default address 0x27)

Adjust pins in `src/main.cpp` if you use different wiring.

## Software / Libraries

This project is intended to be built with PlatformIO.

Required libraries (install via PlatformIO Library Manager or pio CLI):

- DHT sensor library (or use the Arduino DHT library used in code)
- LiquidCrystal_I2C
- Firebase_ESP_Client

You can install them from the PlatformIO IDE or using the CLI. Example:

```bash
pio lib install "DHT sensor library"
pio lib install "LiquidCrystal I2C"
pio lib install "Firebase_ESP_Client"
```

Note: exact library names may vary. Use PlatformIO Library Manager search if needed.

## Configuration

1. Open `src/main.cpp` and set your Wi‑Fi and Firebase credentials at the top:

- `WIFI_SSID` and `WIFI_PASSWORD` — your Wi‑Fi network
- `API_KEY` and `FIREBASE_PROJECT_ID` — values from your Firebase project

2. Optionally change pin defines if your wiring differs.

3. Verify I2C LCD address (0x27 is common). Use an I2C scanner sketch if the LCD does not respond.

## Build & Upload (PlatformIO)

From the project root (where `platformio.ini` is located):

```bash
# Build and upload to the board
pio run -t upload

# Open serial monitor (set the correct baud, 115200 by default)
pio device monitor -b 115200
```

If you prefer the PlatformIO IDE, use the "Build", "Upload" and "Monitor" buttons.

## Runtime behavior

- On boot the LCD shows status and temperature.
- The code reads the DHT11 every 2 seconds and updates the LCD.
- Reed switch is polled continuously; when open, LED lights and buzzer provides a short alarm.
- Every 15 seconds (configurable) the project attempts to upload a log (temperature, status, timestamp) to Firestore.

## Troubleshooting

- LCD shows garbage / blank: confirm I2C address and wiring, try an I2C scanner.
- DHT readings are `nan` or `Error`: confirm DHT wiring and power; try longer stabilization time.
- Reed switch logic opposite of expected: either invert logic in code or change wiring/pull-up configuration. Remember this project expects `LOW` when closed (magnet present).
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

