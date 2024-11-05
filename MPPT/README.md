---

# MPPT140D Solar Charge Controller Recovery with Arduino

This project restores the functionality of the MPPT140D solar charge controller after its original MCU failed. By replacing the MCU with an Arduino, this project re-implements the core functionalities of the charge controller, including maximum power point tracking (MPPT), battery management, and real-time monitoring. The goal is to extend the life of the MPPT140D controller, providing a customizable and efficient solar charging solution.

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Module Descriptions](#module-descriptions)
- [Hardware Requirements](#hardware-requirements)
- [Software Setup](#software-setup)
- [Installation](#installation)
- [Usage](#usage)
- [Configuration](#configuration)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)
- [License](#license)

## Overview

This project is designed for users who wish to recover their MPPT140D solar charge controller by replacing its MCU with an Arduino. The software recreates the essential functions of the charge controller, allowing for efficient solar power management and battery protection.

With this Arduino solution, the MPPT140D controller can operate safely and efficiently, maximizing power extraction from solar panels and ensuring the battery remains in optimal condition.

## Features

- **MPPT Algorithm**: Adjusts the duty cycle to find the maximum power point of the solar panel, ensuring optimal energy transfer.
- **Battery Management**: Implements different charging states (off, bulk, float, and scan) to manage battery voltage and charge levels.
- **Protection Mechanisms**: Monitors parameters like overcurrent, overvoltage, and temperature to prevent damage to the system.
- **LCD Display**: Provides a real-time display of battery status, solar panel information, and system metrics.
- **Energy Accumulation**: Tracks and stores accumulated energy data (e.g., watt-hours and ampere-hours).

## Module Descriptions

### **1. Abstract.h**
Defines the `IState` abstract class and shared constants used by the state machine. This file provides the foundation for state-based control, with each specific state (e.g., `off`, `float`, `bulk`, `scan`) implementing its own behavior through the `Handle` method.

### **2. Charging.ino**
Contains the main charging logic, managing transitions between charging states based on battery voltage, solar power input, and other parameters. This module includes the MPPT algorithm and adjusts the PWM duty cycle for efficient battery charging.

### **3. DeviceProtection.ino**
Implements safety mechanisms to protect the system from potentially damaging conditions. This includes monitoring for overcurrent, overvoltage, and overheating, as well as battery connection detection.

### **4. LCD.ino**
Manages the LCD display, showing real-time data about the solar charge controller s operation. Displays current state, voltages, currents, temperature, and accumulated energy metrics. Also provides basic user interaction through buttons for cycling display data or settings.

### **5. MPPT.ino**
Implements the MPPT algorithm that optimizes power extraction from the solar panel by adjusting the duty cycle based on varying sunlight conditions. This module enhances charging efficiency by dynamically finding the maximum power point.

## Hardware Requirements

- **Arduino Board** (e.g., Arduino Uno, Nano)
- **MPPT140D Solar Charge Controller** (with failed MCU)
- **Voltage and Current Sensors** (e.g., ADS1015 for accurate measurements)
- **Shift 595 register**

## Software Setup

1. **Arduino IDE**: [Download](https://www.arduino.cc/en/software) and install the Arduino IDE.
2. **Required Libraries**:
   - **ADS1X15** (for voltage and current sensing)
   - **TimerOne** (for PWM control, if applicable)
   - **Wire** (for I2C communication with sensors)

3. **Clone the Repository**:
   ```bash
   git clone https://github.com/ic472pbt/arduino.git
   ```

## Installation

1. **Open Project**: Open the main Arduino file (e.g., `MPPT.ino`) in the Arduino IDE.
2. **Select Board and Port**: Choose the correct Arduino board and COM port in the Arduino IDE.
3. **Upload the Code**: Upload the code to your Arduino board.

## Usage

Once uploaded, the Arduino will begin controlling the MPPT140D solar charge controller. You can monitor the system via the Serial Monitor or the connected LCD screen.

### Serial Monitor Output

The Serial Monitor provides real-time data, including the current charging state, voltages, currents, temperatures, and energy metrics.

```plaintext
State: Bulk
Battery Voltage: 12.6V
Solar Voltage: 36.5V
Current: 2.3A
Duty Cycle: 55%
```

### LCD Display Output

The LCD displays similar information to the Serial Monitor, including the current state, voltage, current, temperature, and accumulated energy. You can cycle through different data views using the control buttons.

## Configuration

Several parameters can be adjusted in the code to customize the controller for your specific setup:
- **Voltage Thresholds**: Adjust float and bulk voltage settings in the `config.h` file or within `Charging.ino`.
- **Duty Cycle Limits**: Set minimum and maximum duty cycles to control PWM output.
- **Temperature Compensation**: If temperature sensors are connected, adjust compensation values for more accurate charging control.

## Troubleshooting

If you encounter issues, here are some common troubleshooting steps:

1. **No Display on LCD**: Check wiring and make sure the correct pins are defined in the code.
2. **Inconsistent Voltage/Current Readings**: Verify sensor connections and calibration settings.
3. **System Overheating**: Ensure proper ventilation and heat dissipation, particularly for the MOSFETs and other power components.

## Contributing

Contributions are welcome! To contribute:

1. Fork this repository.
2. Create a new branch for your feature or bug fix.
3. Submit a pull request with a description of your changes.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

---