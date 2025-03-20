# Embedded System Report

## 1. Introduction
This report details the development of an embedded system utilizing the **STM32 microcontroller**, focusing on **real-time task management, CAN communication, user input handling, and display updates**. The system integrates **FreeRTOS** for multitasking and employs **hardware timers** to achieve efficient execution of real-time operations.

The primary goal of the project is to develop a **piano interface** with features such as **key scanning, knob decoding, real-time display updates, and CAN bus communication**. Additionally, an **interactive hidden game** is incorporated as an extension.

## 2. Features
The system provides the following key features:

- **Real-Time Task Management**: Utilizes FreeRTOS for multitasking to ensure efficient operation of concurrent tasks.
- **CAN Bus Communication**: Implements CAN transmission and reception for inter-device communication.
- **Key Scanning**: Detects pressed keys and maps them to corresponding musical notes.
- **Knob Control**: Reads the rotational position of a knob and detects specific press combinations.
- **Display Update System**: Uses an OLED display to provide real-time system feedback.
- **Hidden Game Mode**: Implements an interactive game that activates upon specific user interactions.
- **Performance Benchmarking**: Measures execution times for display updates, ISR execution, and key scanning to optimize performance.

## 3. Tasks
The system is structured into multiple FreeRTOS tasks, each performing specific functions:

### 3.1 `scanKeysTask`
- Periodically scans the keyboard matrix to detect pressed keys.
- Updates the system state with detected key presses.
- Sends CAN messages when key press events occur.

### 3.2 `displayUpdateTask`
- Continuously updates the OLED display with system status, including key presses, knob rotations, and CAN communication status.
- Implements real-time feedback for the user interface.

### 3.3 `CAN_TX_Task` & `CAN_RX_Task`
- Handle CAN transmission and reception.
- Ensures proper synchronization of messages between devices.
- Verifies successful transmission and reception of data.

### 3.4 `gameTask`
- Manages the logic of the hidden game mode.
- Interacts with the user based on key inputs and system states.

## 4. Extensions
The project incorporates several extensions to enhance functionality:

- **Hidden Game Mode**: The system detects a specific combination of knob presses to enable a hidden game mode.
- **Performance Optimization**: Implements worst-case execution time (WCET) analysis for key tasks to identify bottlenecks.
- **Debugging and Logging**: Serial output provides insights into system behavior and assists in troubleshooting.

## 5. Testing & Analysis
A series of test procedures were conducted to ensure system reliability and efficiency:

### 5.1 Execution Time Measurement
To evaluate performance, the system logs execution times for critical tasks:

- **Display Update Timing**: Measures the worst-case execution time for OLED updates.
- **ISR Timing**: Benchmarks the interrupt service routine execution time.
- **Key Scanning Timing**: Evaluates the duration required to scan all keys.
- **CAN Transmission Timing**: Analyzes worst-case delays in CAN messaging.

### 5.2 Functional Testing
Each module was verified individually and integrated progressively:

- **Key Scanning Verification**: Ensured correct detection of pressed keys and mapping to musical notes.
- **Knob Rotation Testing**: Verified knob state transitions and impact on system variables.
- **CAN Communication Testing**: Ensured messages were transmitted and received accurately between nodes.
- **Game Mode Activation**: Confirmed correct detection of the hidden game activation sequence.

### 5.3 Stress Testing
The system was subjected to prolonged operation to identify potential stability issues. This included:

- **High-Frequency CAN Communication**: Simulated rapid CAN messaging to test buffer handling.
- **Continuous Display Updates**: Evaluated the OLED performance under continuous updates.
- **Concurrent Task Execution**: Assessed real-time performance when multiple tasks ran simultaneously.

## 6. References
- **FreeRTOS Documentation**: Official documentation for real-time task scheduling.
- **CAN Bus Protocol Specification**: Standard reference for CAN communication.
- **STM32 Hardware Documentation**: Detailed reference for microcontroller features and peripheral control.
- **U8g2 OLED Library**: Reference for display handling and optimizations.

