# Archery Signal System

A wireless signaling system for archery training and competitions, built with ESP32 microcontrollers and LED displays.

## Version

Current Version: v1.0.0

## Features

- Master-Slave architecture
- One Master controls up to two Slaves
- Wireless communication via Bluetooth
- Visual countdown and status LEDs
- Slave panels use 4x 64x128 RGB LED matrices (Waveshare)
- Master features a Nextion touch display for control
- Synchronization of countdowns across all devices
- Battery and power status feedback from Slaves
- Auto-detection and timeout for inactive Slaves

## Hardware

### Master Unit

- ESP32-S3 (e.g., WROOM-1-N16R8)
- Nextion Enhanced display
- Buttons or touchscreen for input
- Optional: USB power / battery monitoring

### Slave Unit

- ESP32-S3
- 4x 64x128 Waveshare RGB LED panels
- Power supply (e.g., 5V 6A)
- Optional: battery voltage sensor

## Communication

- Master sends broadcast messages (e.g., countdown start)
- Slaves register via REG_REQ message
- Slaves send regular status updates (battery, AC/DC power state)
- All communication is Bluetooth-based

## Software

- Written in C++ using PlatformIO (Arduino framework)
- Modular design with separate files for communication, logic, and hardware abstraction
- Display rendering is optimized for real-time performance

## Getting Started

1. Clone this repository:

   ```bash
   git clone https://github.com/SleXx88/archery-signal-system.git
