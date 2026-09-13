/*
  AgriGate ESP32

  Main controller responsibilities:
  - Servo gate control
  - Manual/automatic operation
  - Nano serial input
  - OLED status
  - Web dashboard
  - Timer control
  - Runoff/drainage logic
  - Event history

  Pin configuration is documented in README.md.

  Replace Wi-Fi credentials before use.
*/

#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>
#include <time.h>

const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

#define SERVO_PIN       18
#define GREEN_LED       19
#define RED_LED         21
#define OLED_SDA        32
#define OLED_SCL        22
#define OPEN_BUTTON     27
#define CLOSE_BUTTON    14
#define MANUAL_SWITCH   33
#define AUTO_SWITCH     12
#define NANO_RX         16
#define NANO_TX         17

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
WebServer server(80);
Servo gateServo;

enum GateState { GATE_OPEN, GATE_CLOSED };
enum SystemMode { MODE_MANUAL, MODE_AUTO };

GateState gateState = GATE_CLOSED;
SystemMode systemMode = MODE_AUTO;

int channelLevel = 0;
int fieldLevel = 0;
int previousChannelLevel = 0;
int previousFieldLevel = 0;

String fieldTrend = "STABLE";
String channelTrend = "STABLE";
String lastEvent = "SYSTEM READY";

void showStatus() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("AGRIGATE");

  display.print("MODE: ");
  display.println(systemMode == MODE_AUTO ? "AUTO" : "MANUAL");

  display.print("GATE: ");
  display.println(gateState == GATE_OPEN ? "OPEN" : "CLOSED");

  display.print("CH: ");
  display.println(channelLevel == 2 ? "HIGH" : channelLevel == 1 ? "MEDIUM" : "LOW");

  display.print("FIELD: ");
  display.println(fieldLevel == 2 ? "HIGH" : fieldLevel == 1 ? "MEDIUM" : "LOW");

  display.print("TREND: ");
  display.println(fieldTrend);

  display.println("SYSTEM NORMAL");
  display.display();
}

void openGate() {
  gateServo.write(0);
  gateState = GATE_OPEN;
  digitalWrite(GREEN_LED, HIGH);
  digitalWrite(RED_LED, LOW);
  lastEvent = "GATE OPEN";
  showStatus();
}

void closeGate() {
  gateServo.write(180);
  gateState = GATE_CLOSED;
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, HIGH);
  lastEvent = "GATE CLOSED";
  showStatus();
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, NANO_RX, NANO_TX);

  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(OPEN_BUTTON, INPUT_PULLUP);
  pinMode(CLOSE_BUTTON, INPUT_PULLUP);
  pinMode(MANUAL_SWITCH, INPUT_PULLUP);
  pinMode(AUTO_SWITCH, INPUT_PULLUP);

  Wire.begin(OLED_SDA, OLED_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);

  gateServo.attach(SERVO_PIN);
  closeGate();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) delay(250);

  configTime(19800, 0, "pool.ntp.org");
  showStatus();
}

void loop() {
  if (Serial2.available()) {
    String data = Serial2.readStringUntil('\n');
    int cIndex = data.indexOf("C:");
    int fIndex = data.indexOf(",F:");

    if (cIndex >= 0 && fIndex > cIndex) {
      previousChannelLevel = channelLevel;
      previousFieldLevel = fieldLevel;

      channelLevel = data.substring(cIndex + 2, fIndex).toInt();
      fieldLevel = data.substring(fIndex + 3).toInt();

      fieldTrend = fieldLevel > previousFieldLevel ? "RISING" :
                   fieldLevel < previousFieldLevel ? "FALLING" : "STABLE";

      channelTrend = channelLevel > previousChannelLevel ? "RISING" :
                     channelLevel < previousChannelLevel ? "FALLING" : "STABLE";
    }
  }

  if (digitalRead(OPEN_BUTTON) == LOW && systemMode == MODE_MANUAL) {
    openGate();
    delay(250);
  }

  if (digitalRead(CLOSE_BUTTON) == LOW && systemMode == MODE_MANUAL) {
    closeGate();
    delay(250);
  }

  // Automatic drainage logic
  if (systemMode == MODE_AUTO) {
    if (fieldLevel == 2) openGate();

    if (fieldLevel >= 1 && fieldTrend == "RISING") {
      openGate();
    }

    if (fieldLevel == 0 && gateState == GATE_OPEN) {
      closeGate();
    }
  }

  delay(50);
}
