# 🎮 ESP32 Custom Gaming Controller

A DIY Bluetooth gaming controller powered by ESP32, designed for wireless gaming with analog joystick input, motion sensing, and customizable controls.

## 🚀 Features

- Dual analog joysticks with clickable thumbsticks (L3 / R3)
- Four action buttons (A, B, X, Y)
- Shoulder bumpers and triggers (L1, R1, L2, R2)
- MPU6050 6-axis motion sensing (400kHz Fast I2C)
- Bluetooth Low Energy (BLE) wireless connectivity
- Haptic vibration feedback (mode cues and in-game rumble)
- Steering control mode (motion wheel for racing)
- Camera control mode (FPS motion aiming)
- Auto-centering calibration on startup

## 🔧 Hardware Components

- ESP32 DevKit V1 (30-pin)
- 2 × Analog Joysticks (with integrated push-click switches)
- MPU6050 Gyroscope and Accelerometer Module
- Tactile Push Buttons
- Vibration Motor (with NPN transistor driver)
- Connecting Wires & Breadboard / Custom PCB

## 📌 Pin Connections

| Component | ESP32 GPIO |
|---|---|
| Left Joystick X | GPIO 32 |
| Left Joystick Y | GPIO 33 |
| Left Joystick Click (L3) | GPIO 14 |
| Right Joystick X | GPIO 34 |
| Right Joystick Y | GPIO 35 |
| Right Joystick Click (R3) | GPIO 13 |
| A Button | GPIO 15 |
| B Button | GPIO 18 |
| X Button | GPIO 5 |
| Y Button | GPIO 23 |
| L1 Bumper | GPIO 19 |
| R1 Bumper | GPIO 25 |
| L2 Trigger | GPIO 27 |
| R2 Trigger | GPIO 26 |
| Gyro Mode Switch | GPIO 4 |
| Vibration Motor | GPIO 16 |
| MPU6050 SDA | GPIO 21 |
| MPU6050 SCL | GPIO 22 |

*All push buttons are wired with internal pull-ups (active LOW to GND).*

## 💻 Software & Technologies

- Arduino IDE
- Embedded C++
- ESP32 BLE Gamepad Library (`BleGamepad`)
- MPU6050 Light Library (`MPU6050_light`)
- Wire (I2C Protocol)

## ⚙️ Working Principle

The ESP32 reads analog joystick coordinates and digital button inputs. Simultaneously, the MPU6050 provides real-time angular velocity and tilt data over a high-speed 400kHz I2C bus. The microcontroller processes these inputs with drift-filtered complementary algorithms, then packages and transmits standard HID gamepad reports wirelessly to a host device (PC, Android, iOS) via Bluetooth Low Energy (BLE) at ~200Hz.

## 🎯 Operating Modes

Press the dedicated **Mode Button (GPIO 4)** to cycle between modes with haptic vibration confirmation:

- **Mode 0 – Standard Mode:** Gyro disabled; operates as a traditional twin-stick controller.
- **Mode 1 – Steering Mode:** Controller roll tilt controls vehicle steering on the Left Stick.
- **Mode 2 – Camera Control Mode:** Fine controller tilt translates into smooth, micro-precision camera aiming on the Right Stick.

## 👨‍💻 Developer

**Sanchit Khobragade**  
Electronics and Telecommunication Engineering


