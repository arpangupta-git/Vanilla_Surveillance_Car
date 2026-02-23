# 🚗 ESP32-CAM Stealth Surveillance Car

A high-performance RC Surveillance Car built using the ESP32-CAM module and an L298N motor driver. It creates its own Wi-Fi network and hosts a sleek, cyberpunk-styled Web HUD with a virtual joystick, live video feed, and tactical features.

![UI Preview](https://img.shields.io/badge/UI-Cyberpunk_HUD-00f7ff.svg)
![Platform](https://img.shields.io/badge/Platform-ESP32--CAM-blue)
![Camera](https://img.shields.io/badge/Camera-OV2640%20%2F%20OV3660-green)

## ✨ Features

* **Optimized Video Feed:** Custom WebSocket logic with aggressive frame-dropping manages network buffering to maintain a real-time feed during motor interference.
* **AP Mode (No Router Needed):** The car hosts its own Wi-Fi network (`Vanilla`), meaning you can drive it anywhere—in a park, an abandoned building, or your backyard.
* **Tactical Night Vision:** Hardware-level grayscale and contrast boost for better visibility in low light, without bogging down the CPU.
* **Sneak Mode (Dual Rates):** Instantly cuts speed in half for precision parking and navigating tight corners.
* **Runaway Watchdog:** A 500ms safety timer automatically hits the brakes if your phone loses connection or the screen turns off.
* **Modern Mobile HUD:** Features a virtual touch-joystick, flashlight intensity slider, speed controls, and a 1-click Photo Capture button.

## 🛠️ Hardware Required

* 1x ESP32-CAM Module (AI-Thinker) + FTDI Programmer
* 1x L298N Motor Driver
* 1x 4WD Robot Car Chassis (4 DC Motors)
* 1x 7V to 12V Battery Pack (e.g., 2x 18650 Li-ion batteries)
* Jumper wires

## 🔌 Wiring & Pinout

Below is the exact wiring configuration mapped to the code. 

| ESP32-CAM | L298N Motor Driver / Hardware |
| :--- | :--- |
| **GPIO 13** | **ENA & ENB** (Shared Speed Control) |
| **GPIO 12** | **IN1** (Right Motor Forward) |
| **GPIO 15** | **IN2** (Right Motor Reverse) |
| **GPIO 14** | **IN3** (Left Motor Forward) |
| **GPIO 2** | **IN4** (Left Motor Reverse) |
| **GPIO 4** | **Flashlight LED** |
| **GND** | **GND** (Common Ground) |

## 💻 Software Installation

1. Install the **ESP32 Board Manager** in the Arduino IDE.
2. Install the following libraries:
   * `ESPAsyncWebServer` by me-no-dev
   * `AsyncTCP` by me-no-dev
3. Select **"AI Thinker ESP32-CAM"** in the Tools menu.
4. Enable **PSRAM** in the Tools menu (Required for high-resolution video buffering).
5. Compile and upload the sketch!

## 🚀 How to Use

1. Turn on the car.
2. Open your phone/computer Wi-Fi settings and connect to **`Vanilla`** (Password: `1223334444`).
3. Open your web browser and go to: **`http://192.168.4.1`**
4. The HUD will load, the video will start, and you are ready to drive!

### ⚠️ Troubleshooting: "Site Can't Be Reached"

If you connect to the Wi-Fi but `192.168.4.1` won't load on your phone:
* **Turn off Mobile Data:** Modern smartphones realize the car's Wi-Fi doesn't have internet, so they secretly route browser traffic through your 4G/5G data. Turn off Cellular Data temporarily.
* **Accept the Network:** Watch for a pop-up on your phone saying *"This network has no internet access. Stay connected?"* You **must** tap "Yes" or "Keep Wi-Fi".
* **Only 4 Users:** The ESP32 can only handle ~4 connected devices at once. Ensure other phones aren't connected to the car.
