# 📷 Smart Doorbell Camera (ESP32-CAM)

A smart IoT-based doorbell system built using the **ESP32-CAM**, designed to detect visitors and instantly send their photo to your Telegram account. This project combines motion detection, distance sensing, and manual triggering for a complete smart doorbell solution.

---

## 🚀 Features

* 📸 Captures and sends images via Telegram
* 🚶 Motion detection using PIR sensor
* 📏 Distance confirmation using ultrasonic sensor
* 🔔 Manual doorbell trigger with push button
* 🔊 Buzzer alert system
* 💡 LED indicators for system status

---

## 🧰 Hardware Requirements

* ESP32-CAM (AI Thinker module)
* PIR Sensor (HC-SR501)
* Ultrasonic Sensor (HC-SR04)
* Push Button
* Active Buzzer
* LEDs (Red + Green) with 220Ω resistors
* FTDI Programmer (for uploading code)
* Jumper wires, breadboard

---

## 🔌 Pin Configuration

| Component       | GPIO Pin |
| --------------- | -------- |
| PIR Sensor      | 13       |
| Ultrasonic TRIG | 12       |
| Ultrasonic ECHO | 14       |
| Push Button     | 15       |
| Buzzer          | 2        |
| Green LED       | 4        |
| Red LED         | 16       |

---

## ⚙️ Setup Instructions

### 1. Install Dependencies

* Install **ESP32 Board Package** via Arduino IDE
* Install library:

  * `ArduinoJson (v6.x)`

### 2. Configure Credentials

Update these in the code:

```cpp
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* BOT_TOKEN     = "YOUR_TELEGRAM_BOT_TOKEN";
const char* CHAT_ID       = "YOUR_TELEGRAM_CHAT_ID";
```

### 3. Upload Code to ESP32-CAM

1. Connect FTDI:

   * TX → RX
   * RX → TX
   * 5V → 5V
   * GND → GND
2. Connect **GPIO 0 → GND** (enter flash mode)
3. Upload code via Arduino IDE
4. Remove GPIO 0 jumper
5. Press **RST** to run

---

## 🤖 Telegram Setup

1. Open Telegram and search for **@BotFather**
2. Create a bot → get `BOT_TOKEN`
3. Open **@userinfobot** → get your `CHAT_ID`
4. Paste both into the code

---

## 🧠 How It Works

* PIR sensor detects motion
* Ultrasonic sensor confirms proximity
* OR user presses doorbell button
* ESP32-CAM captures an image
* Image is sent to your Telegram chat
* Buzzer and LEDs indicate activity

---

## 📂 Project Structure

```
smart-doorbell/
│── smart_doorbell.ino   # Main Arduino code
│── README.md            # Project documentation
```

---

## 🛠️ Future Improvements

* Face recognition
* Cloud storage integration
* Mobile app instead of Telegram
* Battery optimization / sleep modes

---

## 📜 License

This project is open-source and free to use for educational purposes.

---

## 🙌 Acknowledgements

* Espressif Systems (ESP32)
* Arduino Community
* Telegram Bot API

---

## 💡 Tip

Make sure you're powering the ESP32-CAM with a **stable 5V supply** — USB alone sometimes isn’t enough.

---

If you want, I can also make this look more “GitHub pro” with badges, screenshots, and diagrams.
