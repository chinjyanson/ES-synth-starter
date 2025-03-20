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

## 3. Tasks and Interrupts

### 3.1. ScanKeyTask (Thread)

The `ScanKeyTask` is a **thread** responsible for scanning the 12-key keyboard, updating relevant parameters, handling CAN messaging, and managing ADSR envelope progression.

#### **Task Overview**
- **Implementation**: Thread (FreeRTOS task)
- **Initiation Interval**: 20 milliseconds (50 Hz) (NOT SURE!!!!)
- **Max Execution Time**: 237 microseconds (all 12 keys pressed)

#### **Key Scanning Process**
The `scanKeys()` function scans a 3x4 matrix by setting each row active and reading the columns to detect key presses. The results are stored in a 12-bit bitset.

```cpp
std::bitset<12> scanKeys() {
    std::bitset<12> allKeys;
    for (uint8_t row = 0; row < 3; row++) {
        setRow(row);
        delayMicroseconds(3);
        std::bitset<4> cols = readCols();
        for (uint8_t col = 0; col < 4; col++) {
            allKeys[row * 4 + col] = cols[col];
        }
    }
    return allKeys;
}
```

#### **Key State Change Detection**
`scanKeysTask` identifies key transitions (press/release) and sends the corresponding CAN messages.

```cpp
for (int i = 0; i < 12; i++) {
    if (localKeys[i] != previousKeys[i]) {
        uint8_t TX_Message[8] = { localKeys[i] ? 'P' : 'R', getOctaveNumber(), i };
        xQueueSend(msgOutQ, TX_Message, portMAX_DELAY);
    }
}
```

#### **ADSR Envelope Stepping**
The task updates `currentStepSize` based on key presses while ensuring thread safety.

```cpp
void updateStepSizeFromKeys(const std::bitset<12>& keyStates) {
    uint32_t localStepSize = 0;

    xSemaphoreTake(sysState.mutex, portMAX_DELAY);
    bool gameOverride = sysState.gameActiveOverride;
    xSemaphoreGive(sysState.mutex);
    if (gameOverride) return;

    xSemaphoreTake(sysMutex, portMAX_DELAY);
    for (int i = 0; i < 12; i++) {
        if (keyStates[i]) localStepSize = stepSizes[i];
    }
    xSemaphoreGive(sysMutex);

    __atomic_store_n(&currentStepSize, localStepSize, __ATOMIC_RELAXED);
}
```

#### **Thread Safety Considerations**
- **Mutex Usage**: Ensures exclusive access to shared resources.
- **Atomic Operations**: Prevents race conditions when updating `currentStepSize`.
- **Delays**: A short delay (`delayMicroseconds(3)`) stabilizes row activation, reducing false detections.

---

### 3.2. DisplayUpdateTask (Thread)

The `DisplayUpdateTask` is a **FreeRTOS thread** responsible for updating the OLED display with real-time system information, including pressed keys, knob positions, and game-related messages.  

#### **Task Overview**  
- **Implementation**: Thread (FreeRTOS task)  
- **Initiation Interval**: 100 milliseconds  
- **Measured Maximum Execution Time**: 24,156 microseconds (all 12 keys pressed)  

#### **How It Works**  

1. **Regular Updates with Timing Control**  
   - Runs **every 100 milliseconds**, ensuring a smooth and responsive display.  
   - Uses FreeRTOS **task scheduling** to prevent excessive CPU usage.  

2. **System State Retrieval**  
   - Locks **shared system variables** (key states, knob rotation, game mode) with a mutex to prevent data corruption.  
   - Reads the state of pressed keys and **displays them in hex format** alongside the corresponding musical note.  
   - Checks the knob’s rotation value to show volume settings.  
   - Displays **CAN bus status** (TX/RX communication success/failure).  

3. **OLED Screen Updates**  
   - Clears the screen before each update to prevent overlapping text.  
   - Uses **buffered rendering** (`clearBuffer()` and `sendBuffer()`) to minimize flickering.  

4. **Game Mode Integration**  
   - If the synthesizer is in **game mode**, the display shows different messages:  
     - **Waiting for user input** → Prompts the user to make a guess.  
     - **Playing music** → Indicates that music is currently being played.  
     - **Correct/Wrong guess** → Provides feedback and reveals the correct answer if needed.  

5. **Debugging & System Feedback**  
   - **Toggles the built-in LED** to signal each update cycle.  
   - Includes a **fail-safe message** ("how are you here") for unexpected states.  
   - Supports a **test mode (`TEST_DISPLAY`)** to allow manual debugging.  

#### **Key Design Considerations**  

- **Optimized Execution Time**: 24.1 ms max, ensuring no delay in audio processing.  
- **Efficient Resource Management**: Mutex locks prevent conflicts, and buffered rendering reduces CPU load.  
- **Flexible Display Handling**: Can switch between normal synthesizer mode and game mode dynamically.  

---

### 3.3. SampleISR (Interrupt)

The `SampleISR` is an **interrupt** responsible for generating the audio output signal in real-time. It calculates the next sample in the waveform based on the current step size and adjusts the volume dynamically.  

#### **Task Overview**  
- **Implementation**: Interrupt (ISR)  
- **Initiation Interval**: 50.2 microseconds  
- **Measured Maximum Execution Time**: 30.6 microseconds (all 12 keys pressed)  

#### **Pseudocode Explanation**  

```plaintext
ON INTERRUPT:
    - Load the current step size (atomic operation)
    - Update phase accumulator by adding step size
    - Generate output waveform value from phase accumulator
    - Retrieve knob rotation value for volume control
    - Scale the waveform amplitude based on knob rotation
    - Output the final waveform signal to the DAC
```

#### **SampleISR Logic Breakdown**  

1. **Phase Accumulator Update**  
   - Uses a **static phase accumulator** that increments based on `stepSize`.  
   - Higher `stepSize` values result in a **higher frequency output**.  

2. **Waveform Generation**  
   - Extracts the **8 most significant bits** of `phaseAcc` to generate a waveform.  
   - Converts this to a **signed value (-128 to 127)**.  

3. **Volume Control**  
   - Reads `knob3Rotation` atomically.  
   - The waveform amplitude is scaled using a **right shift** operation.  

4. **Final Output to DAC**  
   - The final waveform value is **offset by 128** to fit the DAC’s range (0–255).  
   - Uses `analogWrite()` to send the processed waveform to the output pin.  

#### **Key Features and Considerations**  

- **Atomic Operations**  
  - Ensures thread safety when reading `currentStepSize` and `knob3Rotation`.  

- **Optimized Execution**  
  - Uses **bitwise operations** for fast waveform computation.  
  - Keeps ISR execution time **low** for real-time audio generation.  

- **Efficient Volume Control**  
  - Uses **bit-shifting instead of multiplication** to quickly scale amplitude.  

---

### 3.4. CAN_TX_Task (Thread)
Manages the transmission of CAN messages for the synthesized note(s).

- **Implementation**: Thread (FreeRTOS task)
  - This task is implemented as a FreeRTOS thread that continuously waits on a transmit queue (`msgOutQ`). When a new message is enqueued (for example, from key state changes), the task retrieves the message, then takes a counting semaphore (`CAN_TX_Semaphore`) to ensure that there is a free CAN transmission mailbox. Once a mailbox is available, it calls `CAN_TX(0x123, msgOut)` to send the message. This design decouples the application-level message generation from the actual transmission, ensuring that tasks do not block on hardware delays.
- **Initiation Interval**: 60 milliseconds for 36 iterations
  - In the worst-case scenario, 36 messages could be generated in 60 milliseconds. This means that the task is expected to process an iteration every 60 ms for the batch of 36 messages, ensuring that the transmit queue does not overflow even under high load.
- **Measured Maximum Execution Time**: 12 microseconds
  - The execution time per iteration has been measured at approximately 12 microseconds. This low overhead is achieved by using non-blocking queue operations and the counting semaphore to manage the limited hardware resources.

---

### 3.5. CAN_RX_Task (Thread)
Handles incoming CAN messages and takes the necessary action (e.g., playing or stopping a note).

- **Implementation**: Thread (FreeRTOS task)
  - This task (referred to as the decodeTask in the code) is implemented as a FreeRTOS thread that blocks on a reception queue (`msgInQ`). When a CAN message is received, the `CAN_RX_ISR` enqueues the message into `msgInQ`. The decode task then retrieves each message and processes it: for key press messages, it calculates the new step size by combining a base frequency (from a predefined table) with an octave multiplier (using bit shifts); for key release messages, it sets the step size to zero. Updates to shared variables, such as the global step size and the display buffer for the last received message, are protected by a mutex.
- **Initiation Interval**: 25.2 milliseconds for 36 iterations
  - Under worst-case conditions, if 36 messages are received, the task should ideally process them within 25.2 milliseconds in total. This interval ensures that even in high-traffic conditions, the system’s response remains within acceptable real-time bounds.
- **Measured Maximum Execution Time**: 82.7 microseconds
  - The maximum execution time per iteration is around 82.7 microseconds. This includes the time to dequeue a message, process its contents, update shared state, and copy data for display, all while ensuring thread safety via mutex locks.



---

### 3.6. CAN_TX_ISR (Interrupt)
Triggered when a CAN message is sent, ensuring that the CAN transmission buffer does not overflow.

- **Implementation**: Interrupt (ISR)
  - The `CAN_TX_ISR` is an interrupt service routine that is invoked whenever a transmission mailbox becomes available. Its sole purpose is to release the counting semaphore `(CAN_TX_Semaphore)` using `xSemaphoreGiveFromISR()`. This signal allows the `CAN_TX_Task` to know that a new mailbox is free for a subsequent transmission. Because this ISR only performs a very short operation, it minimizes the risk of interrupt latency affecting system performance.
- **Initiation Interval**: 60 milliseconds for 36 iterations
  - This ISR is called whenever a mailbox frees up. In a scenario where 36 messages are transmitted every 60 milliseconds, the effective initiation interval for the ISR events remains consistent with the system’s transmission rate.
- **Measured Maximum Execution Time**: 5.2 microseconds
  - The measured maximum execution time is approximately 5.2 microseconds. The ISR’s brief execution time helps maintain system responsiveness and minimizes disruption to other real-time tasks.



---

### 3.7. CAN_RX_ISR (Interrupt)
Triggered when a CAN message is received and copies it to the incoming message queue.

- **Implementation**: Interrupt (ISR)
  - The `CAN_RX_ISR` is invoked upon reception of a CAN message. Inside the ISR, the message is immediately read from the CAN hardware using `CAN_RX()` and then enqueued into the msgInQ using `xQueueSendFromISR()`. This rapid hand-off ensures that the ISR remains short and non-blocking, with the heavier processing deferred to the `CAN_RX_Task` (decodeTask).
- **Initiation Interval**: 25.2 milliseconds for 36 iterations
  - In the worst-case scenario, where 36 messages could be received in 25.2 milliseconds, the ISR is triggered as each message arrives, ensuring continuous and timely processing.
- **Measured Maximum Execution Time**: 10 microseconds
  - The maximum execution time for the `CAN_RX_ISR` has been measured at approximately 10 microseconds. This brief execution period is critical for maintaining the integrity of real-time processing and ensuring that no messages are lost due to long ISR blocking times.


---

### 3.8. GameTask (Thread)

The `GameTask` is a **thread** that manages the note recognition game. It plays a randomly selected note and waits for the user to press the correct key before proceeding to the next round.  

#### **Task Overview**  
- **Implementation**: Thread (FreeRTOS task)

#### **Pseudocode Explanation**  

```plaintext
LOOP forever:
    - Check if game mode is activated (all knobs pressed)
    - IF not activated:
        - Wait briefly and continue

    - Select a random note from predefined note list
    - Play the note for 2 seconds
    - Stop the note after playing

    - Allow user input (monitor key presses)
    - WHILE no key is pressed:
        - Wait and continue monitoring

    - Check if the pressed key matches the correct note
    - Store result (correct or incorrect)
    - Display correct answer
    - Wait before starting the next round
```

#### **GameTask Logic Breakdown**  

1. **Game Activation**  
   - The task continuously checks whether the game is active by monitoring if all knobs are pressed.  

2. **Note Selection and Playback**  
   - A random note is selected from an array of predefined frequencies.  
   - The system plays the selected note for 2 seconds by updating `currentStepSize`.  

3. **Waiting for User Input**  
   - After playing the note, the system stops the sound.  
   - The game then waits for the user to press any key.  

4. **Checking User Response**  
   - If the user presses the correct key, it registers a correct guess.  
   - Otherwise, the system marks the response as incorrect and displays the correct answer.  

5. **Prepare for Next Round**  
   - The game introduces a short delay before selecting a new note.  
   - It then checks if the game mode is still active before continuing.  

#### **Key Features and Considerations**  

- **Thread Safety**  
  - Uses `sysState.gameActiveOverride` to prevent unintended key presses during note playback.  
  - Ensures safe access to shared variables with mutexes.  

- **Efficient Execution**  
  - Uses atomic operations to update `currentStepSize`, preventing race conditions.  
  - Short delays prevent excessive CPU usage.  

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
- **CAN_TX_Task**: 10 μs 
- **CAN_RX_Task**: 119 μs 

### 4.4. CPU Utilization - Rate Monotonic Scheduler Critical Instant Analysis

| Task Name           | Initiation Interval ($τ_i$) | Execution Time ($T_i$) | $[\frac{maximum τ_i}{τ_i}]$ | CPU Utilisation $[\frac{T_i}{τ_i}]$ |
|---------------------|-----------------------------|------------------------|----------------------------|--------------------------------------|
| **scanKeyTask**      | 20 ms                       | 241 μs                 | 1.205%                     | 1.205%                               |
| **displayUpdateTask**| 100 ms                      | 18.6 ms                | 18.6%                      | 18.6%                                |
| **sampleISR**        | 45.45 μs                    | 28.0 μs                | 61.6%                      | 61.6%                                |
| **CAN_TX_Task**      | 60 ms                       | 10 μs                  | 0.72%                      | 0.72%                                |
| **CAN_RX_Task**      | 25.2 ms                     | 119 μs                 | 4.43%                      | 4.43%                                |

### Total CPU Utilization:
The total CPU utilization in the system is:

\[
\text{Total CPU Utilization} = 1.205\% + 18.6\% + 61.6\% + 4.43\% + 1.429\% + 0.72\% + 0.312\% = 88.295\%
\]

This indicates that the system is efficiently utilizing most of its resources (about 88.3%) without overwhelming the processor, leaving a buffer of approximately 11.7% for other operations or future optimization.

### 4.5. Conclusion
This analysis proves that the real-time operating system (RTOS) effectively manages all tasks within the available computational resources of the microcontroller. Despite various tasks demanding CPU time, the system manages to avoid deadlocks, handle shared resources efficiently, and fulfill all deadlines, ensuring smooth operation of the synthesizer. With the system's CPU utilization well below 100%, there is ample capacity for additional features, optimizations, or handling unforeseen computational demands.

## 5. References

1. STM32L432KCU6U Processor Datasheet: [STMicroelectronics](https://www.st.com/en/microcontrollers-microprocessors/stm32l432kc.html)
2. STM32Cube HAL Documentation: [STMicroelectronics STM32Cube](https://www.st.com/en/development-tools/stm32cube.html)
3. STM32duino FreeRTOS Library Documentation: [STM32duino FreeRTOS](https://github.com/stm32duino/STM32FreeRTOS)
4. U8g2 Display Driver Documentation: [U8g2 GitHub Repository](https://github.com/olikraus/u8g2)
