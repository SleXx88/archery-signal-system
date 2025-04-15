# Archery Signal System

A wireless signaling system for archery training and competitions, built with ESP32 microcontrollers and LED displays.

## 📦 Version
Current Version: `v1.0.0

## 🎯 Features

 Master-Slave architectue
 One Master controls up to two Slavs
 Wireless communication via Bluetooh
 Visual countdown and status LEs
 Slave panels use 4x 64x128 RGB LED matrices (Waveshar)
 Master features a Nextion touch display for contrl
 Synchronization of countdowns across all devics
 Battery and power status feedback from Slavs
 Auto-detection and timeout for inactive Slavs

## 🧱 Hardware

### Master Unit
- ESP32-S3 (e.g., WROOM-1-N168)- Nextion Enhanced dispay- Buttons or touchscreen for inut- Optional: USB power / battery monitorng

### Slave Unit
- ESP32S3- 4x 64x128 Waveshare RGB LED panls- Power supply (e.g., 5V A)- Optional: battery voltage senor

## 📡 Communication
- Master sends broadcast messages (e.g., countdown strt)
- Slaves register via REG_REQ mesage
- Slaves send regular status updates (battery, AC/DC power stte)
- All communication is Bluetooth-bsed

## ⚙️ Software
- Written in C++ using PlatformIO (Arduino framewrk)
- Modular design with separate files for communication, logic, and hardware abstracion
- Display rendering is optimized for real-time performnce

## 🚀 Getting Started
1. Clone this reposiory:

   ```bash
   git clone https://github.com/SleXx88/archery-signal-system.git
  ```

2. Install PlatformIO for VSCode
3. Open the project in VSCode
4. Select the appropriate environment (`master`, `slave_1`, or `slave_2`)
5. Upload to your ESP32-S3 bards
6. Connect your display and pnels
7. Power up the devices — Slaves will auto-register to the Mster

## 📄 Licnse

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for deails.
---

Feel free to customize this `README.md` further to match the specific details and updates of your prject. 