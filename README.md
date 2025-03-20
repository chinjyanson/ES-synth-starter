# Documentation

## Table of Contents
1. [Introduction](#1-introduction)  
2. [Features](#2-features)  
    2.1. [Core Features](#21-core-features)      
    2.2. [Advanced Features](#22-advanced-features)  
3. [Tasks](#3-tasks)  
    3.1. [scanKeyTask](#31-scankeytask)  
    3.2. [displayUpdateTask](#32-displayupdatetask)    
    3.3. [sampleISR](#33-sampleisr)    
    3.4. [CAN_TX_Task](#34-can_tx_task)  
    3.5. [CAN_RX_Task](#35-can_rx_task)  
    3.6. [CAN_TX_ISR](#36-can_tx_isr)  
    3.7. [CAN_RX_ISR](#37-can_rx_isr)
    3.8. [gameTask](#38-gametask)
4. [Analysis](#4-analysis)   
5. [References](#5-references)  

---

## 1. Introduction 

This project focuses on the embedded software development for controlling a music synthesizer using an ST NUCLEO-L432KC microcontroller. It incorporates various real-time programming and system analysis techniques to successfully implement a range of features for the keyboard-based embedded system, as detailed in this documentation.

**Technical Specifications**:  
The microcontroller is equipped with an STM32L432KCU6U processor, which is based on an Arm Cortex-M4 core. Each keyboard module corresponds to one octave, with 12 keys, 4 control knobs, and a joystick for input. The outputs consist of two audio channels and an OLED display. Multiple keyboard modules can be connected and stacked using a CAN bus for communication.

**Resources Utilized**:  
- Libraries: STM32duino FreeRTOS and U8g2 Display Driver Libraries  
- Frameworks: STM32duino  
- Hardware Abstraction Layers (HALs): CMSIS/STM32Cube  

This project was developed by EEE and EIE students from Imperial College London for the second coursework of the ELEC60013 - Embedded Systems module. 

**Team members:** Anson Chin, Eddie Moualek and Samuel Khoo 

---

## 2. Features

### 2.1. Core Features

- The synthesizer plays the correct music note with a sawtooth wave when the corresponding key is pressed without any delay between the key press and the tone starting.
- There are 8 different volume settings which can be controlled and adjusted with a knob.
- The OLED display shows the current notes being played and the current volume setting, amongst other additional information.
  - The OLED display refreshes and the LED LD3 (on the MCU module) toggles every 100ms.
- The synthesizer can be configured as a sender or receiver at compile time.
  - If configured as a sender, the synthesizer sends the appropriate note(s) when a key is pressed/released as a message via the CAN bus.
  - If configured as a receiver, the synthesizer plays/stops playing the appropriate note(s) after receiving the message.

### 2.2. Advanced Features

- **Octave Knob**
  - Knob X was programmed to control the octave that the corresponding board/module plays in, with the octave ranging from 0-7.

- **Hidden Game (Tone Guessing)**
  - A hidden tone guessing game was implemented, where the system generates a random note, and the user must guess which key corresponds to that note.
  - The user is provided with feedback on whether their guess is correct or not, and the game continues with a new note after each guess.
  - This game provides an interactive and fun way for users to engage with the synthesizer while testing their musical knowledge and recognition of different tones.

---

## 3. Tasks and Interrupts

### 3.1. ScanKeyTask
Handles keyArray reads and registers them into various parameters (Knobs, Menu, etc.). It also generates messages for the CAN task and inputs for the sound generator. ADSR stepping happens here.

- **Initiation Interval**: 20 milliseconds
- **Measured Maximum Execution Time**: 241 microseconds (all 12 keys pressed)

### 3.2. DisplayUpdateTask
Updates the OLED screen with information on the synthesizer’s status, including the current waveform, volume, and octave settings.

- **Initiation Interval**: 100 milliseconds
- **Measured Maximum Execution Time**: 18,604 microseconds (all 12 keys pressed)

### 3.3. SampleISR
Interrupt responsible for generating the audio output signal.

- **Initiation Interval**: 45.45 microseconds
- **Measured Maximum Execution Time**: 28.0 microseconds (all 12 keys pressed)

### 3.4. CAN_TX_Task
Manages the transmission of CAN messages for the synthesized note(s).

- **Initiation Interval**: 60 milliseconds for 36 iterations
- **Measured Maximum Execution Time**: 12 microseconds

### 3.5. CAN_RX_Task
Handles incoming CAN messages and takes the necessary action (e.g., playing or stopping a note).

- **Initiation Interval**: 25.2 milliseconds for 36 iterations
- **Measured Maximum Execution Time**: 82.7 microseconds

### 3.6. CAN_TX_ISR
Triggered when a CAN message is sent, ensuring that the CAN transmission buffer does not overflow.

- **Initiation Interval**: 60 milliseconds for 36 iterations
- **Measured Maximum Execution Time**: 5.2 microseconds

### 3.7. CAN_RX_ISR
Triggered when a CAN message is received and copies it to the incoming message queue.

- **Initiation Interval**: 25.2 milliseconds for 36 iterations
- **Measured Maximum Execution Time**: 10 microseconds

---

## 4. Analysis

### 4.1. Shared Resources

The following resources are shared among multiple tasks and protected by appropriate synchronization mechanisms:

```cpp
struct SystemState {
    bool areAllKnobSPressed;
    bool gameActiveOverride = false;
    int knob3Rotation;
    std::bitset<32> inputs;
    std::bitset<12> keyStates;
};
```

- **`inputs`**: This bitset stores the status of all input keys and control elements. It is accessed by different tasks and is protected by the `mutex` to ensure thread safety.
- **`areAllKnobSPressed`**: This boolean variable tracks whether all knobs are pressed. It is accessed and modified by various tasks and is synchronized using the `mutex`.
- **`gameActiveOverride`**: A boolean flag that controls whether the game override is active. It can be accessed by different tasks, and its access is protected by the `mutex`.
- **`knob3Rotation`**: This variable tracks the rotation of knob 3 and is used by multiple tasks. Its access is synchronized using the `mutex`.
- **`keyStates`**: A bitset representing the states of the 12 keys. It is shared between tasks and protected using the `mutex` to prevent data inconsistencies.


### 4.2. Deadlocks

A **deadlock** occurs when two or more tasks are in a state of waiting for each other to release resources (e.g., mutexes) that they need to continue execution. This can lead to a situation where none of the tasks involved can proceed, which would cause the system to freeze or hang. 

In real-time embedded systems, preventing deadlocks is crucial for ensuring system stability and responsiveness. This can be achieved through careful management of mutexes, ensuring that tasks access shared resources in a safe, consistent manner.

#### Mutex Usage

In our system, a **single mutex** (`sysState.mutex`) is used to protect the shared `SystemState` structure. This structure includes critical variables, such as `keyStates`, that are accessed by multiple tasks. The mutex is used to ensure **mutual exclusion** when modifying these shared resources, preventing race conditions where multiple tasks try to access or modify the variables simultaneously.

Here is an example of how the mutex is used:

```cpp
xSemaphoreTake(sysState.mutex, portMAX_DELAY);  // Wait for the mutex
sysState.keyStates = localKeys;                 // Modify the shared resource
xSemaphoreGive(sysState.mutex);                 // Release the mutex
```

### 4.3. Timing Analysis

- **ScanKeyTask**: 241 μs  
- **DisplayUpdateTask**: 18,604 μs  
- **SampleISR**: 28.0 μs  
- **CAN_TX_Task**: 12 μs  
- **CAN_RX_Task**: 82.7 μs  
- **CAN_TX_ISR**: 5.2 μs  
- **CAN_RX_ISR**: 10 μs  

### 4.4. CPU Utilization - Rate Monotonic Scheduler Critical Instant Analysis

| Task Name           | Initiation Interval ($τ_i$) | Execution Time ($T_i$) | $[\frac{maximum τ_i}{τ_i}]$ | CPU Utilisation $[\frac{T_i}{τ_i}]$ |
|---------------------|-----------------------------|------------------------|----------------------------|--------------------------------------|
| **scanKeyTask**      | 20 ms                       | 241 μs                 | 1.205%                     | 1.205%                               |
| **displayUpdateTask**| 100 ms                      | 18.6 ms                | 18.6%                      | 18.6%                                |
| **sampleISR**        | 45.45 μs                    | 28.0 μs                | 61.6%                      | 61.6%                                |
| **CAN_TX_Task**      | 60 ms                       | 12 μs                  | 0.72%                      | 0.72%                                |
| **CAN_RX_Task**      | 25.2 ms                     | 82.7 μs                | 4.43%                      | 4.43%                                |
| **CAN_TX_ISR**       | 60 ms                       | 5.2 μs                 | 0.312%                     | 0.312%                               |
| **CAN_RX_ISR**       | 25.2 ms                     | 10 μs                  | 1.429%                     | 1.429%                               |

### Total CPU Utilization:
The total CPU utilization in the system is:

\[
\text{Total CPU Utilization} = 1.205\% + 18.6\% + 61.6\% + 4.43\% + 1.429\% + 0.72\% + 0.312\% = 88.295\%
\]

This indicates that the system is efficiently utilizing most of its resources (about 88.3%) without overwhelming the processor, leaving a buffer of approximately 11.7% for other operations or future optimization.

### 4.5. Conclusion
This analysis proves that the real-time operating system (RTOS) effectively manages all tasks within the available computational resources of the microcontroller. Despite various tasks demanding CPU time, the system manages to avoid deadlocks, handle shared resources efficiently, and fulfill all deadlines, ensuring smooth operation of the synthesizer. With the system's CPU utilization well below 100%, there is ample capacity for additional features, optimizations, or handling unforeseen computational demands.

---

## 5. References

1. STM32L432KCU6U Processor Datasheet: [STMicroelectronics](https://www.st.com/en/microcontrollers-microprocessors/stm32l432kc.html)
2. STM32Cube HAL Documentation: [STMicroelectronics STM32Cube](https://www.st.com/en/development-tools/stm32cube.html)
3. STM32duino FreeRTOS Library Documentation: [STM32duino FreeRTOS](https://github.com/stm32duino/STM32FreeRTOS)
4. U8g2 Display Driver Documentation: [U8g2 GitHub Repository](https://github.com/olikraus/u8g2)