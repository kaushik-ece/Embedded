#include <Arduino.h>
#include <Wire.h>
#include <math.h>

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <Chirale_TensorFlowLite.h>
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include "model_data.h"

// =====================================
// HARDWARE CONFIGURATION
// =====================================

Adafruit_MPU6050 mpu;

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

bool oledReady = false;

#define RED_PIN   25
#define GREEN_PIN 26
#define BLUE_PIN  27

// =====================================
// MODEL CONFIGURATION
// =====================================

constexpr int WINDOW_SIZE = 80;
constexpr int FEATURES = 6;
constexpr int NUM_CLASSES = 5;

constexpr int RAW_SAMPLES = 100;
constexpr int SAMPLE_INTERVAL_US = 20000;

constexpr int kTensorArenaSize = 80 * 1024;

alignas(16) uint8_t tensor_arena[kTensorArenaSize];

const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;

TfLiteTensor* input = nullptr;
TfLiteTensor* output = nullptr;

float sensorBuffer[RAW_SAMPLES][FEATURES];

const float CONFIDENCE_THRESHOLD = 0.70f;

// =====================================
// GESTURE LABELS
// =====================================

const char* gestures[NUM_CLASSES] = {
  "DOWN",
  "IDLE",
  "LEFT",
  "RIGHT",
  "UP"
};

// =====================================
// NORMALIZATION PARAMETERS
// =====================================

const float mean[FEATURES] = {
  -0.6550637,
   0.09276093,
   6.3611383,
  -0.04277088,
   0.01511636,
  -0.0178789
};

const float stdDev[FEATURES] = {
  4.6272044,
  4.4664464,
  2.519388,
  0.06820425,
  0.17487434,
  0.02781677
};

// =====================================
// RGB LED CONTROL
// =====================================

void setRGB(bool red, bool green, bool blue) {

  digitalWrite(RED_PIN, red ? HIGH : LOW);
  digitalWrite(GREEN_PIN, green ? HIGH : LOW);
  digitalWrite(BLUE_PIN, blue ? HIGH : LOW);
}

void updateLED(int gesture) {

  switch (gesture) {

    case 0: // DOWN - Red
      setRGB(true, false, false);
      break;

    case 1: // IDLE - OFF
      setRGB(false, false, false);
      break;

    case 2: // LEFT - Blue
      setRGB(false, false, true);
      break;

    case 3: // RIGHT - Cyan
      setRGB(false, true, true);
      break;

    case 4: // UP - Green
      setRGB(false, true, false);
      break;

    default: // UNCERTAIN - Yellow
      setRGB(true, true, false);
      break;
  }
}

// =====================================
// OLED DISPLAY
// =====================================

void showMessage(const char* message) {

  if (!oledReady) return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("GESTURE AI - ESP32");

  display.drawLine(0, 12, 127, 12, SSD1306_WHITE);

  display.setCursor(0, 24);
  display.println(message);

  display.display();
}

void updateOLED(
  const char* gesture,
  float confidence,
  float inferenceTime
) {

  if (!oledReady) return;

  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("TINYML GESTURE AI");

  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  // Display recognized gesture
  display.setTextSize(2);
  display.setCursor(0, 17);
  display.println(gesture);

  // Display confidence
  display.setTextSize(1);
  display.setCursor(0, 41);

  display.print("Confidence: ");
  display.print(confidence * 100.0f, 1);
  display.println("%");

  // Display inference latency
  display.setCursor(0, 54);

  display.print("Time: ");
  display.print(inferenceTime, 1);
  display.println(" ms");

  display.display();
}

// =====================================
// ERROR HANDLER
// =====================================

void stopProgram() {

  setRGB(false, false, false);

  showMessage("SYSTEM ERROR");

  while (true) {
    delay(1000);
  }
}

// =====================================
// INITIALIZE HARDWARE
// =====================================

void initializeHardware() {

  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);

  setRGB(false, false, false);

  Wire.begin(21, 22);

  // Check OLED address
  Wire.beginTransmission(OLED_ADDRESS);

  if (Wire.endTransmission() == 0) {

    oledReady = display.begin(
      SSD1306_SWITCHCAPVCC,
      OLED_ADDRESS,
      true,
      false
    );
  }

  if (oledReady) {
    Serial.println("OLED initialized successfully");
    showMessage("Initializing...");
  } else {
    Serial.println("WARNING: OLED not detected");
  }

  // Initialize MPU6050
  if (!mpu.begin()) {

    Serial.println("ERROR: MPU6050 not detected");

    stopProgram();
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  Serial.println("MPU6050 initialized successfully");
  Serial.println("RGB LED initialized");
}

// =====================================
// INITIALIZE TINYML MODEL
// =====================================

void initializeModel() {

  model = tflite::GetModel(gesture_model);

  if (model->version() != TFLITE_SCHEMA_VERSION) {

    Serial.println("ERROR: Model schema mismatch");

    stopProgram();
  }

  static tflite::MicroMutableOpResolver<7> resolver;

  if (
    resolver.AddExpandDims() != kTfLiteOk ||
    resolver.AddConv2D() != kTfLiteOk ||
    resolver.AddReshape() != kTfLiteOk ||
    resolver.AddMaxPool2D() != kTfLiteOk ||
    resolver.AddMean() != kTfLiteOk ||
    resolver.AddFullyConnected() != kTfLiteOk ||
    resolver.AddSoftmax() != kTfLiteOk
  ) {

    Serial.println("ERROR: Operator registration failed");

    stopProgram();
  }

  static tflite::MicroInterpreter static_interpreter(
    model,
    resolver,
    tensor_arena,
    kTensorArenaSize
  );

  interpreter = &static_interpreter;

  if (interpreter->AllocateTensors() != kTfLiteOk) {

    Serial.println("ERROR: Tensor allocation failed");

    stopProgram();
  }

  input = interpreter->input(0);
  output = interpreter->output(0);

  if (
    input->type != kTfLiteFloat32 ||
    output->type != kTfLiteFloat32 ||
    input->bytes != 480 * sizeof(float) ||
    output->bytes != 5 * sizeof(float)
  ) {

    Serial.println("ERROR: Unexpected tensor format");

    stopProgram();
  }

  Serial.println("TinyML model initialized successfully");
}

// =====================================
// COLLECT SENSOR DATA
// =====================================

void collectSensorData() {

  unsigned long nextSample = micros();

  for (int i = 0; i < RAW_SAMPLES; i++) {

    while ((int32_t)(micros() - nextSample) < 0) {
      delayMicroseconds(100);
    }

    sensors_event_t a, g, temp;

    mpu.getEvent(&a, &g, &temp);

    sensorBuffer[i][0] = a.acceleration.x;
    sensorBuffer[i][1] = a.acceleration.y;
    sensorBuffer[i][2] = a.acceleration.z;

    sensorBuffer[i][3] = g.gyro.x;
    sensorBuffer[i][4] = g.gyro.y;
    sensorBuffer[i][5] = g.gyro.z;

    nextSample += SAMPLE_INTERVAL_US;
  }
}

// =====================================
// RESAMPLE AND NORMALIZE
// =====================================

void prepareModelInput() {

  for (int i = 0; i < WINDOW_SIZE; i++) {

    float position =
      (float)i * (RAW_SAMPLES - 1)
      / (WINDOW_SIZE - 1);

    int lower = (int)position;

    int upper = lower + 1;

    if (upper >= RAW_SAMPLES) {
      upper = RAW_SAMPLES - 1;
    }

    float fraction = position - lower;

    for (int j = 0; j < FEATURES; j++) {

      float value =
        sensorBuffer[lower][j] *
        (1.0f - fraction) +
        sensorBuffer[upper][j] *
        fraction;

      float normalized =
        (value - mean[j]) / stdDev[j];

      input->data.f[i * FEATURES + j] =
        normalized;
    }
  }
}

// =====================================
// GESTURE RECOGNITION
// =====================================

void recognizeGesture() {

  unsigned long startTime = micros();

  if (interpreter->Invoke() != kTfLiteOk) {

    Serial.println("ERROR: Inference failed");

    setRGB(false, false, false);
    showMessage("Inference error");

    return;
  }

  float inferenceTime =
    (micros() - startTime) / 1000.0f;

  int predictedClass = 0;

  for (int i = 0; i < NUM_CLASSES; i++) {

    if (!isfinite(output->data.f[i])) {

      Serial.println("ERROR: Invalid prediction");

      setRGB(false, false, false);
      showMessage("Invalid prediction");

      return;
    }

    if (output->data.f[i] >
        output->data.f[predictedClass]) {

      predictedClass = i;
    }
  }

  float confidence =
    output->data.f[predictedClass];

  const char* detectedGesture;

  if (confidence >= CONFIDENCE_THRESHOLD) {

    detectedGesture = gestures[predictedClass];

    updateLED(predictedClass);

  } else {

    detectedGesture = "UNCERTAIN";

    updateLED(-1);
  }

  // Update OLED
  updateOLED(
    detectedGesture,
    confidence,
    inferenceTime
  );

  // Serial Monitor results
  Serial.println();
  Serial.println("================================");

  for (int i = 0; i < NUM_CLASSES; i++) {

    Serial.print(gestures[i]);
    Serial.print(": ");

    Serial.print(
      output->data.f[i] * 100.0f, 2
    );

    Serial.println("%");
  }

  Serial.println("--------------------------------");

  Serial.print("Detected gesture: ");
  Serial.println(detectedGesture);

  Serial.print("Confidence: ");
  Serial.print(confidence * 100.0f, 2);
  Serial.println("%");

  Serial.print("Inference time: ");
  Serial.print(inferenceTime, 2);
  Serial.println(" ms");

  Serial.println("================================");
}

// =====================================
// SETUP
// =====================================

void setup() {

  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("================================");
  Serial.println("FINAL TINYML GESTURE AI SYSTEM");
  Serial.println("================================");

  initializeHardware();
  initializeModel();

  showMessage("System ready!");

  Serial.println("System ready!");
  Serial.println("Perform a gesture");
}

// =====================================
// MAIN LOOP
// =====================================

void loop() {

  Serial.println();
  Serial.println("Collecting gesture data...");

  collectSensorData();

  prepareModelInput();

  recognizeGesture();
}