// ================================================================
//                    AGRIGATE WATER SYSTEM
//              INTELLIGENT PADDY FIELD CONTROLLER
// ================================================================
//
// FEATURES
// ------------------------------------------------
// 1. Arduino Nano water-level monitoring
// 2. Channel LOW / MEDIUM / HIGH
// 3. Field LOW / MEDIUM / HIGH
// 4. Field rising / falling detection
// 5. Rain / runoff inference
// 6. Automatic drainage
// 7. Manual gate control
// 8. Web AUTO / MANUAL override
// 9. Gate timer
// 10. Servo gate
// 11. Gate LEDs
// 12. 1.3" OLED
// 13. Full-screen OLED task/event display
// 14. Startup IP display
// 15. NVS persistent event history
// 16. Date + time on events
// 17. Web dashboard
// 18. Crop protection monitoring
// 19. Drainage obstruction detection
//
// SERVO CALIBRATION
// ------------------------------------------------
// 0 degrees   = GATE OPEN
// 180 degrees = GATE CLOSED
//
// ================================================================


// ================================================================
// LIBRARIES
// ================================================================

#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <Preferences.h>
#include <time.h>


// ================================================================
// WIFI SETTINGS
// ================================================================

const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";


// ================================================================
// PIN DEFINITIONS
// ================================================================

// Servo
#define SERVO_PIN 18

// Gate LEDs
#define GREEN_LED 19
#define RED_LED   21

// OLED
#define OLED_SDA 32
#define OLED_SCL 22

// Manual buttons
#define OPEN_BUTTON  27
#define CLOSE_BUTTON 14

// Physical mode switch
// UP   = MANUAL
// DOWN = AUTO
#define MANUAL_SWITCH 33
#define AUTO_SWITCH   12

// Arduino Nano serial
#define NANO_RX 16
#define NANO_TX 17


// ================================================================
// OLED SETTINGS
// ================================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  -1
);


// ================================================================
// OBJECTS
// ================================================================

Servo gateServo;

HardwareSerial NanoSerial(2);

WebServer server(80);

Preferences preferences;


// ================================================================
// ENUMERATIONS
// ================================================================

enum GateState {

  GATE_CLOSED,
  GATE_OPEN

};

enum SystemMode {

  MODE_MANUAL,
  MODE_AUTO

};


// ================================================================
// CURRENT STATES
// ================================================================

GateState gateState = GATE_CLOSED;

SystemMode systemMode = MODE_AUTO;


// ================================================================
// WEB OVERRIDE
// ================================================================

bool webModeOverride = false;

SystemMode webSelectedMode = MODE_AUTO;


// ================================================================
// WATER LEVELS
// ================================================================

// 0 = LOW
// 1 = MEDIUM
// 2 = HIGH

int channelLevel = 0;

int fieldLevel = 0;

int previousChannelLevel = 0;

int previousFieldLevel = 0;


// ================================================================
// WATER TRENDS
// ================================================================

String fieldTrend = "STABLE";

String channelTrend = "STABLE";


// ================================================================
// RUNOFF / DRAINAGE
// ================================================================

bool runoffDetected = false;

bool drainageRequired = false;

bool drainageSuccessful = false;

bool drainageProblem = false;

String runoffReason =
  "System operating normally";


// ================================================================
// PREVIOUS CONDITIONS
// ================================================================

bool previousRunoffDetected = false;

bool previousDrainageProblem = false;

bool previousDrainageSuccessful = false;


// ================================================================
// NANO COMMUNICATION
// ================================================================

unsigned long lastNanoData = 0;

#define NANO_TIMEOUT 5000


// ================================================================
// DRAINAGE TIMER
// ================================================================

unsigned long drainageStartTime = 0;

#define DRAIN_CHECK_TIME 15000


// ================================================================
// WEB GATE TIMER
// ================================================================

bool timerActive = false;

String timerAction = "";

unsigned long timerTargetMillis = 0;


// ================================================================
// OLED TASK SCREEN
// ================================================================

bool taskScreen = false;

String taskTitle = "";

String taskLine1 = "";

String taskLine2 = "";

unsigned long taskScreenUntil = 0;


// ================================================================
// LAST EVENT
// ================================================================

String lastEvent = "SYSTEM READY";


// ================================================================
// HISTORY
// ================================================================

#define HISTORY_SIZE 30

String historyEvent[HISTORY_SIZE];

String historyTime[HISTORY_SIZE];

int historyCount = 0;


// ================================================================
// TIME
// ================================================================

const char* NTP_SERVER = "pool.ntp.org";

// India Standard Time
const long GMT_OFFSET_SEC = 19800;

const int DAYLIGHT_OFFSET_SEC = 0;


// ================================================================
// LOOP TIMERS
// ================================================================

unsigned long lastOLEDUpdate = 0;

unsigned long lastAnalysis = 0;

unsigned long lastButtonCheck = 0;

unsigned long lastSerialStatus = 0;

unsigned long lastTimerOLED = 0;


#define OLED_INTERVAL     1000
#define ANALYSIS_INTERVAL 1000
#define BUTTON_INTERVAL    250
#define STATUS_INTERVAL   3000


// ================================================================
// LEVEL NAME
// ================================================================

String levelName(int level) {

  if (level == 2) {

    return "HIGH";

  }

  if (level == 1) {

    return "MEDIUM";

  }

  return "LOW";
}


// ================================================================
// GET DATE AND TIME
// ================================================================

String getDateTime() {

  struct tm timeinfo;

  if (!getLocalTime(&timeinfo)) {

    return "TIME NOT SYNCED";

  }

  char buffer[32];

  strftime(
    buffer,
    sizeof(buffer),
    "%d-%m-%Y %H:%M:%S",
    &timeinfo
  );

  return String(buffer);
}


// ================================================================
// ESCAPE JSON
// ================================================================

String escapeJSON(String text) {

  text.replace(
    "\\",
    "\\\\"
  );

  text.replace(
    "\"",
    "\\\""
  );

  text.replace(
    "\n",
    " "
  );

  text.replace(
    "\r",
    " "
  );

  return text;
}


// ================================================================
// LOAD HISTORY FROM NVS
// ================================================================

void loadHistory() {

  preferences.begin(
    "agrigate",
    false
  );

  historyCount =
    preferences.getInt(
      "count",
      0
    );

  if (
    historyCount < 0 ||
    historyCount > HISTORY_SIZE
  ) {

    historyCount = 0;

  }

  for (
    int i = 0;
    i < historyCount;
    i++
  ) {

    String eventKey =
      "event" + String(i);

    String timeKey =
      "time" + String(i);

    historyEvent[i] =
      preferences.getString(
        eventKey.c_str(),
        ""
      );

    historyTime[i] =
      preferences.getString(
        timeKey.c_str(),
        ""
      );
  }
}


// ================================================================
// SAVE HISTORY TO NVS
// ================================================================

void saveHistory() {

  preferences.putInt(
    "count",
    historyCount
  );

  for (
    int i = 0;
    i < historyCount;
    i++
  ) {

    String eventKey =
      "event" + String(i);

    String timeKey =
      "time" + String(i);

    preferences.putString(
      eventKey.c_str(),
      historyEvent[i]
    );

    preferences.putString(
      timeKey.c_str(),
      historyTime[i]
    );
  }
}


// ================================================================
// ADD HISTORY EVENT
// ================================================================

void addHistory(String event) {

  String timestamp =
    getDateTime();

  lastEvent = event;


  // ------------------------------------------------
  // Shift existing history
  // ------------------------------------------------

  if (
    historyCount <
    HISTORY_SIZE
  ) {

    for (
      int i = historyCount;
      i > 0;
      i--
    ) {

      historyEvent[i] =
        historyEvent[i - 1];

      historyTime[i] =
        historyTime[i - 1];

    }

    historyCount++;

  }

  else {

    for (
      int i = HISTORY_SIZE - 1;
      i > 0;
      i--
    ) {

      historyEvent[i] =
        historyEvent[i - 1];

      historyTime[i] =
        historyTime[i - 1];

    }
  }


  // ------------------------------------------------
  // Add newest event
  // ------------------------------------------------

  historyEvent[0] =
    event;

  historyTime[0] =
    timestamp;


  // ------------------------------------------------
  // Save to NVS
  // ------------------------------------------------

  saveHistory();
}


// ================================================================
// OLED TEXT FIT HELPER
// ================================================================

String oledFit(String text, int maxChars) {

  if (text.length() <= maxChars) {
    return text;
  }

  if (maxChars <= 3) {
    return text.substring(0, maxChars);
  }

  return text.substring(0, maxChars - 3) + "...";
}


// ================================================================
// OLED FULL SCREEN EVENT
// ================================================================

void showEvent(
  String title,
  String line1,
  String line2
) {

  taskScreen = true;
  taskTitle = title;
  taskLine1 = line1;
  taskLine2 = line2;
  taskScreenUntil = millis() + 4500;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  String titleText = oledFit(title, 21);
  int titleX = (128 - titleText.length() * 6) / 2;

  if (titleX < 0) {
    titleX = 0;
  }

  display.setCursor(titleX, 0);
  display.println(titleText);

  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  display.setCursor(0, 17);
  display.println(oledFit(line1, 21));

  display.setCursor(0, 30);
  display.println(oledFit(line2, 21));

  display.drawLine(0, 43, 127, 43, SSD1306_WHITE);

  String dt = getDateTime();

  if (dt.length() >= 19) {
    display.setCursor(0, 48);
    display.println(oledFit(dt.substring(0, 10), 21));

    display.setCursor(0, 57);
    display.println(oledFit(dt.substring(11), 21));
  }
  else {
    display.setCursor(0, 52);
    display.println(oledFit(dt, 21));
  }

  display.display();
}


// ================================================================
// STARTUP OLED
// ================================================================

void showStartupScreen() {

  display.clearDisplay();

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setTextSize(2);

  display.setCursor(
    8,
    0
  );

  display.println(
    "AGRIGATE"
  );


  display.drawLine(
    0,
    19,
    127,
    19,
    SSD1306_WHITE
  );


  display.setTextSize(1);

  display.setCursor(
    0,
    27
  );

  display.println(
    "SYSTEM ONLINE"
  );


  display.setCursor(
    0,
    39
  );

  display.println(
    "IP ADDRESS:"
  );


  display.setCursor(
    0,
    51
  );

  if (
    WiFi.status() ==
    WL_CONNECTED
  ) {

    display.println(
      WiFi.localIP().toString()
    );

  }

  else {

    display.println(
      "WIFI OFFLINE"
    );
  }


  display.display();

  delay(6000);
}


// ================================================================
// NORMAL OLED SCREEN
// ================================================================

void updateOLED() {

  if (
    millis() -
    lastOLEDUpdate <
    OLED_INTERVAL
  ) {

    return;

  }

  lastOLEDUpdate =
    millis();


  // ------------------------------------------------
  // TASK SCREEN
  // ------------------------------------------------

  if (taskScreen) {

    if (millis() < taskScreenUntil) {

      display.clearDisplay();
      display.setTextColor(SSD1306_WHITE);
      display.setTextSize(1);

      String titleText = oledFit(taskTitle, 21);
      int titleX = (128 - titleText.length() * 6) / 2;

      if (titleX < 0) {
        titleX = 0;
      }

      display.setCursor(titleX, 0);
      display.println(titleText);

      display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

      display.setCursor(0, 17);
      display.println(oledFit(taskLine1, 21));

      display.setCursor(0, 30);
      display.println(oledFit(taskLine2, 21));

      display.drawLine(0, 43, 127, 43, SSD1306_WHITE);

      String taskDT = getDateTime();

      if (taskDT.length() >= 19) {
        display.setCursor(0, 48);
        display.println(oledFit(taskDT.substring(0, 10), 21));

        display.setCursor(0, 57);
        display.println(oledFit(taskDT.substring(11), 21));
      }
      else {
        display.setCursor(0, 52);
        display.println(oledFit(taskDT, 21));
      }

      display.display();
      return;
    }

    taskScreen = false;
  }


  // ------------------------------------------------
  // TIMER SCREEN
  // ------------------------------------------------

  if (timerActive) {

    display.clearDisplay();

    display.setTextColor(
      SSD1306_WHITE
    );

    display.setTextSize(2);

    display.setCursor(
      0,
      0
    );

    display.println(
      "TIMER"
    );


    display.drawLine(
      0,
      19,
      127,
      19,
      SSD1306_WHITE
    );


    display.setTextSize(1);

    display.setCursor(
      0,
      28
    );

    display.print(
      "ACTION: "
    );

    display.println(
      timerAction == "open"
      ? "OPEN"
      : "CLOSE"
    );


    display.setCursor(
      0,
      41
    );

    display.println(
      "REMAINING:"
    );


    display.setCursor(
      0,
      53
    );

    display.println(
      timerRemaining()
    );


    display.display();

    return;
  }


  // ------------------------------------------------
  // NORMAL STATUS SCREEN
  // ------------------------------------------------

  display.clearDisplay();

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setTextSize(1);


  // TITLE

  display.setCursor(
    0,
    0
  );

  display.println(
    "AGRIGATE"
  );


  display.drawLine(
    0,
    9,
    127,
    9,
    SSD1306_WHITE
  );


  // MODE

  display.setCursor(
    0,
    13
  );

  display.print(
    "MODE: "
  );

  if (
    webModeOverride
  ) {

    display.print(
      "WEB-"
    );

  }

  display.println(
    systemMode ==
    MODE_AUTO
    ? "AUTO"
    : "MANUAL"
  );


  // GATE

  display.setCursor(
    0,
    24
  );

  display.print(
    "GATE: "
  );

  display.println(
    gateState ==
    GATE_OPEN
    ? "OPEN"
    : "CLOSED"
  );


  // WATER

  display.setCursor(
    0,
    35
  );

  display.print(
    "CH:"
  );

  display.print(
    levelName(channelLevel)
  );


  display.setCursor(
    68,
    35
  );

  display.print(
    "FIELD:"
  );

  display.print(
    levelName(fieldLevel)
  );


  // TREND

  display.setCursor(
    0,
    46
  );

  display.print(
    "TREND:"
  );

  display.print(
    fieldTrend
  );


  // STATUS

  display.setCursor(
    0,
    57
  );

  if (
    drainageProblem
  ) {

    display.print(
      "DRAIN WARNING"
    );

  }

  else if (
    drainageRequired
  ) {

    display.print(
      "DRAINING"
    );

  }

  else if (
    runoffDetected
  ) {

    display.print(
      "RUNOFF"
    );

  }

  else {

    display.print(
      "SYSTEM NORMAL"
    );
  }


  display.display();
}


// ================================================================
// OPEN GATE
// ================================================================

void openGate(
  String reason
) {

  if (
    gateState ==
    GATE_OPEN
  ) {

    return;
  }


  // ------------------------------------------------
  // Servo
  // ------------------------------------------------

  gateServo.write(
    0
  );


  gateState =
    GATE_OPEN;


  // ------------------------------------------------
  // LEDs
  // ------------------------------------------------

  digitalWrite(
    GREEN_LED,
    HIGH
  );

  digitalWrite(
    RED_LED,
    LOW
  );


  // ------------------------------------------------
  // Drainage timer
  // ------------------------------------------------

  drainageStartTime =
    millis();


  // ------------------------------------------------
  // History
  // ------------------------------------------------

  addHistory(
    "Gate OPEN - " +
    reason
  );


  // ------------------------------------------------
  // OLED
  // ------------------------------------------------

  showEvent(
    "GATE OPEN",
    "Reason:",
    reason
  );


  Serial.println(
    "GATE OPEN"
  );

  Serial.println(
    "Reason: " +
    reason
  );
}


// ================================================================
// CLOSE GATE
// ================================================================

void closeGate(
  String reason
) {

  if (
    gateState ==
    GATE_CLOSED
  ) {

    return;
  }


  // ------------------------------------------------
  // Servo
  // ------------------------------------------------

  gateServo.write(
    180
  );


  gateState =
    GATE_CLOSED;


  // ------------------------------------------------
  // LEDs
  // ------------------------------------------------

  digitalWrite(
    GREEN_LED,
    LOW
  );

  digitalWrite(
    RED_LED,
    HIGH
  );


  drainageStartTime =
    0;


  // ------------------------------------------------
  // History
  // ------------------------------------------------

  addHistory(
    "Gate CLOSED - " +
    reason
  );


  // ------------------------------------------------
  // OLED
  // ------------------------------------------------

  showEvent(
    "GATE CLOSED",
    "Reason:",
    reason
  );


  Serial.println(
    "GATE CLOSED"
  );

  Serial.println(
    "Reason: " +
    reason
  );
}


// ================================================================
// READ NANO
// ================================================================

void readNano() {

  while (
    NanoSerial.available()
  ) {

    String data =
      NanoSerial.readStringUntil(
        '\n'
      );

    data.trim();


    if (
      data.length() ==
      0
    ) {

      continue;
    }


    int channelPosition =
      data.indexOf(
        "C:"
      );

    int fieldPosition =
      data.indexOf(
        ",F:"
      );


    if (
      channelPosition >= 0 &&
      fieldPosition >= 0
    ) {

      int newChannel =
        data.substring(
          channelPosition + 2,
          fieldPosition
        ).toInt();


      int newField =
        data.substring(
          fieldPosition + 3
        ).toInt();


      if (
        newChannel >= 0 &&
        newChannel <= 2 &&
        newField >= 0 &&
        newField <= 2
      ) {

        previousChannelLevel =
          channelLevel;

        previousFieldLevel =
          fieldLevel;


        channelLevel =
          newChannel;

        fieldLevel =
          newField;


        lastNanoData =
          millis();


        // ------------------------------------------------
        // FIELD TREND
        // ------------------------------------------------

        if (
          fieldLevel >
          previousFieldLevel
        ) {

          fieldTrend =
            "RISING";

        }

        else if (
          fieldLevel <
          previousFieldLevel
        ) {

          fieldTrend =
            "FALLING";

        }

        else {

          fieldTrend =
            "STABLE";
        }


        // ------------------------------------------------
        // CHANNEL TREND
        // ------------------------------------------------

        if (
          channelLevel >
          previousChannelLevel
        ) {

          channelTrend =
            "RISING";

        }

        else if (
          channelLevel <
          previousChannelLevel
        ) {

          channelTrend =
            "FALLING";

        }

        else {

          channelTrend =
            "STABLE";
        }
      }
    }
  }
}


// ================================================================
// WATER ANALYSIS
// ================================================================

void analyzeWater() {

  if (
    millis() -
    lastAnalysis <
    ANALYSIS_INTERVAL
  ) {

    return;
  }

  lastAnalysis =
    millis();


  runoffDetected =
    false;

  drainageRequired =
    false;

  drainageSuccessful =
    false;

  drainageProblem =
    false;


  // ==============================================================
  // FIELD RISING + CHANNEL LOW
  // ==============================================================

  if (
    fieldTrend ==
    "RISING" &&
    channelLevel ==
    0
  ) {

    runoffDetected =
      true;

    runoffReason =
      "Field rising while channel is LOW";

  }


  // ==============================================================
  // FIELD RISING + CHANNEL MEDIUM
  // ==============================================================

  else if (
    fieldTrend ==
    "RISING" &&
    channelLevel ==
    1
  ) {

    runoffDetected =
      true;

    runoffReason =
      "Field rising - possible surrounding runoff";

  }


  // ==============================================================
  // FIELD HIGH
  // ==============================================================

  if (
    fieldLevel ==
    2
  ) {

    drainageRequired =
      true;


    if (
      channelLevel <= 1
    ) {

      runoffDetected =
        true;

      runoffReason =
        "Field HIGH - drainage priority";

    }
  }


  // ==============================================================
  // BOTH HIGH
  // ==============================================================

  if (
    channelLevel ==
    2 &&
    fieldLevel ==
    2
  ) {

    runoffDetected =
      true;

    drainageRequired =
      true;

    runoffReason =
      "Channel and field HIGH";

  }


  // ==============================================================
  // SUCCESSFUL DRAINAGE
  // ==============================================================

  if (
    gateState ==
    GATE_OPEN &&
    fieldTrend ==
    "FALLING"
  ) {

    drainageSuccessful =
      true;

    runoffReason =
      "Field level falling - drainage successful";

  }


  // ==============================================================
  // DRAINAGE PROBLEM
  // ==============================================================

  if (
    gateState ==
    GATE_OPEN &&
    drainageStartTime > 0 &&
    millis() -
    drainageStartTime >
    DRAIN_CHECK_TIME &&
    fieldLevel ==
    2 &&
    fieldTrend !=
    "FALLING"
  ) {

    drainageProblem =
      true;

    runoffReason =
      "Field remains HIGH - possible drainage obstruction";

  }


  // ==============================================================
  // NEW RUNOFF EVENT
  // ==============================================================

  if (
    runoffDetected &&
    !previousRunoffDetected
  ) {

    addHistory(
      "Possible rain/runoff detected"
    );

    showEvent(
      "RUNOFF",
      "FIELD WATER RISING",
      "POSSIBLE RAIN / INFLOW"
    );
  }


  // ==============================================================
  // NEW DRAINAGE WARNING
  // ==============================================================

  if (
    drainageProblem &&
    !previousDrainageProblem
  ) {

    addHistory(
      "Possible drainage obstruction"
    );

    showEvent(
      "DRAIN WARNING",
      "FIELD STILL HIGH",
      "CHECK DRAINAGE"
    );
  }


  // ==============================================================
  // NEW DRAINAGE SUCCESS
  // ==============================================================

  if (
    drainageSuccessful &&
    !previousDrainageSuccessful
  ) {

    addHistory(
      "Drainage successful"
    );

    showEvent(
      "DRAINAGE OK",
      "FIELD LEVEL FALLING",
      "WATER IS EXITING"
    );
  }


  // ==============================================================
  // STORE CONDITIONS
  // ==============================================================

  previousRunoffDetected =
    runoffDetected;

  previousDrainageProblem =
    drainageProblem;

  previousDrainageSuccessful =
    drainageSuccessful;
}


// ================================================================
// AUTOMATIC CONTROL
// ================================================================

void autoControl() {

  // AUTO LOGIC IS ACTIVE ONLY IN AUTO MODE.
  if (systemMode != MODE_AUTO) {
    return;
  }

  // =============================================================
  // TARGET FIELD LEVEL = MEDIUM
  // LOW    -> OPEN if channel has water
  // MEDIUM -> CLOSE / HOLD
  // HIGH   -> OPEN only if channel is lower than field (DRAIN)
  // =============================================================

  // FIELD LOW: bring water into the field.
  if (fieldLevel == 0) {

    if (channelLevel >= 1) {

      if (gateState == GATE_CLOSED) {
        openGate(
          channelLevel == 2
          ? "Field LOW - Channel HIGH"
          : "Field LOW - Channel MEDIUM"
        );
      }

    }
    else {

      // Both are LOW: there is no useful water supply.
      if (gateState == GATE_OPEN) {
        closeGate("Field LOW - Channel LOW");
      }
    }

    return;
  }

  // FIELD MEDIUM: desired level reached.
  if (fieldLevel == 1) {

    if (gateState == GATE_OPEN) {
      closeGate("Field MEDIUM - Maintain Level");
    }

    return;
  }

  // FIELD HIGH: drain only when channel level is lower.
  if (fieldLevel == 2) {

    if (channelLevel < fieldLevel) {

      if (gateState == GATE_CLOSED) {
        openGate("Field HIGH - Drainage");
      }

    }
    else {

      // Channel is also HIGH, so there is no safe drainage gradient.
      if (gateState == GATE_OPEN) {
        closeGate("Field HIGH - Channel HIGH");
      }
    }

    return;
  }
}


// ================================================================
// PHYSICAL / WEB MODE CONTROL
// ================================================================

void updateMode() {

  // Read the physical switch position every cycle.
  bool manual = digitalRead(MANUAL_SWITCH) == LOW;
  bool automatic = digitalRead(AUTO_SWITCH) == LOW;

  // Valid physical selection.
  int physicalSelection = -1;

  if (manual && !automatic) {
    physicalSelection = MODE_MANUAL;
  }
  else if (automatic && !manual) {
    physicalSelection = MODE_AUTO;
  }

  // ---------------------------------------------------------------
  // PHYSICAL SWITCH HAS HIGHEST PRIORITY WHEN IT IS CHANGED
  // ---------------------------------------------------------------
  // This is the important part: if the physical switch changes while
  // a web override is active, the web override is cancelled immediately.
  static int lastPhysicalSelection = -2;

  if (physicalSelection != -1 &&
      physicalSelection != lastPhysicalSelection) {

    lastPhysicalSelection = physicalSelection;

    webModeOverride = false;

    systemMode =
      physicalSelection == MODE_MANUAL
      ? MODE_MANUAL
      : MODE_AUTO;

    if (systemMode == MODE_MANUAL) {
      addHistory("Physical switch: MANUAL - Web override cancelled");
      showEvent(
        "MANUAL MODE",
        "PHYSICAL SWITCH",
        "WEB OVERRIDE OFF"
      );
    }
    else {
      addHistory("Physical switch: AUTO - Web override cancelled");
      showEvent(
        "AUTO MODE",
        "PHYSICAL SWITCH",
        "WEB OVERRIDE OFF"
      );
    }

    return;
  }

  // ---------------------------------------------------------------
  // WEB OVERRIDE
  // ---------------------------------------------------------------
  // If the physical switch has NOT changed, the web-selected mode
  // remains active and overrides the physical switch setting.
  if (webModeOverride) {

    if (systemMode != webSelectedMode) {

      systemMode = webSelectedMode;

      String modeName =
        systemMode == MODE_AUTO
        ? "AUTO"
        : "MANUAL";

      addHistory("Web override: " + modeName);

      showEvent(
        "WEB OVERRIDE",
        "SYSTEM MODE:",
        modeName
      );
    }

    return;
  }

  // ---------------------------------------------------------------
  // PHYSICAL SWITCH STEADY STATE
  // ---------------------------------------------------------------
  if (physicalSelection == MODE_MANUAL) {
    systemMode = MODE_MANUAL;
  }
  else if (physicalSelection == MODE_AUTO) {
    systemMode = MODE_AUTO;
  }
}


// ================================================================
// BUTTON CONTROL
// ================================================================

void checkButtons() {

  if (
    millis() -
    lastButtonCheck <
    BUTTON_INTERVAL
  ) {

    return;
  }

  lastButtonCheck =
    millis();


  // ------------------------------------------------
  // Buttons work only in MANUAL
  // ------------------------------------------------

  if (
    systemMode !=
    MODE_MANUAL
  ) {

    return;
  }


  // ------------------------------------------------
  // OPEN
  // ------------------------------------------------

  if (
    digitalRead(
      OPEN_BUTTON
    ) == LOW
  ) {

    openGate(
      "Manual button"
    );

  }


  // ------------------------------------------------
  // CLOSE
  // ------------------------------------------------

  if (
    digitalRead(
      CLOSE_BUTTON
    ) == LOW
  ) {

    closeGate(
      "Manual button"
    );

  }
}


// ================================================================
// TIMER REMAINING
// ================================================================

String timerRemaining() {

  if (
    !timerActive
  ) {

    return "--";
  }


  if (
    millis() >=
    timerTargetMillis
  ) {

    return "0m 0s";
  }


  unsigned long remaining =
    timerTargetMillis -
    millis();


  unsigned long seconds =
    remaining /
    1000;


  unsigned long minutes =
    seconds /
    60;


  seconds %=
    60;


  return
    String(minutes) +
    "m " +
    String(seconds) +
    "s";
}


// ================================================================
// TIMER
// ================================================================

void checkTimer() {

  if (
    !timerActive
  ) {

    return;
  }


  // ------------------------------------------------
  // Timer OLED
  // ------------------------------------------------

  if (millis() - lastTimerOLED >= 1000) {

    lastTimerOLED = millis();

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);

    display.setCursor(0, 0);
    display.println("TIMER");

    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    display.setCursor(0, 18);
    display.print("ACTION: ");
    display.println(
      timerAction == "open"
      ? "OPEN"
      : "CLOSE"
    );

    display.setCursor(0, 31);
    display.println("REMAINING:");

    display.setTextSize(2);
    display.setCursor(0, 44);
    display.println(oledFit(timerRemaining(), 10));

    display.display();
  }


  // ------------------------------------------------
  // Execute timer
  // ------------------------------------------------

  if (
    millis() >=
    timerTargetMillis
  ) {

    String action =
      timerAction;


    timerActive =
      false;


    timerAction =
      "";


    // ------------------------------------------------
    // Execute
    // ------------------------------------------------

    if (
      action ==
      "open"
    ) {

      openGate(
        "Scheduled timer"
      );

    }

    else if (
      action ==
      "close"
    ) {

      closeGate(
        "Scheduled timer"
      );

    }


    addHistory(
      "Timer executed: " +
      action
    );


    showEvent(
      "TIMER DONE",
      "SCHEDULED ACTION",
      action == "open"
      ? "GATE OPENED"
      : "GATE CLOSED"
    );
  }
}


// ================================================================
// WEB MODE HANDLER
// ================================================================

void handleMode() {

  if (!server.hasArg("action")) {
    server.send(400, "text/plain", "Missing action");
    return;
  }

  String action = server.arg("action");

  // ---------------------------------------------------------------
  // RETURN TO PHYSICAL SWITCH CONTROL
  // ---------------------------------------------------------------
  if (action == "physical") {

    webModeOverride = false;

    // Immediately read and apply the physical switch.
    bool manual = digitalRead(MANUAL_SWITCH) == LOW;
    bool automatic = digitalRead(AUTO_SWITCH) == LOW;

    if (manual && !automatic) {
      systemMode = MODE_MANUAL;
    }
    else if (automatic && !manual) {
      systemMode = MODE_AUTO;
    }

    addHistory("Web override OFF - physical switch active");

    showEvent(
      "OVERRIDE OFF",
      "PHYSICAL SWITCH",
      "CONTROL RESTORED"
    );

    server.send(200, "text/plain", "OK");
    return;
  }

  // ---------------------------------------------------------------
  // WEB MANUAL OVERRIDE
  // ---------------------------------------------------------------
  if (action == "manual") {

    webModeOverride = true;
    webSelectedMode = MODE_MANUAL;
    systemMode = MODE_MANUAL;

    addHistory("Web override: MANUAL");

    showEvent(
      "MANUAL OVERRIDE",
      "WEB CONTROL",
      "ENABLED"
    );

    server.send(200, "text/plain", "OK");
    return;
  }

  // ---------------------------------------------------------------
  // WEB AUTO OVERRIDE
  // ---------------------------------------------------------------
  if (action == "auto") {

    webModeOverride = true;
    webSelectedMode = MODE_AUTO;
    systemMode = MODE_AUTO;

    addHistory("Web override: AUTO");

    showEvent(
      "AUTO OVERRIDE",
      "WEB CONTROL",
      "ENABLED"
    );

    server.send(200, "text/plain", "OK");
    return;
  }

  server.send(400, "text/plain", "Invalid mode");
}


// ================================================================
// WEB GATE CONTROL
// ================================================================

void handleGate() {

  // ------------------------------------------------
  // Must be manual
  // ------------------------------------------------

  if (
    systemMode !=
    MODE_MANUAL
  ) {

    server.send(
      403,
      "text/plain",
      "MANUAL MODE REQUIRED"
    );

    return;
  }


  if (
    !server.hasArg(
      "action"
    )
  ) {

    server.send(
      400,
      "text/plain",
      "Missing action"
    );

    return;
  }


  String action =
    server.arg(
      "action"
    );


  if (
    action ==
    "open"
  ) {

    openGate(
      "Web manual control"
    );

  }

  else if (
    action ==
    "close"
  ) {

    closeGate(
      "Web manual control"
    );

  }

  else {

    server.send(
      400,
      "text/plain",
      "Invalid action"
    );

    return;
  }


  server.send(
    200,
    "text/plain",
    "OK"
  );
}


// ================================================================
// WEB TIMER HANDLER
// ================================================================

void handleTimer() {

  // ------------------------------------------------
  // CANCEL
  // ------------------------------------------------

  if (
    server.hasArg(
      "action"
    ) &&
    server.arg(
      "action"
    ) ==
    "cancel"
  ) {

    if (
      timerActive
    ) {

      timerActive =
        false;

      timerAction =
        "";

      addHistory(
        "Gate timer cancelled"
      );


      showEvent(
        "TIMER CANCEL",
        "SCHEDULE REMOVED",
        "SYSTEM READY"
      );
    }


    server.send(
      200,
      "text/plain",
      "OK"
    );

    return;
  }


  // ------------------------------------------------
  // PARAMETERS
  // ------------------------------------------------

  if (
    !server.hasArg(
      "action"
    ) ||
    !server.hasArg(
      "minutes"
    )
  ) {

    server.send(
      400,
      "text/plain",
      "Missing timer parameters"
    );

    return;
  }


  String action =
    server.arg(
      "action"
    );


  int minutes =
    server.arg(
      "minutes"
    ).toInt();


  // ------------------------------------------------
  // Validate
  // ------------------------------------------------

  if (
    minutes < 1 ||
    minutes > 1440
  ) {

    server.send(
      400,
      "text/plain",
      "Timer must be 1-1440 minutes"
    );

    return;
  }


  if (
    action != "open" &&
    action != "close"
  ) {

    server.send(
      400,
      "text/plain",
      "Invalid action"
    );

    return;
  }


  // ------------------------------------------------
  // Create timer
  // ------------------------------------------------

  timerActive =
    true;


  timerAction =
    action;


  timerTargetMillis =
    millis() +
    (
      (unsigned long)
      minutes *
      60000UL
    );


  addHistory(
    "Timer set: " +
    action +
    " in " +
    String(minutes) +
    " minutes"
  );


  showEvent(
    "TIMER SET",
    action == "open"
    ? "GATE WILL OPEN"
    : "GATE WILL CLOSE",
    String(minutes) +
    " MINUTES"
  );


  server.send(
    200,
    "text/plain",
    "OK"
  );
}


// ================================================================
// WEB DATA
// ================================================================

void handleData() {

  String json = "{";


  // ------------------------------------------------
  // Gate
  // ------------------------------------------------

  json += "\"gate\":\"";

  json +=
    gateState ==
    GATE_OPEN
    ? "OPEN"
    : "CLOSED";

  json += "\",";


  // ------------------------------------------------
  // Mode
  // ------------------------------------------------

  json += "\"mode\":\"";

  json +=
    systemMode ==
    MODE_AUTO
    ? "AUTO"
    : "MANUAL";

  json += "\",";


  // ------------------------------------------------
  // Field
  // ------------------------------------------------

  json += "\"field\":\"";

  json +=
    levelName(
      fieldLevel
    );

  json += "\",";


  // ------------------------------------------------
  // Channel
  // ------------------------------------------------

  json += "\"channel\":\"";

  json +=
    levelName(
      channelLevel
    );

  json += "\",";


  // ------------------------------------------------
  // Numerical levels
  // ------------------------------------------------

  json += "\"fieldLevel\":";

  json +=
    fieldLevel;

  json += ",";


  json += "\"channelLevel\":";

  json +=
    channelLevel;

  json += ",";


  // ------------------------------------------------
  // Trends
  // ------------------------------------------------

  json += "\"fieldTrend\":\"";

  json +=
    fieldTrend;

  json += "\",";


  json += "\"channelTrend\":\"";

  json +=
    channelTrend;

  json += "\",";


  // ------------------------------------------------
  // Runoff
  // ------------------------------------------------

  json += "\"runoff\":";

  json +=
    runoffDetected
    ? "true"
    : "false";

  json += ",";


  // ------------------------------------------------
  // Drain problem
  // ------------------------------------------------

  json += "\"problem\":";

  json +=
    drainageProblem
    ? "true"
    : "false";

  json += ",";


  // ------------------------------------------------
  // Reason
  // ------------------------------------------------

  json += "\"reason\":\"";

  json +=
    escapeJSON(
      runoffReason
    );

  json += "\",";


  // ------------------------------------------------
  // Drainage
  // ------------------------------------------------

  json += "\"drainage\":\"";


  if (
    drainageProblem
  ) {

    json +=
      "WARNING";

  }

  else if (
    gateState ==
    GATE_OPEN
  ) {

    json +=
      "ACTIVE";

  }

  else if (
    drainageSuccessful
  ) {

    json +=
      "SUCCESSFUL";

  }

  else {

    json +=
      "IDLE";
  }


  json += "\",";


  // ------------------------------------------------
  // IP
  // ------------------------------------------------

  json += "\"ip\":\"";

  json +=
    WiFi.localIP().toString();

  json += "\",";


  // ------------------------------------------------
  // Web override
  // ------------------------------------------------

  json += "\"override\":";

  json +=
    webModeOverride
    ? "true"
    : "false";

  json += ",";


  json += "\"overrideMode\":\"";

  json +=
    webSelectedMode ==
    MODE_AUTO
    ? "AUTO"
    : "MANUAL";

  json += "\",";


  // ------------------------------------------------
  // Timer
  // ------------------------------------------------

  json += "\"timer\":";

  json +=
    timerActive
    ? "true"
    : "false";

  json += ",";


  json += "\"timerAction\":\"";

  json +=
    escapeJSON(
      timerAction
    );

  json += "\",";


  json += "\"timerRemaining\":\"";

  json +=
    timerRemaining();

  json += "\"";


  // ------------------------------------------------
  // Finish
  // ------------------------------------------------

  json += "}";


  server.send(
    200,
    "application/json",
    json
  );
}


// ================================================================
// WEB HISTORY
// ================================================================

void handleHistory() {

  String json = "[";


  for (
    int i = 0;
    i < historyCount;
    i++
  ) {

    json += "{";


    json += "\"event\":\"";

    json +=
      escapeJSON(
        historyEvent[i]
      );

    json += "\",";


    json += "\"time\":\"";

    json +=
      escapeJSON(
        historyTime[i]
      );

    json += "\"";


    json += "}";


    if (
      i <
      historyCount - 1
    ) {

      json += ",";
    }
  }


  json += "]";


  server.send(
    200,
    "application/json",
    json
  );
}


// ================================================================
// WEB PAGE
// ================================================================

String webPage() {

  String html = R"rawliteral(

<!DOCTYPE html>

<html>

<head>

<meta charset="UTF-8">

<meta
name="viewport"
content="width=device-width,initial-scale=1"
>

<title>AgriGate</title>
<link rel="icon" type="image/svg+xml" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 64 64'%3E%3Crect width='64' height='64' rx='16' fill='%232f7d32'/%3E%3Cpath d='M32 56V10M32 20C22 18 16 12 14 6M32 29C42 27 48 21 50 15M32 38C23 36 18 31 16 25M32 47C41 45 46 40 48 34' fill='none' stroke='white' stroke-width='4' stroke-linecap='round'/%3E%3C/svg%3E">


<style>

/* ============================================================
   AGRIGATE LIGHT AGRICULTURAL UI
   ============================================================ */

@import url(
'https://fonts.googleapis.com/css2?family=DM+Sans:wght@400;500;600;700&family=Plus+Jakarta+Sans:wght@500;600;700;800&display=swap'
);


:root{

--bg:#f4f8ef;

--white:#ffffff;

--green:#2f7d32;

--green2:#4f9b43;

--greenLight:#e0f0d9;

--greenPale:#f0f7eb;

--gold:#d39e25;

--goldLight:#fff2cc;

--red:#d64b4b;

--redLight:#fbe2e0;

--blue:#347d8d;

--blueLight:#e1f1f4;

--text:#213128;

--muted:#718078;

--border:#dce6d8;

--shadow:
0 8px 30px
rgba(38,75,39,.07);

}


*{

box-sizing:border-box;

}


body{

margin:0;

background:
linear-gradient(
135deg,
#f8fbf5 0%,
#edf5e6 100%
);

color:var(--text);

font-family:
"DM Sans",
Arial,
sans-serif;

}


.topbar{

height:78px;

display:flex;

align-items:center;

justify-content:space-between;

padding:
0 28px;

background:
rgba(255,255,255,.94);

border-bottom:
1px solid var(--border);

position:sticky;

top:0;

z-index:30;

backdrop-filter:
blur(14px);

}


.brand{

display:flex;

align-items:center;

gap:12px;

}


.logo{

width:47px;

height:47px;

border-radius:15px;

display:flex;

align-items:center;

justify-content:center;

background:
linear-gradient(
145deg,
#4f9d43,
#28702d
);

box-shadow:
0 8px 20px
rgba(47,125,50,.20);

}


.logo svg{

width:29px;

height:29px;

stroke:white;

fill:none;

stroke-width:2;

stroke-linecap:round;

stroke-linejoin:round;

}


.brandText strong{

display:block;

font-family:
"Plus Jakarta Sans",
Arial,
sans-serif;

font-size:18px;

font-weight:800;

}


.brandText span{

font-size:11px;

color:var(--muted);

}


.clock{

text-align:right;

}


.clock strong{

display:block;

font-size:14px;

}


.clock span{

font-size:11px;

color:var(--muted);

}


.nav{

display:flex;

gap:7px;

padding:
13px 22px;

overflow-x:auto;

background:white;

border-bottom:
1px solid var(--border);

}


.nav button{

border:0;

background:none;

color:#748177;

padding:
10px 15px;

border-radius:11px;

font-family:inherit;

font-weight:700;

cursor:pointer;

white-space:nowrap;

}


.nav button:hover{

background:
var(--greenPale);

}


.nav button.active{

background:
var(--greenLight);

color:var(--green);

}


.page{

display:none;

max-width:1200px;

margin:auto;

padding:
25px 22px 50px;

}


.page.active{

display:block;

}


.hero{

position:relative;

overflow:hidden;

padding:28px;

border-radius:25px;

color:white;

background:
linear-gradient(
135deg,
#28622d,
#57984a
);

box-shadow:
0 17px 38px
rgba(49,104,45,.18);

}


.hero h1{

margin:0;

font-family:
"Plus Jakarta Sans",
Arial,
sans-serif;

font-size:29px;

font-weight:800;

}


.hero p{

margin:
8px 0 0;

opacity:.84;

font-size:13px;

}


.paddy{

position:absolute;

right:25px;

bottom:0;

width:160px;

height:100px;

opacity:.22;

}


.paddy path{

fill:none;

stroke:white;

stroke-width:2;

stroke-linecap:round;

}


.grid{

display:grid;

grid-template-columns:
repeat(
auto-fit,
minmax(210px,1fr)
);

gap:16px;

margin-top:17px;

}


.card{

background:
rgba(255,255,255,.97);

border:
1px solid var(--border);

border-radius:19px;

padding:20px;

box-shadow:
var(--shadow);

}


.card h2{

margin:
0 0 10px;

font-family:
"Plus Jakarta Sans",
Arial,
sans-serif;

font-size:20px;

}


.card h3{

margin:
0 0 13px;

font-family:
"Plus Jakarta Sans",
Arial,
sans-serif;

font-size:15px;

}


.metric{

font-family:
"Plus Jakarta Sans",
Arial,
sans-serif;

font-size:30px;

font-weight:800;

}


.muted{

color:var(--muted);

font-size:12px;

}


.iconBox{

width:43px;

height:43px;

border-radius:13px;

display:flex;

align-items:center;

justify-content:center;

background:
var(--greenLight);

margin-bottom:12px;

}


.iconBox svg{

width:23px;

height:23px;

stroke:var(--green);

fill:none;

stroke-width:2;

stroke-linecap:round;

stroke-linejoin:round;

}


.progress{

height:8px;

margin-top:13px;

background:#edf2e9;

border-radius:20px;

overflow:hidden;

}


.progress i{

display:block;

height:100%;

border-radius:20px;

background:
linear-gradient(
90deg,
#72ad61,
#2e7d32
);

transition:
width .5s;

}


.pill{

display:inline-flex;

align-items:center;

padding:
6px 10px;

border-radius:30px;

font-size:10px;

font-weight:800;

margin-top:9px;

}


.green{

background:
var(--greenLight);

color:
#2e7131;

}


.red{

background:
var(--redLight);

color:
#b63838;

}


.gold{

background:
var(--goldLight);

color:
#86640d;

}


.blue{

background:
var(--blueLight);

color:
#347385;

}


.gateCard{

text-align:center;

}


.gateCircle{

width:112px;

height:112px;

border-radius:50%;

margin:
0 auto 13px;

display:flex;

align-items:center;

justify-content:center;

background:#f8fcf5;

border:
8px solid
var(--greenLight);

}


.gateCircle svg{

width:54px;

height:54px;

stroke:
var(--green);

fill:none;

stroke-width:1.6;

stroke-linecap:round;

stroke-linejoin:round;

}


.buttons{

display:grid;

grid-template-columns:
1fr 1fr;

gap:10px;

margin-top:14px;

}


.btn{

border:0;

border-radius:12px;

padding:14px;

font-family:inherit;

font-weight:800;

cursor:pointer;

}


.btnOpen{

background:
var(--greenLight);

color:
#2a7130;

}


.btnClose{

background:
var(--redLight);

color:
#b33737;

}


.btnPrimary{

background:
var(--green);

color:white;

}


.btnGold{

background:
var(--goldLight);

color:
#76590c;

}


.btnSoft{

background:
#edf2eb;

color:
#536259;

}


.alert{

margin-top:16px;

padding:16px;

border-radius:15px;

font-size:13px;

}


.alert.warning{

background:
#fff4d5;

border:
1px solid #eddca5;

color:#745b17;

}


.alert.danger{

background:
#fde5e3;

border:
1px solid #efc0bd;

color:#a63131;

}


.alert.success{

background:
#e4f2df;

border:
1px solid #c9e3c2;

color:#2e7131;

}


.timerBox{

padding:18px;

border-radius:16px;

background:#fffaf0;

border:
1px solid #eadcae;

}


.timerGrid{

display:grid;

grid-template-columns:
1fr 1fr;

gap:10px;

}


input,
select{

width:100%;

padding:13px;

border:
1px solid var(--border);

border-radius:11px;

background:white;

color:var(--text);

font-family:inherit;

}


.timerActive{

display:none;

margin-top:15px;

}


.history{

display:flex;

flex-direction:column;

gap:9px;

}


.event{

display:flex;

gap:13px;

padding:14px;

border:
1px solid var(--border);

border-radius:14px;

background:white;

}


.eventDot{

width:9px;

height:9px;

border-radius:50%;

background:
var(--green);

margin-top:5px;

flex:none;

}


.event strong{

font-size:13px;

}


.event small{

display:block;

margin-top:4px;

font-size:10px;

color:var(--muted);

}


.footer{

text-align:center;

padding:20px;

font-size:11px;

color:#8a968c;

}


/* ============================================================
   TASK OVERLAY
   ============================================================ */

.taskOverlay{

position:fixed;

inset:0;

z-index:100;

display:none;

align-items:center;

justify-content:center;

text-align:center;

background:
rgba(255,255,255,.97);

}


.taskOverlay.show{

display:flex;

}


.taskInner{

padding:30px;

}


.taskIcon{

width:95px;

height:95px;

margin:auto;

display:flex;

align-items:center;

justify-content:center;

border-radius:28px;

background:
var(--greenLight);

}


.taskIcon svg{

width:50px;

height:50px;

stroke:
var(--green);

fill:none;

stroke-width:1.7;

stroke-linecap:round;

stroke-linejoin:round;

}


.taskInner h2{

font-family:
"Plus Jakarta Sans";

font-size:29px;

margin:
20px 0 7px;

}


.taskInner p{

color:var(--muted);

}


@media(max-width:650px){

.topbar{

padding:
0 15px;

}

.clock{

display:none;

}

.page{

padding:
18px 13px 40px;

}

.hero h1{

font-size:23px;

}

.paddy{

display:none;

}

.buttons{

grid-template-columns:1fr;

}

.timerGrid{

grid-template-columns:1fr;

}

}

</style>

</head>


<body>


<!-- =========================================================
     HEADER
     ========================================================= -->

<header class="topbar">

<div class="brand">

<div class="logo">

<!-- Paddy SVG -->

<svg viewBox="0 0 32 32">

<path d="M7 27V7"/>

<path d="M7 15C13 15 18 11 22 5"/>

<path d="M7 21C13 21 18 18 24 12"/>

<path d="M14 24C19 24 23 21 26 17"/>

</svg>

</div>


<div class="brandText">

<strong>AgriGate</strong>

<span>
Intelligent Paddy Water Management
</span>

</div>

</div>


<div class="clock">

<strong id="clock">
--:--:--
</strong>

<span id="date">
--
</span>

</div>

</header>


<!-- =========================================================
     NAVIGATION
     ========================================================= -->

<nav class="nav">

<button
class="active"
onclick="tab('dashboard',this)"
>
Overview
</button>


<button
onclick="tab('gate',this)"
>
Gate
</button>


<button
onclick="tab('water',this)"
>
Water
</button>








<button
onclick="tab('timer',this)"
>
Timer
</button>
<button
onclick="tab('settings',this)"
>
Settings
</button>




</nav>


<!-- =========================================================
     DASHBOARD
     ========================================================= -->

<section
id="dashboard"
class="page active"
>


<div class="hero">

<h1>
Paddy Field Control Center
</h1>

<p>
Smart irrigation control, drainage and crop protection.
</p>


<svg
class="paddy"
viewBox="0 0 180 110"
>

<path d="M10 105V45"/>

<path d="M10 75C55 75 85 50 120 5"/>

<path d="M10 90C55 90 105 65 155 20"/>

<path d="M35 105C75 105 115 85 165 50"/>

</svg>

</div>


<div class="grid">


<!-- FIELD -->

<div class="card">

<div class="iconBox">

<svg viewBox="0 0 24 24">

<path d="M12 3C12 3 5 10 5 15a7 7 0 0014 0c0-5-7-12-7-12z"/>

</svg>

</div>

<h3>
Field Water
</h3>

<div
id="field"
class="metric"
>
--
</div>


<div
id="fieldTrend"
class="pill blue"
>
STABLE
</div>


<div class="progress">

<i
id="fieldBar"
style="width:20%"
>
</i>

</div>

</div>


<!-- CHANNEL -->

<div class="card">

<div class="iconBox">

<svg viewBox="0 0 24 24">

<path d="M3 7c3 0 3 3 6 3s3-3 6-3 3 3 6 3"/>

<path d="M3 13c3 0 3 3 6 3s3-3 6-3 3 3 6 3"/>

<path d="M3 19c3 0 3 3 6 3s3-3 6-3 3 3 6 3"/>

</svg>

</div>

<h3>
Channel Water
</h3>

<div
id="channel"
class="metric"
>
--
</div>


<div
id="channelTrend"
class="pill blue"
>
STABLE
</div>


<div class="progress">

<i
id="channelBar"
style="width:20%"
>
</i>

</div>

</div>


<!-- GATE -->

<div class="card gateCard">

<div class="gateCircle">

<svg viewBox="0 0 64 64">

<path d="M18 10v44"/>

<path d="M46 10v44"/>

<path d="M18 20h28"/>

<path d="M18 44h28"/>

<path d="M25 20v24"/>

<path d="M32 20v24"/>

<path d="M39 20v24"/>

</svg>

</div>


<h3>
Gate
</h3>


<div
id="gateState"
class="metric"
>
--
</div>


<div
id="gatePill"
class="pill green"
>
LIVE
</div>

</div>


<!-- MODE -->

<div class="card">

<div class="iconBox">

<svg viewBox="0 0 24 24">

<circle
cx="12"
cy="12"
r="3"
/>

<path d="M12 2v3"/>

<path d="M12 19v3"/>

<path d="M2 12h3"/>

<path d="M19 12h3"/>

<path d="M4.9 4.9l2.1 2.1"/>

<path d="M17 17l2.1 2.1"/>

<path d="M19.1 4.9L17 7"/>

<path d="M7 17l-2.1 2.1"/>

</svg>

</div>


<h3>
Operating Mode
</h3>


<div
id="mode"
class="metric"
>
--
</div>


<div
id="overridePill"
class="pill blue"
>
PHYSICAL SWITCH
</div>


<p
id="wifi"
class="muted"
>
IP: --
</p>

</div>

</div>


<div id="alert"></div>


</section>


<!-- =========================================================
     GATE
     ========================================================= -->

<section
id="gatePage"
class="page"
>


<div class="card">

<h2>
Gate Control
</h2>

<p class="muted">
Manual gate controls are available when MANUAL control is active.
</p>


<div class="buttons">

<button
class="btn btnOpen"
onclick="gateControl('open')"
>
OPEN GATE
</button>


<button
class="btn btnClose"
onclick="gateControl('close')"
>
CLOSE GATE
</button>

</div>

</div>


<br>


<!-- WEB OVERRIDE -->

<div class="card">

<h2>
Control Priority
</h2>

<p class="muted">
Override the physical AUTO/MANUAL switch directly from the web interface.
</p>


<div class="buttons">

<button
class="btn btnOpen"
onclick="setMode('manual')"
>
WEB MANUAL
</button>


<button
class="btn btnPrimary"
onclick="setMode('auto')"
>
WEB AUTO
</button>

</div>


<button
class="btn btnSoft"
style="width:100%;margin-top:10px"
onclick="setMode('physical')"
>
USE PHYSICAL SWITCH
</button>


<div
id="overrideStatus"
class="pill blue"
>
PHYSICAL SWITCH ACTIVE
</div>

</div>


<br>


<div class="card">

<h3>
Current Gate State
</h3>

<p>
Position:
<strong id="gateStatus">
--
</strong>
</p>


<p>
Control:
<strong id="gateMode">
--
</strong>
</p>

</div>


</section>


<!-- =========================================================
     WATER
     ========================================================= -->

<section
id="waterPage"
class="page"
>


<div class="grid">


<div class="card">

<h3>
Channel Water
</h3>

<div
id="channelW"
class="metric"
>
--
</div>

<p>
Trend:
<strong id="channelT">
--
</strong>
</p>

</div>


<div class="card">

<h3>
Field Water
</h3>

<div
id="fieldW"
class="metric"
>
--
</div>

<p>
Trend:
<strong id="fieldT"
>
--
</strong>
</p>

</div>

</div>


<br>


<div class="card">

<h3>
Drainage State
</h3>

<p id="drain">
--
</p>

</div>


<br>

<div class="card">

<h3>
Water Intelligence
</h3>

<p>
Status:
<strong id="runoffStatus">
NORMAL
</strong>
</p>

<p>
Reason:
<strong id="reason">
System operating normally
</strong>
</p>

<p>
Drainage:
<strong id="drain">
IDLE
</strong>
</p>

<p class="muted">
Channel MEDIUM/HIGH + Field LOW opens the gate for water supply.
Field HIGH opens the gate for automatic drainage.
</p>

</div>

</section>


<!-- =========================================================
     RUNOFF
     ========================================================= -->

<!-- =========================================================
     CROP
     ========================================================= -->

<!-- =========================================================
     TIMER
     ========================================================= -->

<section
id="timerPage"
class="page"
>


<div class="card">

<h2>
Gate Timer
</h2>

<p class="muted">
Create a timer only when you want a delayed gate action.
</p>


<br>


<div class="timerBox">


<div class="timerGrid">


<select id="timerAction">

<option value="open">
OPEN GATE
</option>

<option value="close">
CLOSE GATE
</option>

</select>


<input
id="timerMinutes"
type="number"
min="1"
max="1440"
placeholder="Minutes"
>


</div>


<br>


<button
class="btn btnGold"
style="width:100%"
onclick="setTimer()"
>
APPLY TIMER
</button>

</div>


<div
id="timerActive"
class="timerActive alert warning"
>


<strong>
TIMER ACTIVE
</strong>


<p id="timerText">
--
</p>


<button
class="btn btnSoft"
onclick="cancelTimer()"
>
CANCEL TIMER
</button>

</div>

</div>


</section>


<!-- =========================================================
     HISTORY
     ========================================================= -->

<!-- =========================================================
     SETTINGS
     ========================================================= -->

<section
id="settingsPage"
class="page"
>

<div class="card">

<h2>
System Settings
</h2>

<p class="muted">
Configure control priority and review the automatic water rules.
</p>

<br>

<h3>
Control Priority
</h3>

<div class="buttons">

<button
class="btn btnOpen"
onclick="setMode('manual')"
>
WEB MANUAL
</button>

<button
class="btn btnPrimary"
onclick="setMode('auto')"
>
WEB AUTO
</button>

</div>

<button
class="btn btnSoft"
style="width:100%;margin-top:10px"
onclick="setMode('physical')"
>
USE PHYSICAL SWITCH
</button>

<div
id="settingsOverride"
class="pill blue"
>
PHYSICAL SWITCH ACTIVE
</div>

</div>

<br>

<div class="grid">

<div class="card">

<h3>
Automatic Water Supply
</h3>

<p>
<strong>Channel MEDIUM + Field LOW</strong>
</p>

<p class="muted">
Gate opens automatically to supply available channel water to the field.
</p>

<p>
<strong>Channel HIGH + Field LOW</strong>
</p>

<p class="muted">
Gate opens automatically because channel water is available while the field is low.
</p>

</div>

<div class="card">

<h3>
Automatic Drainage
</h3>

<p>
<strong>Field HIGH</strong>
</p>

<p class="muted">
Gate opens automatically to drain excess field water.
</p>

<p>
<strong>Field LOW + Channel LOW</strong>
</p>

<p class="muted">
Gate closes when both the field and channel are low.
</p>

</div>

</div>

<br>

<div class="card">

<h3>
Hardware & Network
</h3>

<p>
Controller:
<strong>ESP32 Dev Module</strong>
</p>

<p>
Water monitor:
<strong>Arduino Nano</strong>
</p>

<p>
OLED:
<strong>128 × 64</strong>
</p>

<p>
Servo:
<strong>0° OPEN / 180° CLOSED</strong>
</p>

<p>
IP Address:
<strong id="settingsIP">
--
</strong>
</p>

<p>
Current Control:
<strong id="settingsControl">
--
</strong>
</p>

</div>

</section>


<!-- =========================================================
     FOOTER
     ========================================================= -->

<div class="footer">

AgriGate • Intelligent Paddy Water Management

</div>


<!-- =========================================================
     TASK OVERLAY
     ========================================================= -->

<div
id="taskOverlay"
class="taskOverlay"
>


<div class="taskInner">


<div class="taskIcon">

<svg viewBox="0 0 64 64">

<path d="M18 10v44"/>

<path d="M46 10v44"/>

<path d="M18 20h28"/>

<path d="M18 44h28"/>

<path d="M25 20v24"/>

<path d="M32 20v24"/>

<path d="M39 20v24"/>

</svg>

</div>


<h2 id="taskTitle">
PROCESSING
</h2>


<p id="taskText">
Please wait...
</p>

</div>

</div>


<!-- =========================================================
     JAVASCRIPT
     ========================================================= -->

<script>


// ============================================================
// TAB CONTROL
// ============================================================

function tab(
  name,
  button
){

document
.querySelectorAll('.page')
.forEach(
  x =>
  x.classList.remove('active')
);


const pages = {

dashboard:
'dashboard',

gate:
'gatePage',

water:
'waterPage',

timer:
'timerPage',

settings:
'settingsPage'

};


document
.getElementById(
  pages[name]
)
.classList.add('active');


document
.querySelectorAll('.nav button')
.forEach(
  x =>
  x.classList.remove('active')
);


button.classList.add('active');

}


// ============================================================
// GATE CONTROL
// ============================================================

function gateControl(
  action
){

fetch(
  '/gate?action=' +
  action
)

.then(
  r => r.text()
)

.then(
  result => {

    if(
      result != 'OK'
    ){

      alert(result);

    }

  }
);

}


// ============================================================
// MODE CONTROL
// ============================================================

function setMode(
  mode
){

fetch(
  '/mode?action=' +
  mode
)

.then(
  r => r.text()
)

.then(
  result => {

    if(
      result != 'OK'
    ){

      alert(result);

    }

  }
);

}


// ============================================================
// TIMER SET
// ============================================================

function setTimer(){

let action =
document
.getElementById(
  'timerAction'
)
.value;


let minutes =
parseInt(
document
.getElementById(
  'timerMinutes'
)
.value
);


if(
  !minutes ||
  minutes < 1
){

alert(
  'Enter a valid timer duration.'
);

return;

}


fetch(
  '/timer?action=' +
  action +
  '&minutes=' +
  minutes
)

.then(
  r => r.text()
)

.then(
  result => {

    if(
      result != 'OK'
    ){

      alert(result);

    }

  }
);

}


// ============================================================
// TIMER CANCEL
// ============================================================

function cancelTimer(){

fetch(
  '/timer?action=cancel'
);

}


// ============================================================
// CLOCK
// ============================================================

function updateClock(){

const now =
new Date();


document
.getElementById(
  'clock'
)
.innerHTML =
now.toLocaleTimeString();


document
.getElementById(
  'date'
)
.innerHTML =
now.toLocaleDateString(
  undefined,
  {
    weekday:'long',
    year:'numeric',
    month:'long',
    day:'numeric'
  }
);

}


// ============================================================
// MAIN DATA UPDATE
// ============================================================

function update(){

fetch('/data')

.then(
  r => r.json()
)

.then(
  d => {


    // --------------------------------------------------------
    // MAIN VALUES
    // --------------------------------------------------------

    document
    .getElementById('field')
    .innerHTML =
    d.field;


    document
    .getElementById('fieldW')
    .innerHTML =
    d.field;


    document
    .getElementById('channel')
    .innerHTML =
    d.channel;


    document
    .getElementById('channelW')
    .innerHTML =
    d.channel;


    document
    .getElementById('gateState')
    .innerHTML =
    d.gate;


    document
    .getElementById('gateStatus')
    .innerHTML =
    d.gate;


    document
    .getElementById('mode')
    .innerHTML =
    d.mode;


    document
    .getElementById('gateMode')
    .innerHTML =
    d.mode;


    document
    .getElementById('wifi')
    .innerHTML =
    'IP: ' + d.ip;


    // --------------------------------------------------------
    // TRENDS
    // --------------------------------------------------------

    document
    .getElementById('fieldTrend')
    .innerHTML =
    d.fieldTrend;


    document
    .getElementById('fieldT')
    .innerHTML =
    d.fieldTrend;


    document
    .getElementById('channelTrend')
    .innerHTML =
    d.channelTrend;


    document
    .getElementById('channelT')
    .innerHTML =
    d.channelTrend;
document
    .getElementById('reason')
    .innerHTML =
    d.reason;


    document
    .getElementById('drain')
    .innerHTML =
    d.drainage;
// --------------------------------------------------------
    // WATER BARS
    // --------------------------------------------------------

    document
    .getElementById('fieldBar')
    .style.width =
    ((d.fieldLevel + 1) * 33.3)
    + '%';


    document
    .getElementById('channelBar')
    .style.width =
    ((d.channelLevel + 1) * 33.3)
    + '%';


    // --------------------------------------------------------
    // GATE PILL
    // --------------------------------------------------------

    let gatePill =
    document
    .getElementById(
      'gatePill'
    );


    gatePill.innerHTML =
    d.gate;


    gatePill.className =
    'pill ' +
    (
      d.gate == 'OPEN'
      ? 'green'
      : 'red'
    );


    // --------------------------------------------------------
    // OVERRIDE STATUS
    // --------------------------------------------------------

    let override =
    document
    .getElementById(
      'overrideStatus'
    );


    let overrideTop =
    document
    .getElementById(
      'overridePill'
    );


    if(
      d.override
    ){

      override.innerHTML =
      'WEB OVERRIDE: ' +
      d.overrideMode;


      override.className =
      'pill gold';


      overrideTop.innerHTML =
      'WEB ' +
      d.overrideMode;


      overrideTop.className =
      'pill gold';

    }

    else{

      override.innerHTML =
      'PHYSICAL SWITCH ACTIVE';


      override.className =
      'pill blue';


      overrideTop.innerHTML =
      'PHYSICAL SWITCH';


      overrideTop.className =
      'pill blue';

    }


    document
    .getElementById(
      'settingsOverride'
    )
    .innerHTML =
    d.override
    ? 'WEB OVERRIDE: ' + d.overrideMode
    : 'PHYSICAL SWITCH ACTIVE';

    document
    .getElementById(
      'settingsOverride'
    )
    .className =
    d.override
    ? 'pill gold'
    : 'pill blue';

    document
    .getElementById(
      'settingsIP'
    )
    .innerHTML =
    d.ip;

    document
    .getElementById(
      'settingsControl'
    )
    .innerHTML =
    d.mode;


    // --------------------------------------------------------
    // RUNOFF
    // --------------------------------------------------------

    let runoff =
    document
    .getElementById(
      'runoffStatus'
    );


    let alertBox =
    document
    .getElementById(
      'alert'
    );


    if(
      d.problem
    ){

      runoff.innerHTML =
      'DRAINAGE WARNING';


      runoff.className =
      'pill red';


      alertBox.innerHTML =
      '<div class="alert danger">' +
      '<strong>Possible drainage obstruction.</strong><br>' +
      d.reason +
      '</div>';

    }

    else if(
      d.runoff
    ){

      runoff.innerHTML =
      'RUNOFF DETECTED';


      runoff.className =
      'pill gold';


      alertBox.innerHTML =
      '<div class="alert warning">' +
      '<strong>Possible rain / runoff detected.</strong><br>' +
      d.reason +
      '</div>';

    }

    else{

      runoff.innerHTML =
      'NORMAL';


      runoff.className =
      'pill green';


      alertBox.innerHTML =
      '';

    }


    // --------------------------------------------------------
    // TIMER
    // --------------------------------------------------------

    let timerBox =
    document
    .getElementById(
      'timerActive'
    );


    if(
      d.timer
    ){

      timerBox.style.display =
      'block';


      document
      .getElementById(
        'timerText'
      )
      .innerHTML =
      d.timerAction.toUpperCase() +
      ' GATE IN ' +
      d.timerRemaining;

    }

    else{

      timerBox.style.display =
      'none';

    }

  }
)

.catch(
  () => {}
);

}


// ============================================================
// HISTORY
// ============================================================

function loadHistory(){

fetch('/history')

.then(
  r => r.json()
)

.then(
  data => {

    let html = '';


    data.forEach(
      event => {

        html +=
        '<div class="event">' +

        '<div class="eventDot"></div>' +

        '<div>' +

        '<strong>' +
        event.event +
        '</strong>' +

        '<small>' +
        event.time +
        '</small>' +

        '</div>' +

        '</div>';

      }
    );


    document
    .getElementById(
      'historyList'
    )
    .innerHTML =
    html ||
    'No events recorded yet.';

  }
)

.catch(
  () => {}
);

}


// ============================================================
// START
// ============================================================

updateClock();

update();

setInterval(
  updateClock,
  1000
);


setInterval(
  update,
  1000
);




</script>


</body>

</html>

)rawliteral";


  return html;
}


// ================================================================
// SETUP
// ================================================================

void setup() {

  Serial.begin(
    115200
  );


  // ==============================================================
  // PIN SETUP
  // ==============================================================

  pinMode(
    GREEN_LED,
    OUTPUT
  );

  pinMode(
    RED_LED,
    OUTPUT
  );


  pinMode(
    OPEN_BUTTON,
    INPUT_PULLUP
  );

  pinMode(
    CLOSE_BUTTON,
    INPUT_PULLUP
  );


  pinMode(
    MANUAL_SWITCH,
    INPUT_PULLUP
  );

  pinMode(
    AUTO_SWITCH,
    INPUT_PULLUP
  );


  // ==============================================================
  // SERVO
  // ==============================================================

  gateServo.attach(
    SERVO_PIN
  );


  // Start CLOSED

  gateServo.write(
    180
  );


  gateState =
    GATE_CLOSED;


  digitalWrite(
    GREEN_LED,
    LOW
  );

  digitalWrite(
    RED_LED,
    HIGH
  );


  // ==============================================================
  // OLED
  // ==============================================================

  Wire.begin(
    OLED_SDA,
    OLED_SCL
  );


  if (
    !display.begin(
      SSD1306_SWITCHCAPVCC,
      OLED_ADDRESS
    )
  ) {

    Serial.println(
      "OLED INITIALIZATION FAILED"
    );

  }


  // ==============================================================
  // NVS HISTORY
  // ==============================================================

  loadHistory();


  // ==============================================================
  // NANO SERIAL
  // ==============================================================

  NanoSerial.begin(
    9600,
    SERIAL_8N1,
    NANO_RX,
    NANO_TX
  );


  // ==============================================================
  // WIFI STARTUP SCREEN
  // ==============================================================

  display.clearDisplay();

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setTextSize(1);

  display.setCursor(
    0,
    10
  );

  display.println(
    "CONNECTING WIFI..."
  );

  display.display();


  // ==============================================================
  // WIFI
  // ==============================================================

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );


  Serial.print(
    "Connecting WiFi"
  );


  int attempts = 0;


  while (
    WiFi.status() !=
    WL_CONNECTED &&
    attempts < 30
  ) {

    delay(500);

    Serial.print(
      "."
    );

    attempts++;
  }


  Serial.println();


  // ==============================================================
  // TIME
  // ==============================================================

  configTime(
    GMT_OFFSET_SEC,
    DAYLIGHT_OFFSET_SEC,
    NTP_SERVER
  );


  // ==============================================================
  // HISTORY
  // ==============================================================

  addHistory(
    "System started"
  );


  // ==============================================================
  // WEB ROUTES
  // ==============================================================

  server.on(
    "/",
    []() {

      server.send(
        200,
        "text/html",
        webPage()
      );

    }
  );


  server.on(
    "/data",
    handleData
  );


  server.on(
    "/history",
    handleHistory
  );


  server.on(
    "/gate",
    handleGate
  );


  server.on(
    "/mode",
    handleMode
  );


  server.on(
    "/timer",
    handleTimer
  );


  // ==============================================================
  // START SERVER
  // ==============================================================

  server.begin();


  // ==============================================================
  // STARTUP IP
  // ==============================================================

  showStartupScreen();


  // ==============================================================
  // SERIAL INFORMATION
  // ==============================================================

  Serial.println();

  Serial.println(
    "======================================"
  );

  Serial.println(
    "       AGRIGATE WATER SYSTEM"
  );

  Serial.println(
    "======================================"
  );


  if (
    WiFi.status() ==
    WL_CONNECTED
  ) {

    Serial.print(
      "WEB ADDRESS: http://"
    );

    Serial.println(
      WiFi.localIP()
    );

  }

  else {

    Serial.println(
      "WIFI NOT CONNECTED"
    );
  }


  Serial.println(
    "SYSTEM READY"
  );

  Serial.println(
    "======================================"
  );
}


// ================================================================
// LOOP
// ================================================================

void loop() {


  // ==============================================================
  // NANO WATER DATA
  // ==============================================================

  readNano();


  // ==============================================================
  // MODE
  // ==============================================================

  updateMode();


  // ==============================================================
  // WATER ANALYSIS
  // ==============================================================

  analyzeWater();


  // ==============================================================
  // AUTOMATIC GATE
  // ==============================================================

  autoControl();


  // ==============================================================
  // MANUAL BUTTONS
  // ==============================================================

  checkButtons();


  // ==============================================================
  // GATE TIMER
  // ==============================================================

  checkTimer();


  // ==============================================================
  // OLED
  // ==============================================================

  updateOLED();


  // ==============================================================
  // WEB SERVER
  // ==============================================================

  server.handleClient();


  // ==============================================================
  // SERIAL STATUS
  // ==============================================================

  if (
    millis() -
    lastSerialStatus >=
    STATUS_INTERVAL
  ) {

    lastSerialStatus =
      millis();


    Serial.println(
      "--------------------------------"
    );


    Serial.print(
      "TIME: "
    );

    Serial.println(
      getDateTime()
    );


    Serial.print(
      "MODE: "
    );

    if (
      webModeOverride
    ) {

      Serial.print(
        "WEB-"
      );
    }


    Serial.println(
      systemMode ==
      MODE_AUTO
      ? "AUTO"
      : "MANUAL"
    );


    Serial.print(
      "GATE: "
    );

    Serial.println(
      gateState ==
      GATE_OPEN
      ? "OPEN"
      : "CLOSED"
    );


    Serial.print(
      "CHANNEL: "
    );

    Serial.println(
      levelName(
        channelLevel
      )
    );


    Serial.print(
      "FIELD: "
    );

    Serial.println(
      levelName(
        fieldLevel
      )
    );


    Serial.print(
      "FIELD TREND: "
    );

    Serial.println(
      fieldTrend
    );


    Serial.print(
      "CHANNEL TREND: "
    );

    Serial.println(
      channelTrend
    );


    Serial.print(
      "RUNOFF: "
    );

    Serial.println(
      runoffDetected
      ? "YES"
      : "NO"
    );


    Serial.print(
      "STATUS: "
    );

    Serial.println(
      runoffReason
    );


    if (
      timerActive
    ) {

      Serial.print(
        "TIMER: "
      );

      Serial.print(
        timerAction
      );

      Serial.print(
        " / "
      );

      Serial.println(
        timerRemaining()
      );
    }


    if (
      drainageProblem
    ) {

      Serial.println(
        "WARNING: POSSIBLE DRAINAGE OBSTRUCTION"
      );
    }
  }
}