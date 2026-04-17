/*
 * ============================================================
 *  SMART DOORBELL CAMERA — ESP32-CAM  (MPCA Project)
 * ============================================================
 *  Controller : AI-Thinker ESP32-CAM (ESP32 + OV2640)
 *
 *  Sensors    :
 *    1. PIR HC-SR501   → GPIO 13  (motion detection)
 *    2. HC-SR04        → GPIO 12/14 (proximity confirmation)
 *    3. Push Button    → GPIO 15  (manual doorbell trigger)
 *
 *  Outputs    :
 *    • Active Buzzer   → GPIO 2
 *    • Green LED       → GPIO 4  (via 220Ω) — also onboard flash
 *    • Red LED         → GPIO 16 (via 220Ω)
 *
 *  Feature    : Sends JPEG photo to Telegram on every alert
 *
 *  Libraries (install via Arduino Library Manager):
 *    ▸ ESP32 board package by Espressif Systems
 *    ▸ ArduinoJson by Benoit Blanchon (v6.x)
 *
 *  How to flash:
 *    1. Connect FTDI to ESP32-CAM (TX→RX, RX→TX, GND, 5V)
 *    2. Short GPIO 0 to GND before powering on  → flash mode
 *    3. Upload sketch
 *    4. Remove GPIO 0 jumper, press RST → run mode
 * ============================================================
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "esp_camera.h"

// ─────────────────────────────────────────────────────────────
//  CONFIGURATION  ← edit these before uploading
// ─────────────────────────────────────────────────────────────
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* BOT_TOKEN     = "YOUR_TELEGRAM_BOT_TOKEN";  // from @BotFather
const char* CHAT_ID       = "YOUR_TELEGRAM_CHAT_ID";    // from @userinfobot

// ─────────────────────────────────────────────────────────────
//  PIN DEFINITIONS
// ─────────────────────────────────────────────────────────────
#define PIR_PIN      13   // PIR HC-SR501 signal output
#define TRIG_PIN     12   // HC-SR04 trigger
#define ECHO_PIN     14   // HC-SR04 echo
#define BTN_PIN      15   // Push button (10 kΩ pull-down to GND)
#define BUZZER_PIN    2   // Active buzzer (+) pin
#define GREEN_LED     4   // Green LED anode — via 220 Ω resistor
#define RED_LED      16   // Red LED anode  — via 220 Ω resistor

// ─────────────────────────────────────────────────────────────
//  CAMERA PINS  — AI-Thinker module, do NOT change
// ─────────────────────────────────────────────────────────────
#define PWDN_GPIO_NUM   32
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM    0
#define SIOD_GPIO_NUM   26
#define SIOC_GPIO_NUM   27
#define Y9_GPIO_NUM     35
#define Y8_GPIO_NUM     34
#define Y7_GPIO_NUM     39
#define Y6_GPIO_NUM     36
#define Y5_GPIO_NUM     21
#define Y4_GPIO_NUM     19
#define Y3_GPIO_NUM     18
#define Y2_GPIO_NUM      5
#define VSYNC_GPIO_NUM  25
#define HREF_GPIO_NUM   23
#define PCLK_GPIO_NUM   22

// ─────────────────────────────────────────────────────────────
//  TUNING PARAMETERS
// ─────────────────────────────────────────────────────────────
#define PROXIMITY_CM       150   // alert only if visitor within 150 cm
#define COOLDOWN_MS       8000   // minimum gap between consecutive alerts
#define BUZZ_SHORT_MS     1000   // buzzer on-time for motion alert
#define BUZZ_LONG_MS      2000   // buzzer on-time for button press
#define PIR_DEBOUNCE_MS    400   // ignore rapid PIR re-triggers (ms)

// ─────────────────────────────────────────────────────────────
//  GLOBALS
// ─────────────────────────────────────────────────────────────
WiFiClientSecure secureClient;
unsigned long    lastAlertMs = 0;
unsigned long    lastPIRMs   = 0;
bool             camReady    = false;


// =============================================================
//  CAMERA INITIALISATION
// =============================================================
bool initCamera() {
  camera_config_t cfg;

  cfg.ledc_channel  = LEDC_CHANNEL_0;
  cfg.ledc_timer    = LEDC_TIMER_0;
  cfg.pin_d0        = Y2_GPIO_NUM;
  cfg.pin_d1        = Y3_GPIO_NUM;
  cfg.pin_d2        = Y4_GPIO_NUM;
  cfg.pin_d3        = Y5_GPIO_NUM;
  cfg.pin_d4        = Y6_GPIO_NUM;
  cfg.pin_d5        = Y7_GPIO_NUM;
  cfg.pin_d6        = Y8_GPIO_NUM;
  cfg.pin_d7        = Y9_GPIO_NUM;
  cfg.pin_xclk      = XCLK_GPIO_NUM;
  cfg.pin_pclk      = PCLK_GPIO_NUM;
  cfg.pin_vsync     = VSYNC_GPIO_NUM;
  cfg.pin_href      = HREF_GPIO_NUM;
  cfg.pin_sscb_sda  = SIOD_GPIO_NUM;   // I2C SDA for camera config
  cfg.pin_sscb_scl  = SIOC_GPIO_NUM;   // I2C SCL for camera config
  cfg.pin_pwdn      = PWDN_GPIO_NUM;
  cfg.pin_reset     = RESET_GPIO_NUM;
  cfg.xclk_freq_hz  = 20000000;        // 20 MHz XCLK
  cfg.pixel_format  = PIXFORMAT_JPEG;
  cfg.frame_size    = FRAMESIZE_SVGA;  // 800 × 600 — good balance of size/quality
  cfg.jpeg_quality  = 12;              // 0 = best, 63 = worst
  cfg.fb_count      = 1;

  esp_err_t err = esp_camera_init(&cfg);
  if (err != ESP_OK) {
    Serial.printf("[CAM] Init FAILED: 0x%x\n", err);
    return false;
  }

  // Flip image if camera is mounted upside-down
  // sensor_t* s = esp_camera_sensor_get();
  // s->set_vflip(s, 1);
  // s->set_hmirror(s, 1);

  Serial.println("[CAM] OV2640 ready");
  return true;
}


// =============================================================
//  HC-SR04 — DISTANCE MEASUREMENT
//  Returns distance in cm, or -1.0 on timeout (no object)
// =============================================================
float readDistanceCm() {
  // Send 10 µs trigger pulse
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // pulseIn with 30 ms timeout ≈ max measurable range ~5 m
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return -1.0f;       // timeout = nothing detected

  // distance = (time × speed of sound) / 2
  // speed of sound ≈ 0.0343 cm/µs at room temperature
  return (duration * 0.0343f) / 2.0f;
}


// =============================================================
//  SEND PHOTO TO TELEGRAM
//  Constructs a multipart/form-data HTTP POST over TLS.
//  No extra library needed beyond WiFiClientSecure.
// =============================================================
bool sendPhoto(const char* caption) {
  if (!camReady) {
    Serial.println("[CAM] Camera not ready — cannot send photo");
    return false;
  }

  // Capture frame
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("[CAM] Frame capture failed");
    return false;
  }
  Serial.printf("[TEL] JPEG captured: %u bytes\n", fb->len);

  // Connect to Telegram API
  if (!secureClient.connect("api.telegram.org", 443)) {
    Serial.println("[TEL] TLS connection failed");
    esp_camera_fb_return(fb);
    return false;
  }

  // Build multipart body
  String bound = "ESP32DoorBell";
  String partA;
  partA  = "--" + bound + "\r\n";
  partA += "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n";
  partA += String(CHAT_ID) + "\r\n";
  partA += "--" + bound + "\r\n";
  partA += "Content-Disposition: form-data; name=\"caption\"\r\n\r\n";
  partA += String(caption) + "\r\n";
  partA += "--" + bound + "\r\n";
  partA += "Content-Disposition: form-data; name=\"photo\"; filename=\"door.jpg\"\r\n";
  partA += "Content-Type: image/jpeg\r\n\r\n";
  String partB = "\r\n--" + bound + "--\r\n";

  int totalLen = partA.length() + (int)fb->len + partB.length();

  // HTTP headers
  secureClient.printf(
    "POST /bot%s/sendPhoto HTTP/1.1\r\n"
    "Host: api.telegram.org\r\n"
    "Content-Type: multipart/form-data; boundary=%s\r\n"
    "Content-Length: %d\r\n"
    "Connection: close\r\n\r\n",
    BOT_TOKEN, bound.c_str(), totalLen
  );

  // Send body — stream JPEG directly without buffering
  secureClient.print(partA);
  secureClient.write(fb->buf, fb->len);
  secureClient.print(partB);

  // Done with frame buffer — return ASAP
  esp_camera_fb_return(fb);

  // Read Telegram response
  unsigned long timeout = millis();
  bool success = false;
  while (secureClient.connected() && millis() - timeout < 10000) {
    if (secureClient.available()) {
      String line = secureClient.readStringUntil('\n');
      if (line.indexOf("\"ok\":true") >= 0) {
        success = true;
        break;
      }
    }
  }
  secureClient.stop();

  Serial.println(success ? "[TEL] Photo sent!" : "[TEL] Send failed (check token/chat_id)");
  return success;
}


// =============================================================
//  TRIGGER ALERT  — buzzer + LED + Telegram photo
// =============================================================
void triggerAlert(const char* caption, int buzzDurationMs) {
  unsigned long now = millis();

  // Cooldown guard — prevents alert spam
  if (now - lastAlertMs < COOLDOWN_MS) {
    Serial.printf("[ALT] Cooldown: %lu ms remaining\n",
                  COOLDOWN_MS - (now - lastAlertMs));
    return;
  }
  lastAlertMs = now;

  Serial.printf("[ALT] Triggering: %s\n", caption);

  // Buzzer on
  digitalWrite(RED_LED,    HIGH);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(buzzDurationMs);
  digitalWrite(BUZZER_PIN, LOW);

  // Capture + send photo (after buzzer, camera needs a moment post-boot)
  sendPhoto(caption);

  digitalWrite(RED_LED, LOW);
}


// =============================================================
//  SETUP
// =============================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n============================");
  Serial.println("    Smart Doorbell Boot     ");
  Serial.println("============================");

  // Configure GPIO pins
  pinMode(PIR_PIN,    INPUT);
  pinMode(TRIG_PIN,   OUTPUT);
  pinMode(ECHO_PIN,   INPUT);
  pinMode(BTN_PIN,    INPUT);          // 10 kΩ pull-down wired on breadboard
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(GREEN_LED,  OUTPUT);
  pinMode(RED_LED,    OUTPUT);

  // Safe initial state
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(GREEN_LED,  LOW);
  digitalWrite(RED_LED,    LOW);

  // Initialise camera
  camReady = initCamera();
  if (!camReady) {
    Serial.println("[ERR] Camera failed — check wiring. Continuing without camera.");
  }

  // Connect to WiFi
  Serial.printf("[NET] Connecting to %s", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[NET] Connected — IP: %s\n",
                  WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n[NET] WiFi failed — alerts will not be sent");
  }

  // TLS: use setInsecure() for a student project (skips cert verification)
  // For production: replace with secureClient.setCACert(cert_pem);
  secureClient.setInsecure();

  // Boot confirmation — 3 green blinks
  for (int i = 0; i < 3; i++) {
    digitalWrite(GREEN_LED, HIGH); delay(150);
    digitalWrite(GREEN_LED, LOW);  delay(150);
  }

  Serial.println("[SYS] System ready — watching door\n");
}


// =============================================================
//  MAIN LOOP
// =============================================================
void loop() {
  unsigned long now = millis();

  // ─── SENSOR 3: Push Button ────────────────────────────────
  //  Checked first — highest priority (visitor already at door)
  if (digitalRead(BTN_PIN) == HIGH) {
    Serial.println("[BTN] Doorbell button pressed!");
    triggerAlert("Someone pressed your doorbell!", BUZZ_LONG_MS);

    // Wait for button release before continuing
    while (digitalRead(BTN_PIN) == HIGH) delay(50);
    delay(300);   // debounce pause
    return;
  }

  // ─── SENSOR 1: PIR Motion ────────────────────────────────
  if (digitalRead(PIR_PIN) == HIGH) {

    // Debounce — PIR can re-trigger rapidly
    if (now - lastPIRMs < PIR_DEBOUNCE_MS) {
      delay(100);
      return;
    }
    lastPIRMs = now;

    Serial.println("[PIR] Motion detected — checking distance");
    digitalWrite(GREEN_LED, HIGH);      // visual feedback while measuring

    // ─── SENSOR 2: HC-SR04 Proximity Confirmation ─────────
    //  This filters out distant motion (cars, animals, swaying trees)
    float dist = readDistanceCm();

    if (dist < 0) {
      Serial.println("[USS] No echo — object too far or absent");
    } else {
      Serial.printf("[USS] Distance: %.1f cm\n", dist);
    }

    if (dist > 0 && dist <= PROXIMITY_CM) {
      // CONFIRMED visitor — within 150 cm threshold
      char caption[120];
      snprintf(caption, sizeof(caption),
               "Motion alert! Visitor detected %.0f cm from door.", dist);
      triggerAlert(caption, BUZZ_SHORT_MS);

    } else {
      // Distant movement — green LED double-blink only, no Telegram alert
      Serial.printf("[USS] Too far (%.0f cm) — ignored\n", dist);
      for (int i = 0; i < 2; i++) {
        digitalWrite(GREEN_LED, LOW);  delay(80);
        digitalWrite(GREEN_LED, HIGH); delay(80);
      }
    }

    digitalWrite(GREEN_LED, LOW);
  }

  delay(100);   // poll sensors every 100 ms
}
