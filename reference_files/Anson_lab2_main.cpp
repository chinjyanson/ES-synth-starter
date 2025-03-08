#include <Arduino.h>
#include <U8g2lib.h>
#include <bitset>
#include <STM32FreeRTOS.h>
#include <ES_CAN.h>  // Provided CAN library

// Hardware timer for 22kHz sawtooth wave generation
HardwareTimer sampleTimer(TIM1);

// Global Variables
volatile uint32_t currentStepSize = 0;  // Shared with ISR
volatile bool canTxSuccess = false;
volatile bool canRxSuccess = false;


// System state (FreeRTOS-safe)
struct {
    std::bitset<32> inputs;
    SemaphoreHandle_t mutex;
    int knob3Rotation;
    std::bitset<12> keyStates;
} sysState;

// Quadrature decoder state for knob 3
uint8_t prevKnobState = 0;

// Global variable to store the latest received CAN message for display
uint8_t globalRXMessage[8] = {0};

// Queues for CAN messages
QueueHandle_t msgInQ;
QueueHandle_t msgOutQ;

// Semaphore for CAN TX (three available mailbox slots)
SemaphoreHandle_t CAN_TX_Semaphore;


// Pin definitions
const int RA0_PIN = D3, RA1_PIN = D6, RA2_PIN = D12, REN_PIN = A5;
const int C0_PIN = A2, C1_PIN = D9, C2_PIN = A6, C3_PIN = D1;
const int OUT_PIN = D11, OUTL_PIN = A4, OUTR_PIN = A3;
const int JOYY_PIN = A0, JOYX_PIN = A1;
const int DEN_BIT = 3, DRST_BIT = 4, HKOW_BIT = 5, HKOE_BIT = 6;

// Display driver object
U8G2_SSD1305_128X32_ADAFRUIT_F_HW_I2C u8g2(U8G2_R0);

// Function prototypes
void scanKeysTask(void *pvParameters);
void displayUpdateTask(void *pvParameters);
void CAN_TX_Task(void *pvParameters);
void CAN_RX_Task(void *pvParameters);

void setOutMuxBit(const uint8_t bitIdx, const bool value) {
    digitalWrite(REN_PIN, LOW);
    digitalWrite(RA0_PIN, (bitIdx & 0x01) ? HIGH : LOW);
    digitalWrite(RA1_PIN, (bitIdx & 0x02) ? HIGH : LOW);
    digitalWrite(RA2_PIN, (bitIdx & 0x04) ? HIGH : LOW);
    digitalWrite(OUT_PIN, value ? HIGH : LOW);
    digitalWrite(REN_PIN, HIGH);
    delayMicroseconds(2);
    digitalWrite(REN_PIN, LOW);
}

std::array<uint32_t, 12> getArray() {
    int cur_octave = 4;
    double freq_factor = pow(2, 1.0/12.0);
    std::array<uint32_t, 12> result = {0};
    double freq = 0.0;

    for (size_t i = 0; i < 12; i++) {
        if (i >= 9) {
            freq = 440 * pow(freq_factor, (i - 9)); 
        } else {
            freq = 440 / pow(freq_factor, (9 - i));
        }
        if (globalRXMessage[1] != 0) {
            cur_octave = globalRXMessage[1];
        }
        result[i] = ((1 << (cur_octave - 4)) * (pow(2, 32) * freq)) / 22000;
    }
    return result;
}


// Constants (example step sizes)
std::array<uint32_t, 12> stepSizes = getArray();

void setRow(uint8_t row) {
    digitalWrite(REN_PIN, LOW);
    digitalWrite(RA0_PIN, (row & 0x01) ? HIGH : LOW);
    digitalWrite(RA1_PIN, (row & 0x02) ? HIGH : LOW);
    digitalWrite(RA2_PIN, (row & 0x04) ? HIGH : LOW);
    digitalWrite(REN_PIN, HIGH);
}

std::bitset<4> readCols() {
    std::bitset<4> result;
    result[0] = !digitalRead(C0_PIN);
    result[1] = !digitalRead(C1_PIN);
    result[2] = !digitalRead(C2_PIN);
    result[3] = !digitalRead(C3_PIN);
    return result;
}

void sampleISR() {
    static uint32_t phaseAcc = 0;
    uint32_t stepSize = __atomic_load_n(&currentStepSize, __ATOMIC_RELAXED);
    phaseAcc += stepSize;
    int32_t Vout = (phaseAcc >> 24) - 128;

    // Get knob rotation atomically for volume control.
    int knobRot = __atomic_load_n(&sysState.knob3Rotation, __ATOMIC_RELAXED);
    knobRot = constrain(knobRot, 0, 8);
    // Apply volume control using right shift.
    Vout = Vout >> (8 - knobRot);

    analogWrite(OUTR_PIN, Vout + 128);
}

void updateStepSizeFromKeys(const std::bitset<12>& keyStates) {
    uint32_t localStepSize = 0;
    
    // ✅ Recalculate stepSizes dynamically based on the latest CAN message
    // stepSizes = getArray(); 

    // Use the last active key (if any) to choose the step size.
    for (int i = 0; i < 12; i++) {
        if (keyStates[i]) {
            localStepSize = stepSizes[i];
        }
    }

    // ✅ Display the step size on the OLED for debugging
    u8g2.setCursor(2, 20);
    u8g2.print("Step: ");
    u8g2.print(globalRXMessage[1]);
    u8g2.sendBuffer();

    __atomic_store_n(&currentStepSize, localStepSize, __ATOMIC_RELAXED);
}


std::bitset<12> scanKeys() {
    std::bitset<12> allKeys;
    // Scan rows 0 to 2 (3 rows x 4 cols = 12 keys)
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

void displayCurrentNote(const std::bitset<12>& keyStates) {
    const char* noteNames[12] = {
        "C ", "C#", "D ", "D#", "E ", "F ",
        "F#", "G ", "G#", "A ", "A#", "B "
    };
    u8g2.setCursor(60, 30);
    bool foundNote = false;
    for (int i = 0; i < 12; i++) {
        if (keyStates[i]) {
            u8g2.print(noteNames[i]);
            foundNote = true;
        }
    }
    if (!foundNote) {
        u8g2.print("No Note");
    }
}

// Quadrature decoder for knob 3 (Row 3, Columns 0 & 1)
void decodeKnob() {
    setRow(3);
    delayMicroseconds(3);
    std::bitset<4> cols = readCols();
    // Columns: 0 is A, 1 is B.
    uint8_t currentState = (cols[1] << 1) | cols[0];

    int rotation = 0;
    if ((prevKnobState == 0b00 && currentState == 0b01) ||
        (prevKnobState == 0b01 && currentState == 0b11) ||
        (prevKnobState == 0b11 && currentState == 0b10) ||
        (prevKnobState == 0b10 && currentState == 0b00)) {
        rotation = +1;
    } else if ((prevKnobState == 0b00 && currentState == 0b10) ||
               (prevKnobState == 0b10 && currentState == 0b11) ||
               (prevKnobState == 0b11 && currentState == 0b01) ||
               (prevKnobState == 0b01 && currentState == 0b00)) {
        rotation = -1;
    }
    
    xSemaphoreTake(sysState.mutex, portMAX_DELAY);
    sysState.knob3Rotation = constrain(sysState.knob3Rotation + rotation, 0, 8);
    xSemaphoreGive(sysState.mutex);

    prevKnobState = currentState;
}

// --- CAN Bus Integration ---

// CAN RX ISR: Reads incoming CAN message and enqueues it.
void CAN_RX_ISR(void) {
    uint8_t RX_Message_ISR[8];
    uint32_t id;
    CAN_RX(id, RX_Message_ISR);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(msgInQ, RX_Message_ISR, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// CAN TX ISR: Releases a transmit mailbox slot.
void CAN_TX_ISR(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(CAN_TX_Semaphore, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void setup() {
    // Pin setup
    pinMode(RA0_PIN, OUTPUT); pinMode(RA1_PIN, OUTPUT); pinMode(RA2_PIN, OUTPUT);
    pinMode(REN_PIN, OUTPUT); pinMode(OUT_PIN, OUTPUT); pinMode(OUTL_PIN, OUTPUT);
    pinMode(OUTR_PIN, OUTPUT); pinMode(LED_BUILTIN, OUTPUT);
    pinMode(C0_PIN, INPUT); pinMode(C1_PIN, INPUT); pinMode(C2_PIN, INPUT); pinMode(C3_PIN, INPUT);
    pinMode(JOYX_PIN, INPUT); pinMode(JOYY_PIN, INPUT);

    setOutMuxBit(DRST_BIT, LOW);
    delayMicroseconds(2);
    setOutMuxBit(DRST_BIT, HIGH);
    u8g2.begin();
    setOutMuxBit(DEN_BIT, HIGH);

    Serial.begin(9600);
    Serial.println("Hello World");

    // Configure and start the hardware timer for audio generation.
    sampleTimer.setOverflow(22000, HERTZ_FORMAT);
    sampleTimer.attachInterrupt(sampleISR);
    sampleTimer.resume();

    // Create the mutex BEFORE starting tasks.
    sysState.mutex = xSemaphoreCreateMutex();
    sysState.knob3Rotation = 8;  // Initialize knob rotation to maximum (no attenuation)

    // --- Initialize CAN Bus ---
    CAN_Init(false);  // Loopback mode for testing; set to false for normal operation.
    setCANFilter(0, 0);
    CAN_RegisterRX_ISR(CAN_RX_ISR); // registers the ISR (as soon as message is received, it will call the ISR)
    CAN_RegisterTX_ISR(CAN_TX_ISR);
    CAN_Start();

    // Create CAN message queues.
    msgInQ = xQueueCreate(36, 8);
    msgOutQ = xQueueCreate(36, 8);

    // Create the CAN TX semaphore (3 mailboxes available).
    CAN_TX_Semaphore = xSemaphoreCreateCounting(3, 3);

    // Create tasks.
    xTaskCreate(scanKeysTask, "scanKeys", 128, NULL, 2, NULL);
    xTaskCreate(displayUpdateTask, "displayUpdate", 256, NULL, 1, NULL);
    xTaskCreate(CAN_TX_Task, "CAN_TX", 128, NULL, 3, NULL);
    xTaskCreate(CAN_RX_Task, "CAN_RX", 128, NULL, 3, NULL);
    vTaskStartScheduler();
}

void scanKeysTask(void *pvParameters) {
    const TickType_t xFrequency = 50 / portTICK_PERIOD_MS;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    static std::bitset<12> previousKeys;  // To detect transitions

    while (1) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        std::bitset<12> localKeys = scanKeys();
        decodeKnob();
        
        // Detect key state changes and send a CAN message for each transition.
        for (int i = 0; i < 12; i++) {
            if (localKeys[i] != previousKeys[i]) {
                uint8_t TX_Message[8] = {0};
                // Use 'P' for press, 'R' for release.
                TX_Message[0] = localKeys[i] ? 'P' : 'R';
                TX_Message[1] = 5;  // Example octave number; adjust as needed.
                TX_Message[2] = i;  // Note number.
                // Place message on the transmit queue.
                xQueueSend(msgOutQ, TX_Message, portMAX_DELAY);
            }
        }
        previousKeys = localKeys;
        
        // Update shared key states and step size.
        xSemaphoreTake(sysState.mutex, portMAX_DELAY);
        sysState.keyStates = localKeys;
        xSemaphoreGive(sysState.mutex);
        updateStepSizeFromKeys(localKeys);
    }
}

void displayUpdateTask(void *pvParameters) {
    const TickType_t xFrequency = 100 / portTICK_PERIOD_MS;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    while (1) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        xSemaphoreTake(sysState.mutex, portMAX_DELAY);
        std::bitset<12> localKeys = sysState.keyStates;
        int localKnob = sysState.knob3Rotation;
        xSemaphoreGive(sysState.mutex);

        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_ncenB08_tr);
        u8g2.drawStr(2, 10, "Keys:");
        // u8g2.setCursor(2, 20);
        // u8g2.print(localKeys.to_ulong(), HEX);
        // displayCurrentNote(localKeys);
        
        // Display knob rotation
        u8g2.setCursor(2, 30);
        u8g2.print("Knob:");
        u8g2.print(localKnob);
        
        // Display CAN status
        u8g2.setCursor(70, 10);
        u8g2.print("TX: ");
        u8g2.print(globalRXMessage[1]);

        u8g2.setCursor(70, 20);
        u8g2.print("RX: ");
        u8g2.print(canRxSuccess ? "OK" : "Fail");

        u8g2.sendBuffer();
    
        digitalToggle(LED_BUILTIN);
    }
}


void CAN_TX_Task(void *pvParameters) {
    uint8_t msgOut[8];
    while (1) {
        // Block until a message is available on the transmit queue.
        xQueueReceive(msgOutQ, msgOut, portMAX_DELAY);
        // Wait for an available transmit mailbox.
        xSemaphoreTake(CAN_TX_Semaphore, portMAX_DELAY);
        CAN_TX(0x123, msgOut);
        if (CAN_TX(0x123, msgOut) == 0) {
            canTxSuccess = true;  // ✅ Message successfully sent
        }
    }
}

void CAN_RX_Task(void *pvParameters) {
    uint8_t msgIn[8];
    uint32_t id;
    while (1) {
        xQueueReceive(msgInQ, msgIn, portMAX_DELAY);
        noInterrupts();
        memcpy(globalRXMessage, msgIn, 8);
        canRxSuccess = true;  // ✅ Message successfully received
        interrupts();

        stepSizes = getArray();
    }
}

void loop() {}  // FreeRTOS handles all tasks