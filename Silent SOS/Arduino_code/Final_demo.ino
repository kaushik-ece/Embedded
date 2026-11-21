/*
============================================================
              SILENT SOS - FINAL M3
============================================================

HARDWARE
------------------------------------------------------------
ESP32 32-PIN
MPU6050
OLED SSD1306 128x64
ALERT BUTTON
CANCEL / RESET BUTTON
RGB LED

COMMUNICATION
------------------------------------------------------------
WiFi
Firebase Realtime Database
Telegram Bot

STATE MACHINE
------------------------------------------------------------
SAFE -> ARMED -> DANGER

MANUAL TRIGGER
------------------------------------------------------------
Press ALERT 3 times within 2 seconds
        ↓
      ARMED
        ↓
5 second cancellation window
        ↓
      DANGER

AUTOMATIC TRIGGER
------------------------------------------------------------
Free fall + impact + rotation scoring

CANCEL / RESET BUTTON
------------------------------------------------------------
ARMED  -> SAFE
DANGER -> SAFE

FIREBASE
------------------------------------------------------------
devices/
   DEVICE_001/
      state
      trigger
      online
      wifi
      lastSeen

Phone GPS is handled separately by:
   index.html

Live dashboard:
   dashboard.html

============================================================
*/


// ============================================================
// FIREBASE SETTINGS
// ============================================================

#define ENABLE_USER_AUTH
#define ENABLE_DATABASE


// ============================================================
// LIBRARIES
// ============================================================

#include <Wire.h>

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <time.h>

#include <FirebaseClient.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>


// ============================================================
// WIFI
// ============================================================

#define WIFI_SSID       "Spidey"
#define WIFI_PASSWORD   "12345678"


// ============================================================
// FIREBASE
// ============================================================

#define API_KEY \
"AIzaSyDISjJUdnWNgtlaO7502IlzrkntAEzLBuM"

#define USER_EMAIL \
"kaushik7t7@gmail.com"

#define USER_PASSWORD \
"12345678"

#define DATABASE_URL \
"https://silent-sos-572ac-default-rtdb.asia-southeast1.firebasedatabase.app"


// ============================================================
// TELEGRAM
// ============================================================
//
// IMPORTANT:
// Replace BOT_TOKEN with your current BotFather token.
// Do not publish your real token on GitHub.
//
// ============================================================

#define BOT_TOKEN \
"REPLACE_WITH_YOUR_TELEGRAM_BOT_TOKEN"

#define CHAT_ID \
"7815830971"


// ============================================================
// DEVICE
// ============================================================

#define DEVICE_ID "DEVICE_001"


// ============================================================
// PIN CONFIGURATION
// ============================================================

#define SDA_PIN 21
#define SCL_PIN 22

#define ALERT_BUTTON_PIN 32
#define CANCEL_BUTTON_PIN 33

#define RED_PIN 25
#define GREEN_PIN 26
#define BLUE_PIN 27


// ============================================================
// OLED
// ============================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);


// ============================================================
// MPU6050
// ============================================================

Adafruit_MPU6050 mpu;

bool mpuAvailable = false;


// ============================================================
// RGB LED
// ============================================================

#define COMMON_CATHODE true


// ============================================================
// STATE MACHINE
// ============================================================

enum SystemState
{
  STATE_SAFE,
  STATE_ARMED,
  STATE_DANGER
};

SystemState currentState = STATE_SAFE;


// ============================================================
// TRIGGER TYPE
// ============================================================

enum TriggerType
{
  TRIGGER_NONE,
  TRIGGER_MANUAL,
  TRIGGER_AUTOMATIC
};

TriggerType currentTrigger = TRIGGER_NONE;


// ============================================================
// ARMED TIMER
// ============================================================

const unsigned long ARM_DURATION = 5000;

unsigned long armedStartTime = 0;

int lastCountdown = -1;


// ============================================================
// MANUAL TRIGGER
// ============================================================

int alertPressCount = 0;

unsigned long firstAlertPressTime = 0;

const unsigned long PRESS_WINDOW = 2000;


// ============================================================
// BUTTON DEBOUNCE
// ============================================================

bool alertLastReading = HIGH;
bool alertStableState = HIGH;

bool cancelLastReading = HIGH;
bool cancelStableState = HIGH;

unsigned long alertDebounceTime = 0;
unsigned long cancelDebounceTime = 0;

const unsigned long DEBOUNCE_TIME = 50;


// ============================================================
// MPU CALIBRATION
// ============================================================

const float ACCEL_X_OFFSET = -0.104125;
const float ACCEL_Y_OFFSET = -0.015647;
const float ACCEL_Z_OFFSET = -0.035401;

const float GYRO_X_OFFSET = -2.039589;
const float GYRO_Y_OFFSET = 1.185229;
const float GYRO_Z_OFFSET = -0.848420;


// ============================================================
// MPU THRESHOLDS
// ============================================================

const float FREE_FALL_THRESHOLD = 0.45;

const float IMPACT_THRESHOLD = 2.20;

const float GYRO_THRESHOLD = 250.0;


// ============================================================
// AUTOMATIC DETECTION
// ============================================================

const unsigned long IMPACT_WINDOW = 1000;

const unsigned long AUTOMATIC_COOLDOWN = 5000;

bool freeFallDetected = false;

unsigned long freeFallTime = 0;

int automaticScore = 0;

unsigned long lastAutomaticTrigger = 0;


// ============================================================
// MPU VALUES
// ============================================================

float accelX = 0;
float accelY = 0;
float accelZ = 0;

float gyroX = 0;
float gyroY = 0;
float gyroZ = 0;

float accelerationMagnitude = 0;

float gyroMagnitude = 0;


// ============================================================
// FIREBASE
// ============================================================

void processData(AsyncResult &aResult);

UserAuth user_auth(
  API_KEY,
  USER_EMAIL,
  USER_PASSWORD
);

FirebaseApp app;

WiFiClientSecure ssl_client;

using AsyncClient = AsyncClientClass;

AsyncClient aClient(ssl_client);

RealtimeDatabase Database;

bool firebaseReady = false;


// ============================================================
// TELEGRAM
// ============================================================

bool telegramConfigured = false;


// ============================================================
// FUNCTION DECLARATIONS
// ============================================================

// RGB
void setRGB(bool red, bool green, bool blue);

// OLED
void showSafeScreen();
void showArmedScreen(int seconds);
void showDangerScreen();

// Buttons
void readButtons();
void handleAlertPress();
void handleCancelPress();

// MPU
bool initializeMPU();
void readMPU();
void analyzeAutomaticMotion();

// Trigger
void triggerManual();
void triggerAutomatic();

// State
void enterSafe();
void enterArmed(TriggerType trigger);
void updateArmed();
void enterDanger();

// Firebase
void updateFirebaseState();
void updateFirebaseStatus();

// Telegram
void sendTelegram(String message);

// WiFi
void connectWiFi();

// Time
void initializeTime();
String getTimeString();


// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("========================================");
  Serial.println("          SILENT SOS - FINAL M3");
  Serial.println("========================================");
  Serial.println();


  // ==========================================================
  // GPIO
  // ==========================================================

  pinMode(
    ALERT_BUTTON_PIN,
    INPUT_PULLUP
  );

  pinMode(
    CANCEL_BUTTON_PIN,
    INPUT_PULLUP
  );

  pinMode(
    RED_PIN,
    OUTPUT
  );

  pinMode(
    GREEN_PIN,
    OUTPUT
  );

  pinMode(
    BLUE_PIN,
    OUTPUT
  );

  setRGB(false, false, false);


  // ==========================================================
  // I2C
  // ==========================================================

  Wire.begin(
    SDA_PIN,
    SCL_PIN
  );


  // ==========================================================
  // OLED
  // ==========================================================

  Serial.println("[OLED] Initializing...");

  if (
    !display.begin(
      SSD1306_SWITCHCAPVCC,
      OLED_ADDRESS
    )
  )
  {
    Serial.println("[OLED] ERROR");

    while (true)
    {
      setRGB(true, false, false);

      delay(200);

      setRGB(false, false, false);

      delay(200);
    }
  }

  Serial.println("[OLED] OK");


  // ==========================================================
  // MPU6050
  // ==========================================================

  mpuAvailable = initializeMPU();


  // ==========================================================
  // INITIAL SAFE STATE
  // ==========================================================

  currentState = STATE_SAFE;

  currentTrigger = TRIGGER_NONE;

  setRGB(false, true, false);

  showSafeScreen();


  // ==========================================================
  // WIFI
  // ==========================================================

  connectWiFi();


  // ==========================================================
  // NTP TIME
  // ==========================================================

  if (WiFi.status() == WL_CONNECTED)
  {
    initializeTime();
  }


  // ==========================================================
  // FIREBASE
  // ==========================================================

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println();
    Serial.println("[FIREBASE] Initializing...");

    ssl_client.setInsecure();

    initializeApp(
      aClient,
      app,
      getAuth(user_auth),
      processData,
      "authTask"
    );

    app.getApp<RealtimeDatabase>(
      Database
    );

    Database.url(
      DATABASE_URL
    );

    Serial.println(
      "[FIREBASE] Waiting for authentication..."
    );
  }


  // ==========================================================
  // TELEGRAM CHECK
  // ==========================================================

  if (
    WiFi.status() == WL_CONNECTED &&
    strcmp(
      BOT_TOKEN,
      "REPLACE_WITH_YOUR_TELEGRAM_BOT_TOKEN"
    ) != 0
  )
  {
    telegramConfigured = true;

    Serial.println(
      "[TELEGRAM] Token configured"
    );
  }
  else
  {
    Serial.println(
      "[TELEGRAM] Token NOT configured"
    );
  }


  // ==========================================================
  // READY
  // ==========================================================

  Serial.println();
  Serial.println("========================================");
  Serial.println("          SILENT SOS READY");
  Serial.println("========================================");

  Serial.println();

  Serial.println(
    "Manual trigger:"
  );

  Serial.println(
    "ALERT x3 within 2 seconds"
  );

  Serial.println();

  Serial.println(
    "Automatic trigger:"
  );

  Serial.println(
    "Free fall + impact + rotation"
  );

  Serial.println();

  Serial.println(
    "CANCEL:"
  );

  Serial.println(
    "ARMED -> SAFE"
  );

  Serial.println(
    "DANGER -> SAFE"
  );

  Serial.println();
}


// ============================================================
// MAIN LOOP
// ============================================================

void loop()
{
  // ==========================================================
  // FIREBASE BACKGROUND
  // ==========================================================

  if (
    WiFi.status() == WL_CONNECTED
  )
  {
    app.loop();
  }


  // ==========================================================
  // FIREBASE AUTHENTICATION
  // ==========================================================

  if (
    app.ready() &&
    !firebaseReady
  )
  {
    firebaseReady = true;

    Serial.println();
    Serial.println("========================================");
    Serial.println("[FIREBASE] AUTHENTICATED");
    Serial.println("========================================");

    updateFirebaseState();

    updateFirebaseStatus();
  }


  // ==========================================================
  // BUTTONS
  // ==========================================================

  readButtons();


  // ==========================================================
  // AUTOMATIC MPU DETECTION
  // ==========================================================

  if (
    currentState == STATE_SAFE &&
    mpuAvailable
  )
  {
    readMPU();

    analyzeAutomaticMotion();
  }


  // ==========================================================
  // STATE MACHINE
  // ==========================================================

  switch (currentState)
  {
    case STATE_SAFE:
      break;

    case STATE_ARMED:
      updateArmed();
      break;

    case STATE_DANGER:
      break;
  }


  delay(5);
}


// ============================================================
// WIFI
// ============================================================

void connectWiFi()
{
  Serial.println();
  Serial.println("[WIFI] Connecting...");

  WiFi.mode(WIFI_STA);

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );

  unsigned long startTime = millis();

  while (
    WiFi.status() != WL_CONNECTED &&
    millis() - startTime < 20000
  )
  {
    Serial.print(".");

    delay(500);
  }

  Serial.println();

  if (
    WiFi.status() == WL_CONNECTED
  )
  {
    Serial.println("[WIFI] Connected!");

    Serial.print("[WIFI] IP: ");

    Serial.println(
      WiFi.localIP()
    );
  }
  else
  {
    Serial.println(
      "[WIFI] CONNECTION FAILED"
    );
  }
}


// ============================================================
// NTP TIME
// ============================================================

void initializeTime()
{
  Serial.println(
    "[TIME] Synchronizing NTP..."
  );

  // India timezone = UTC + 5:30

  configTime(
    19800,
    0,
    "pool.ntp.org",
    "time.nist.gov"
  );

  struct tm timeinfo;

  unsigned long start = millis();

  while (
    !getLocalTime(
      &timeinfo
    ) &&
    millis() - start < 10000
  )
  {
    Serial.print(".");

    delay(500);
  }

  Serial.println();

  if (
    getLocalTime(
      &timeinfo
    )
  )
  {
    Serial.println(
      "[TIME] NTP synchronized"
    );

    Serial.println(
      getTimeString()
    );
  }
  else
  {
    Serial.println(
      "[TIME] NTP synchronization failed"
    );
  }
}


// ============================================================
// GET TIME
// ============================================================

String getTimeString()
{
  struct tm timeinfo;

  if (
    !getLocalTime(
      &timeinfo
    )
  )
  {
    return String(
      millis()
    );
  }

  char buffer[30];

  strftime(
    buffer,
    sizeof(buffer),
    "%Y-%m-%d %H:%M:%S",
    &timeinfo
  );

  return String(buffer);
}


// ============================================================
// RGB
// ============================================================

void setRGB(
  bool red,
  bool green,
  bool blue
)
{
#if COMMON_CATHODE

  digitalWrite(
    RED_PIN,
    red ? HIGH : LOW
  );

  digitalWrite(
    GREEN_PIN,
    green ? HIGH : LOW
  );

  digitalWrite(
    BLUE_PIN,
    blue ? HIGH : LOW
  );

#else

  digitalWrite(
    RED_PIN,
    red ? LOW : HIGH
  );

  digitalWrite(
    GREEN_PIN,
    green ? LOW : HIGH
  );

  digitalWrite(
    BLUE_PIN,
    blue ? LOW : HIGH
  );

#endif
}


// ============================================================
// OLED SAFE
// ============================================================

void showSafeScreen()
{
  display.clearDisplay();

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setTextSize(2);

  display.setCursor(
    25,
    5
  );

  display.println(
    "SAFE"
  );

  display.setTextSize(1);

  display.setCursor(
    15,
    35
  );

  display.println(
    "SYSTEM READY"
  );

  display.setCursor(
    18,
    50
  );

  display.println(
    "MONITORING"
  );

  display.display();
}


// ============================================================
// OLED ARMED
// ============================================================

void showArmedScreen(
  int seconds
)
{
  display.clearDisplay();

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setTextSize(2);

  display.setCursor(
    25,
    2
  );

  display.println(
    "ARMED"
  );

  display.setTextSize(1);

  display.setCursor(
    25,
    25
  );

  display.println(
    "CANCEL IN"
  );

  display.setTextSize(3);

  display.setCursor(
    55,
    38
  );

  display.println(
    seconds
  );

  display.display();
}


// ============================================================
// OLED DANGER
// ============================================================

void showDangerScreen()
{
  display.clearDisplay();

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setTextSize(2);

  display.setCursor(
    10,
    5
  );

  display.println(
    "DANGER"
  );

  display.setTextSize(1);

  display.setCursor(
    22,
    35
  );

  display.println(
    "EMERGENCY"
  );

  display.setCursor(
    25,
    50
  );

  display.println(
    "ACTIVE"
  );

  display.display();
}


// ============================================================
// BUTTON READING
// ============================================================

void readButtons()
{
  // ==========================================================
  // ALERT BUTTON
  // ==========================================================

  bool alertReading =
    digitalRead(
      ALERT_BUTTON_PIN
    );

  if (
    alertReading !=
    alertLastReading
  )
  {
    alertDebounceTime =
      millis();
  }

  if (
    millis() -
    alertDebounceTime >
    DEBOUNCE_TIME
  )
  {
    if (
      alertReading !=
      alertStableState
    )
    {
      alertStableState =
        alertReading;

      if (
        alertStableState ==
        LOW
      )
      {
        handleAlertPress();
      }
    }
  }

  alertLastReading =
    alertReading;


  // ==========================================================
  // CANCEL BUTTON
  // ==========================================================

  bool cancelReading =
    digitalRead(
      CANCEL_BUTTON_PIN
    );

  if (
    cancelReading !=
    cancelLastReading
  )
  {
    cancelDebounceTime =
      millis();
  }

  if (
    millis() -
    cancelDebounceTime >
    DEBOUNCE_TIME
  )
  {
    if (
      cancelReading !=
      cancelStableState
    )
    {
      cancelStableState =
        cancelReading;

      if (
        cancelStableState ==
        LOW
      )
      {
        handleCancelPress();
      }
    }
  }

  cancelLastReading =
    cancelReading;
}


// ============================================================
// ALERT BUTTON
// ============================================================

void handleAlertPress()
{
  Serial.println(
    "[EVENT] ALERT BUTTON PRESSED"
  );


  if (
    currentState !=
    STATE_SAFE
  )
  {
    Serial.println(
      "[EVENT] Ignored - system not SAFE"
    );

    return;
  }


  unsigned long now =
    millis();


  // ==========================================================
  // FIRST PRESS / EXPIRED WINDOW
  // ==========================================================

  if (
    alertPressCount == 0 ||
    now -
    firstAlertPressTime >
    PRESS_WINDOW
  )
  {
    alertPressCount = 1;

    firstAlertPressTime =
      now;

    Serial.println(
      "[MANUAL] PRESS COUNT = 1"
    );

    return;
  }


  // ==========================================================
  // NEXT PRESS
  // ==========================================================

  alertPressCount++;

  Serial.print(
    "[MANUAL] PRESS COUNT = "
  );

  Serial.println(
    alertPressCount
  );


  // ==========================================================
  // THREE PRESSES
  // ==========================================================

  if (
    alertPressCount >= 3
  )
  {
    alertPressCount = 0;

    Serial.println();
    Serial.println(
      "=============================="
    );

    Serial.println(
      "MANUAL TRIGGER DETECTED"
    );

    Serial.println(
      "=============================="
    );

    triggerManual();
  }
}


// ============================================================
// CANCEL / RESET
// ============================================================

void handleCancelPress()
{
  Serial.println(
    "[EVENT] CANCEL / RESET PRESSED"
  );


  if (
    currentState ==
    STATE_ARMED
  )
  {
    Serial.println(
      "[SYSTEM] ARMED CANCELLED"
    );

    enterSafe();

    return;
  }


  if (
    currentState ==
    STATE_DANGER
  )
  {
    Serial.println(
      "[SYSTEM] EMERGENCY RESET"
    );

    enterSafe();

    return;
  }


  Serial.println(
    "[SYSTEM] Already SAFE"
  );
}


// ============================================================
// MPU INITIALIZATION
// ============================================================

bool initializeMPU()
{
  Serial.println();

  Serial.println(
    "[MPU6050] Initializing..."
  );


  if (
    !mpu.begin()
  )
  {
    Serial.println(
      "[MPU6050] ERROR"
    );

    return false;
  }


  Serial.println(
    "[MPU6050] OK"
  );


  mpu.setAccelerometerRange(
    MPU6050_RANGE_8_G
  );


  mpu.setGyroRange(
    MPU6050_RANGE_500_DEG
  );


  mpu.setFilterBandwidth(
    MPU6050_BAND_21_HZ
  );


  Serial.println(
    "[MPU6050] Configuration complete"
  );


  return true;
}


// ============================================================
// MPU READING
// ============================================================

void readMPU()
{
  sensors_event_t accel;
  sensors_event_t gyro;
  sensors_event_t temp;


  mpu.getEvent(
    &accel,
    &gyro,
    &temp
  );


  // ==========================================================
  // ACCELERATION
  // ==========================================================

  float ax =
    accel.acceleration.x /
    9.80665;

  float ay =
    accel.acceleration.y /
    9.80665;

  float az =
    accel.acceleration.z /
    9.80665;


  // Apply calibration

  accelX =
    ax -
    ACCEL_X_OFFSET;

  accelY =
    ay -
    ACCEL_Y_OFFSET;

  accelZ =
    az -
    ACCEL_Z_OFFSET;


  // ==========================================================
  // GYROSCOPE
  // ==========================================================

  gyroX =
    gyro.gyro.x *
    180.0 /
    PI;

  gyroY =
    gyro.gyro.y *
    180.0 /
    PI;

  gyroZ =
    gyro.gyro.z *
    180.0 /
    PI;


  gyroX -=
    GYRO_X_OFFSET;

  gyroY -=
    GYRO_Y_OFFSET;

  gyroZ -=
    GYRO_Z_OFFSET;


  // ==========================================================
  // MAGNITUDES
  // ==========================================================

  accelerationMagnitude =
    sqrt(
      accelX * accelX +
      accelY * accelY +
      accelZ * accelZ
    );


  gyroMagnitude =
    sqrt(
      gyroX * gyroX +
      gyroY * gyroY +
      gyroZ * gyroZ
    );
}


// ============================================================
// AUTOMATIC MOTION ANALYSIS
// ============================================================

void analyzeAutomaticMotion()
{
  unsigned long now =
    millis();


  // ==========================================================
  // FREE FALL
  // ==========================================================

  if (
    accelerationMagnitude <
    FREE_FALL_THRESHOLD
  )
  {
    if (
      !freeFallDetected
    )
    {
      freeFallDetected =
        true;

      freeFallTime =
        now;

      automaticScore += 2;

      Serial.println(
        ">>> FREE FALL DETECTED <<<"
      );

      Serial.print(
        "Acceleration = "
      );

      Serial.print(
        accelerationMagnitude,
        2
      );

      Serial.println(
        " G"
      );
    }
  }


  // ==========================================================
  // IMPACT
  // ==========================================================

  if (
    freeFallDetected
  )
  {
    if (
      now -
      freeFallTime <=
      IMPACT_WINDOW
    )
    {
      if (
        accelerationMagnitude >=
        IMPACT_THRESHOLD
      )
      {
        automaticScore += 2;

        freeFallDetected =
          false;

        Serial.println(
          ">>> IMPACT DETECTED <<<"
        );

        Serial.print(
          "Acceleration = "
        );

        Serial.print(
          accelerationMagnitude,
          2
        );

        Serial.println(
          " G"
        );
      }
    }
    else
    {
      freeFallDetected =
        false;

      automaticScore =
        0;
    }
  }


  // ==========================================================
  // ROTATION
  // ==========================================================

  if (
    gyroMagnitude >=
    GYRO_THRESHOLD
  )
  {
    automaticScore += 1;

    Serial.println(
      ">>> SIGNIFICANT ROTATION <<<"
    );

    Serial.print(
      "Gyro = "
    );

    Serial.print(
      gyroMagnitude,
      1
    );

    Serial.println(
      " deg/s"
    );
  }


  // ==========================================================
  // LIMIT SCORE
  // ==========================================================

  if (
    automaticScore > 5
  )
  {
    automaticScore = 5;
  }


  // ==========================================================
  // TRIGGER
  // ==========================================================

  if (
    automaticScore >= 3
  )
  {
    if (
      now -
      lastAutomaticTrigger >
      AUTOMATIC_COOLDOWN
    )
    {
      Serial.println();
      Serial.println(
        "========================================"
      );

      Serial.println(
        "AUTOMATIC EMERGENCY DETECTED"
      );

      Serial.println(
        "========================================"
      );

      Serial.print(
        "Score = "
      );

      Serial.println(
        automaticScore
      );


      automaticScore = 0;

      freeFallDetected =
        false;

      lastAutomaticTrigger =
        now;


      triggerAutomatic();
    }
  }
}


// ============================================================
// MANUAL TRIGGER
// ============================================================

void triggerManual()
{
  Serial.println(
    "[TRIGGER] MANUAL"
  );

  enterArmed(
    TRIGGER_MANUAL
  );
}


// ============================================================
// AUTOMATIC TRIGGER
// ============================================================

void triggerAutomatic()
{
  Serial.println(
    "[TRIGGER] AUTOMATIC"
  );

  enterArmed(
    TRIGGER_AUTOMATIC
  );
}


// ============================================================
// ENTER ARMED
// ============================================================

void enterArmed(
  TriggerType trigger
)
{
  currentState =
    STATE_ARMED;

  currentTrigger =
    trigger;

  armedStartTime =
    millis();

  lastCountdown =
    -1;


  // Yellow

  setRGB(
    true,
    true,
    false
  );


  showArmedScreen(
    5
  );


  Serial.println();
  Serial.println(
    "[STATE] SAFE -> ARMED"
  );


  if (
    trigger ==
    TRIGGER_MANUAL
  )
  {
    Serial.println(
      "[STATE] Trigger = MANUAL"
    );
  }
  else
  {
    Serial.println(
      "[STATE] Trigger = AUTOMATIC"
    );
  }


  Serial.println(
    "[STATE] Cancel window = 5 seconds"
  );


  updateFirebaseState();
}


// ============================================================
// UPDATE ARMED
// ============================================================

void updateArmed()
{
  unsigned long elapsed =
    millis() -
    armedStartTime;


  // ==========================================================
  // TIMEOUT
  // ==========================================================

  if (
    elapsed >=
    ARM_DURATION
  )
  {
    Serial.println(
      "[TIMER] ARMED timeout"
    );

    enterDanger();

    return;
  }


  // ==========================================================
  // COUNTDOWN
  // ==========================================================

  unsigned long remaining =
    ARM_DURATION -
    elapsed;


  int seconds =
    (remaining + 999) /
    1000;


  if (
    seconds !=
    lastCountdown
  )
  {
    lastCountdown =
      seconds;

    showArmedScreen(
      seconds
    );

    Serial.print(
      "[ARMED] "
    );

    Serial.print(
      seconds
    );

    Serial.println(
      " seconds"
    );
  }
}


// ============================================================
// ENTER DANGER
// ============================================================

void enterDanger()
{
  currentState =
    STATE_DANGER;


  // ==========================================================
  // RED LED
  // ==========================================================

  setRGB(
    true,
    false,
    false
  );


  // ==========================================================
  // OLED
  // ==========================================================

  showDangerScreen();


  // ==========================================================
  // TRIGGER TEXT
  // ==========================================================

  String triggerText;

  if (
    currentTrigger ==
    TRIGGER_MANUAL
  )
  {
    triggerText =
      "MANUAL";
  }
  else if (
    currentTrigger ==
    TRIGGER_AUTOMATIC
  )
  {
    triggerText =
      "AUTOMATIC";
  }
  else
  {
    triggerText =
      "UNKNOWN";
  }


  // ==========================================================
  // SERIAL
  // ==========================================================

  Serial.println();
  Serial.println(
    "========================================"
  );

  Serial.println(
    "          STATE: DANGER"
  );

  Serial.println(
    "       EMERGENCY CONFIRMED"
  );

  Serial.println(
    "========================================"
  );

  Serial.print(
    "Trigger: "
  );

  Serial.println(
    triggerText
  );


  // ==========================================================
  // FIREBASE
  // ==========================================================

  updateFirebaseState();


  // ==========================================================
  // TELEGRAM
  // ==========================================================

  String telegramMessage =
    "SILENT SOS EMERGENCY\n\n"
    "Device: " DEVICE_ID "\n"
    "State: DANGER\n"
    "Trigger: ";

  telegramMessage +=
    triggerText;

  telegramMessage +=
    "\nTime: ";

  telegramMessage +=
    getTimeString();

  telegramMessage +=
    "\n\nEmergency detected by SILENT SOS device.";


  sendTelegram(
    telegramMessage
  );
}


// ============================================================
// ENTER SAFE
// ============================================================

void enterSafe()
{
  currentState =
    STATE_SAFE;

  currentTrigger =
    TRIGGER_NONE;


  // Reset manual

  alertPressCount =
    0;


  // Reset automatic

  automaticScore =
    0;

  freeFallDetected =
    false;


  // Green

  setRGB(
    false,
    true,
    false
  );


  // OLED

  showSafeScreen();


  Serial.println();
  Serial.println(
    "[STATE] -> SAFE"
  );


  // Firebase

  updateFirebaseState();
}


// ============================================================
// FIREBASE STATE UPDATE
// ============================================================

void updateFirebaseState()
{
  if (
    !firebaseReady
  )
  {
    Serial.println(
      "[FIREBASE] Not ready"
    );

    return;
  }


  String stateText;

  switch (
    currentState
  )
  {
    case STATE_SAFE:
      stateText = "SAFE";
      break;

    case STATE_ARMED:
      stateText = "ARMED";
      break;

    case STATE_DANGER:
      stateText = "DANGER";
      break;
  }


  String triggerText;

  switch (
    currentTrigger
  )
  {
    case TRIGGER_NONE:
      triggerText = "NONE";
      break;

    case TRIGGER_MANUAL:
      triggerText = "MANUAL";
      break;

    case TRIGGER_AUTOMATIC:
      triggerText = "AUTOMATIC";
      break;
  }


  String basePath =
    "/devices/" DEVICE_ID;


  Serial.print(
    "[FIREBASE] State = "
  );

  Serial.println(
    stateText
  );


  Database.set<String>(
    aClient,
    basePath + "/state",
    stateText,
    processData,
    "stateWrite"
  );


  Database.set<String>(
    aClient,
    basePath + "/trigger",
    triggerText,
    processData,
    "triggerWrite"
  );
}


// ============================================================
// FIREBASE STATUS
// ============================================================

void updateFirebaseStatus()
{
  if (
    !firebaseReady
  )
  {
    return;
  }


  String basePath =
    "/devices/" DEVICE_ID;


  Database.set<String>(
    aClient,
    basePath + "/online",
    "true",
    processData,
    "onlineWrite"
  );


  Database.set<String>(
    aClient,
    basePath + "/wifi",
    "connected",
    processData,
    "wifiWrite"
  );


  Database.set<String>(
    aClient,
    basePath + "/lastSeen",
    getTimeString(),
    processData,
    "lastSeenWrite"
  );
}


// ============================================================
// TELEGRAM
// ============================================================

String urlEncode(
  const String &text
)
{
  String encoded = "";

  char hex[] =
    "0123456789ABCDEF";


  for (
    unsigned int i = 0;
    i < text.length();
    i++
  )
  {
    unsigned char c =
      text[i];


    if (
      (
        c >= 'a' &&
        c <= 'z'
      ) ||
      (
        c >= 'A' &&
        c <= 'Z'
      ) ||
      (
        c >= '0' &&
        c <= '9'
      ) ||
      c == '-' ||
      c == '_' ||
      c == '.' ||
      c == '~'
    )
    {
      encoded +=
        (char)c;
    }
    else
    {
      encoded += '%';

      encoded +=
        hex[
          (c >> 4) &
          0x0F
        ];

      encoded +=
        hex[
          c &
          0x0F
        ];
    }
  }


  return encoded;
}


// ============================================================
// SEND TELEGRAM
// ============================================================

void sendTelegram(
  String message
)
{
  if (
    WiFi.status() !=
    WL_CONNECTED
  )
  {
    Serial.println(
      "[TELEGRAM] WiFi NOT connected"
    );

    return;
  }


  if (
    !telegramConfigured
  )
  {
    Serial.println(
      "[TELEGRAM] Bot token not configured"
    );

    return;
  }


  Serial.println();
  Serial.println(
    "[TELEGRAM] Sending..."
  );


  WiFiClientSecure client;

  client.setInsecure();


  HTTPClient https;


  String url =
    "https://api.telegram.org/bot";

  url +=
    BOT_TOKEN;

  url +=
    "/sendMessage?chat_id=";

  url +=
    CHAT_ID;

  url +=
    "&text=";

  url +=
    urlEncode(
      message
    );


  Serial.println(
    "[TELEGRAM] Connecting..."
  );


  if (
    https.begin(
      client,
      url
    )
  )
  {
    https.setTimeout(
      10000
    );


    int httpCode =
      https.GET();


    Serial.print(
      "[TELEGRAM] HTTP Code: "
    );

    Serial.println(
      httpCode
    );


    if (
      httpCode > 0
    )
    {
      String response =
        https.getString();


      Serial.println(
        "[TELEGRAM] Response:"
      );

      Serial.println(
        response
      );


      if (
        httpCode == 200
      )
      {
        Serial.println(
          "[TELEGRAM] ALERT SENT SUCCESSFULLY"
        );
      }
      else
      {
        Serial.println(
          "[TELEGRAM] TELEGRAM REJECTED REQUEST"
        );
      }
    }
    else
    {
      Serial.print(
        "[TELEGRAM] HTTPS ERROR: "
      );

      Serial.println(
        https.errorToString(
          httpCode
        )
      );
    }


    https.end();
  }
  else
  {
    Serial.println(
      "[TELEGRAM] HTTPS BEGIN FAILED"
    );
  }
}


// ============================================================
// FIREBASE CALLBACK
// ============================================================

void processData(
  AsyncResult &aResult
)
{
  if (
    !aResult.isResult()
  )
  {
    return;
  }


  if (
    aResult.isError()
  )
  {
    Serial.print(
      "[FIREBASE ERROR] "
    );

    Serial.print(
      aResult.error().code()
    );

    Serial.print(
      " : "
    );

    Serial.println(
      aResult.error().message()
    );

    return;
  }


  if (
    aResult.available()
  )
  {
    Serial.print(
      "[FIREBASE] "
    );

    Serial.println(
      aResult.c_str()
    );
  }
}