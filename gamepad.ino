#include <Arduino.h>
#include <Wire.h>
#include <MPU6050_light.h>
#include <BleGamepad.h>

BleGamepad bleGamepad("ESP32 Pro Controller", "DIY", 100);
BleGamepadConfiguration bleGamepadConfig;

MPU6050 mpu(Wire);

// ==========================================
// 1. ANALOG PINS (Joysticks)
// ==========================================
#define LEFT_X        32
#define LEFT_Y        33
#define RIGHT_X       34
#define RIGHT_Y       35

// ==========================================
// 2. INVERSION SETTINGS
// ==========================================
#define INVERT_LEFT_X    false
#define INVERT_LEFT_Y    false
#define INVERT_RIGHT_X   false
#define INVERT_RIGHT_Y   false

// ==========================================
// 3. BUTTON PINS
// ==========================================
#define BUTTON_A      15    // D15 (A)
#define BUTTON_B      18    // D18 (B)
#define BUTTON_X      5     // D5  (X)
#define BUTTON_Y      23    // D23 (Y)

#define BUTTON_L1     19    // D19 (Left Bumper / LB)
#define BUTTON_R1     25    // D25 (Right Bumper / RB)

#define BUTTON_L2     27    // D27 (Left Trigger)
#define BUTTON_R2     26    // D26 (Right Trigger)

// JOYSTICK CLICK SWITCHES (SW pins)
#define BUTTON_L3     14    // D14 (SW on Left Stick -> L3 Sprint)
#define BUTTON_R3     13    // D13 (SW on Right Stick -> R3 Melee)

#define BUTTON_GYRO   4     // D4 (Dedicated Gyro Mode Button)

// ==========================================
// 4. VIBRATION MOTOR PIN
// ==========================================
#define VIBRATION_PIN 16    // Pin D16 (RX2) -> Transistor Base

// Hall Effect Analog Triggers (Set to true if using SS49E)
#define USE_HALL_TRIGGERS false
#define TRIGGER_LEFT      36    // VP
#define TRIGGER_RIGHT     39    // VN

// Modes: 0 = Standard (Gyro OFF), 1 = Driving, 2 = Shooter
int currentMode = 0; 

// ==========================================
// 5. HIGH-SPEED GYRO TUNING
// ==========================================
// Reduced deadzone from 3.0° to 1.0°: instantaneous micro-tilt response without lag
#define GYRO_DEADZONE   1.0f   
// Reduced max tilt from 30.0° to 18.0°: reach full steering/aiming speed with less wrist angle
#define GYRO_MAX_TILT  18.0f   

int leftXCenter = 2048, leftYCenter = 2048;
int rightXCenter = 2048, rightYCenter = 2048;
int trigLRest = 1800, trigRRest = 1800;

unsigned long rumbleUntil = 0;

void startVibration(uint16_t durationMs) {
  digitalWrite(VIBRATION_PIN, HIGH);
  rumbleUntil = millis() + durationMs;
}

// Fast 2-sample average with short 15µs settle time (cuts ~1ms blocking time per loop)
int readAnalogStable(uint8_t pin) {
  int v1 = analogRead(pin);
  delayMicroseconds(15);
  int v2 = analogRead(pin);
  return (v1 + v2) >> 1;
}

uint16_t convertAxis(int raw, int center, bool invert) {
  int diff = raw - center;
  if (abs(diff) <= 200) return 16384;
  long output = 16384;
  if (diff > 0) output = constrain(map(diff, 200, 1300, 16384, 32767), 16384, 32767);
  else output = constrain(map(diff, -1300, -200, 0, 16384), 0, 16384);

  if (invert) {
    output = 32767 - output;
  }
  return (uint16_t)output;
}

// Smooth linear scaling from deadzone to max tilt (eliminates the abrupt 10% jump of map())
int16_t getGyroOffset(float angle) {
  float absAngle = abs(angle);
  if (absAngle <= GYRO_DEADZONE) return 0;

  float effectiveAngle = absAngle - GYRO_DEADZONE;
  float range = GYRO_MAX_TILT - GYRO_DEADZONE;
  if (effectiveAngle > range) effectiveAngle = range;

  float normalized = effectiveAngle / range; // 0.0 to 1.0
  int16_t offset = (int16_t)(normalized * 16384.0f);

  return (angle < 0) ? -offset : offset;
}

void setup() {
  Serial.begin(115200);

  // Vibration Pin
  pinMode(VIBRATION_PIN, OUTPUT);
  digitalWrite(VIBRATION_PIN, LOW);

  // Setup Buttons with internal pullups
  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(BUTTON_X, INPUT_PULLUP);
  pinMode(BUTTON_Y, INPUT_PULLUP);
  pinMode(BUTTON_L1, INPUT_PULLUP);
  pinMode(BUTTON_R1, INPUT_PULLUP);
  pinMode(BUTTON_L2, INPUT_PULLUP);
  pinMode(BUTTON_R2, INPUT_PULLUP);
  pinMode(BUTTON_L3, INPUT_PULLUP); // Left Stick Click (D14)
  pinMode(BUTTON_R3, INPUT_PULLUP); // Right Stick Click (D13)
  pinMode(BUTTON_GYRO, INPUT_PULLUP);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  delay(500);
  long lx = 0, ly = 0, rx = 0, ry = 0;
  for (int i = 0; i < 50; i++) {
    lx += readAnalogStable(LEFT_X);
    ly += readAnalogStable(LEFT_Y);
    rx += readAnalogStable(RIGHT_X);
    ry += readAnalogStable(RIGHT_Y);
    delay(3);
  }
  leftXCenter  = lx / 50;
  leftYCenter  = ly / 50;
  rightXCenter = rx / 50;
  rightYCenter = ry / 50;

  if (USE_HALL_TRIGGERS) {
    trigLRest = readAnalogStable(TRIGGER_LEFT);
    trigRRest = readAnalogStable(TRIGGER_RIGHT);
  }

  // 1. Initialize I2C and boost bus clock to 400 kHz (Fast Mode)
  Wire.begin(21, 22);
  Wire.setClock(400000); // 400kHz I2C: 4x faster data transfer from MPU-6050

  byte status = mpu.begin();
  if (status == 0) {
    delay(1000);
    mpu.calcOffsets();
    Serial.println("✅ High-Speed Gyro Ready (400kHz I2C)!");
  }

  bleGamepadConfig.setAutoReport(false);
  bleGamepadConfig.setControllerType(CONTROLLER_TYPE_GAMEPAD);
  bleGamepadConfig.setEnableOutputReport(true);
  bleGamepadConfig.setOutputReportLength(64);
  bleGamepadConfig.setButtonCount(16);
  bleGamepadConfig.setWhichAxes(
    true,   // X  (Left Stick Horizontal -> Axis 0)
    true,   // Y  (Left Stick Vertical   -> Axis 1)
    false,  // Z  (Disabled)
    true,   // RX (Right Stick Horizontal / Camera Left-Right -> Axis 2)
    true,   // RY (Right Stick Vertical   / Camera Up-Down    -> Axis 3)
    true,   // RZ (Backup Vertical Axis)
    true,   // Slider 1 (Left Trigger)
    true    // Slider 2 (Right Trigger)
  );
  bleGamepadConfig.setAxesMin(0);
  bleGamepadConfig.setAxesMax(32767);
  bleGamepad.begin(&bleGamepadConfig);

  Serial.println("🎮 Controller Ready! Fast gyro polling active.");
}

void loop() {
  // Update MPU-6050 angles
  mpu.update();

  if (rumbleUntil > 0 && millis() >= rumbleUntil) {
    digitalWrite(VIBRATION_PIN, LOW);
    rumbleUntil = 0;
  }

  if (!bleGamepad.isConnected()) {
    delay(10);
    return;
  }

  // Force Feedback / Rumble from PC
  if (bleGamepad.isOutputReceived()) {
    uint8_t* buffer = bleGamepad.getOutputBuffer();
    if (buffer[0] > 0 || buffer[1] > 0) {
      startVibration(150);
    }
  }

  // Gyro Mode Button (D4)
  static bool lastGyroBtnState = HIGH;
  static unsigned long lastModeSwitchTime = 0;
  bool currentGyroBtnState = digitalRead(BUTTON_GYRO);

  if (lastGyroBtnState == HIGH && currentGyroBtnState == LOW) {
    if (millis() - lastModeSwitchTime > 250) {
      currentMode = (currentMode + 1) % 3;
      if (currentMode == 1) {
        Serial.println("🚗 MODE 1: DRIVING (Tilt Steers Left Stick)");
        startVibration(100);
      } else if (currentMode == 2) {
        Serial.println("🎯 MODE 2: SHOOTER (Tilt Aims Right Stick Camera)");
        startVibration(220);
      } else {
        Serial.println("⏹️ MODE 0: GYRO OFF (Standard Controller)");
        startVibration(350);
      }
      lastModeSwitchTime = millis();
    }
  }
  lastGyroBtnState = currentGyroBtnState;

  // 1. Read Physical Sticks
  uint16_t stickLX = convertAxis(readAnalogStable(LEFT_X), leftXCenter, INVERT_LEFT_X);
  uint16_t stickLY = convertAxis(readAnalogStable(LEFT_Y), leftYCenter, INVERT_LEFT_Y);

  uint16_t stickRX = convertAxis(readAnalogStable(RIGHT_X), rightXCenter, INVERT_RIGHT_X);
  uint16_t stickRY = convertAxis(readAnalogStable(RIGHT_Y), rightYCenter, INVERT_RIGHT_Y);

  // 2. Read Gyro Angles
  float roll  = mpu.getAngleX();
  float pitch = mpu.getAngleY();

  uint16_t outLX = stickLX;
  uint16_t outLY = stickLY;
  uint16_t outRX = stickRX;
  uint16_t outRY = stickRY;

  // 3. Apply Gyro Mode
  if (currentMode == 1) {
    if (stickLX == 16384) {
      int16_t steerOffset = getGyroOffset(roll);
      outLX = constrain(16384 + steerOffset, 0, 32767);
    }
  } 
  else if (currentMode == 2) {
    int16_t aimX = getGyroOffset(roll);
    int16_t aimY = getGyroOffset(-pitch);
    outRX = constrain(stickRX + aimX, 0, 32767);
    outRY = constrain(stickRY + aimY, 0, 32767);
  }

  // 4. Send Sticks to Gamepad
  bleGamepad.setX(outLX);     // Left Stick Horizontal
  bleGamepad.setY(outLY);     // Left Stick Vertical

  bleGamepad.setRX(outRX);    // Right Stick Horizontal (Camera Left-Right)
  bleGamepad.setRY(outRY);    // Right Stick Vertical (Camera Up-Down)
  bleGamepad.setRZ(outRY);

  // 5. Send Triggers
  if (USE_HALL_TRIGGERS) {
    int diffL = abs(readAnalogStable(TRIGGER_LEFT) - trigLRest);
    int diffR = abs(readAnalogStable(TRIGGER_RIGHT) - trigRRest);
    uint16_t LT = (diffL > 80) ? constrain(map(diffL, 80, 900, 0, 32767), 0, 32767) : 0;
    uint16_t RT = (diffR > 80) ? constrain(map(diffR, 80, 900, 0, 32767), 0, 32767) : 0;
    bleGamepad.setSlider1(LT);
    bleGamepad.setSlider2(RT);
    if (LT > 3000) bleGamepad.press(BUTTON_7); else bleGamepad.release(BUTTON_7);
    if (RT > 3000) bleGamepad.press(BUTTON_8); else bleGamepad.release(BUTTON_8);
  } else {
    if (digitalRead(BUTTON_L2) == LOW) {
      bleGamepad.press(BUTTON_7);
      bleGamepad.setSlider1(32767);
    } else {
      bleGamepad.release(BUTTON_7);
      bleGamepad.setSlider1(0);
    }

    if (digitalRead(BUTTON_R2) == LOW) {
      bleGamepad.press(BUTTON_8);
      bleGamepad.setSlider2(32767);
    } else {
      bleGamepad.release(BUTTON_8);
      bleGamepad.setSlider2(0);
    }
  }

  // 6. Send Buttons
  if (digitalRead(BUTTON_A) == LOW) bleGamepad.press(BUTTON_1); else bleGamepad.release(BUTTON_1);
  if (digitalRead(BUTTON_B) == LOW) bleGamepad.press(BUTTON_2); else bleGamepad.release(BUTTON_2);
  if (digitalRead(BUTTON_X) == LOW) bleGamepad.press(BUTTON_3); else bleGamepad.release(BUTTON_3);
  if (digitalRead(BUTTON_Y) == LOW) bleGamepad.press(BUTTON_4); else bleGamepad.release(BUTTON_4);
  if (digitalRead(BUTTON_L1) == LOW) bleGamepad.press(BUTTON_5); else bleGamepad.release(BUTTON_5);
  if (digitalRead(BUTTON_R1) == LOW) bleGamepad.press(BUTTON_6); else bleGamepad.release(BUTTON_6);

  // 7. L3 & R3 Joystick Clicks (Mapped to Standard BUTTON_11 and BUTTON_12)
  bool l3State = (digitalRead(BUTTON_L3) == LOW);
  bool r3State = (digitalRead(BUTTON_R3) == LOW);

  if (l3State) bleGamepad.press(BUTTON_11); else bleGamepad.release(BUTTON_11);
  if (r3State) bleGamepad.press(BUTTON_12); else bleGamepad.release(BUTTON_12);

  // Diagnostic print in Serial Monitor when you click either stick
  static bool lastL3State = false;
  static bool lastR3State = false;
  if (l3State != lastL3State) {
    if (l3State) Serial.println("🔘 LEFT STICK (L3) CLICKED!");
    else Serial.println("⚪ Left Stick (L3) Released");
    lastL3State = l3State;
  }
  if (r3State != lastR3State) {
    if (r3State) Serial.println("🔘 RIGHT STICK (R3) CLICKED!");
    else Serial.println("⚪ Right Stick (R3) Released");
    lastR3State = r3State;
  }

  bleGamepad.sendReport();

  // Fast polling delay (2ms gives ~200Hz refresh rate instead of ~70Hz)
  delay(2);
}