#include <Arduino.h>
#include <U8g2lib.h>
#include <bitset>
#include <STM32FreeRTOS.h>

// Hardware timer for 22kHz sawtooth wave generation
HardwareTimer sampleTimer(TIM1);

// Global Variables
volatile uint32_t currentStepSize = 0;  // Shared with ISR

// System state (FreeRTOS-safe)
struct {
    std::bitset<32> inputs;
    SemaphoreHandle_t mutex;
    int knob3Rotation;      // Volume control knob (0 to 8)
    std::bitset<12> keyStates;
} sysState;

// Quadrature decoder state for knob 3
uint8_t prevKnobState = 0;

// Constants
const uint32_t stepSizes[12] = {
    51076057, 54113197, 57330935, 60740010,
    64351799, 68178356, 72232452, 76527617,
    81078186, 85899346, 91007187, 96418756
};

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
    // Atomically load the current step size
    uint32_t stepSize = __atomic_load_n(&currentStepSize, __ATOMIC_RELAXED);
    phaseAcc += stepSize;
    int32_t Vout = (phaseAcc >> 24) - 128;

    // Get knob rotation atomically for volume control.
    // No mutex allowed here so we use an atomic load.
    int knobRot = __atomic_load_n(&sysState.knob3Rotation, __ATOMIC_RELAXED);
    knobRot = constrain(knobRot, 0, 8);
    // Apply volume control using a right-shift.
    Vout = Vout >> (8 - knobRot);

    // Adjust output for analogWrite (range 0-255)
    analogWrite(OUTR_PIN, Vout + 128);
}

void updateStepSizeFromKeys(const std::bitset<12>& keyStates) {
    uint32_t localStepSize = 0;
    // Choose the step size from the last key pressed (if any)
    for (int i = 0; i < 12; i++) {
        if (keyStates[i]) {
            localStepSize = stepSizes[i];
        }
    }
    __atomic_store_n(&currentStepSize, localStepSize, __ATOMIC_RELAXED);
}

std::bitset<12> scanKeys() {
    std::bitset<12> allKeys;
    // Scan rows 0 to 2 for the key matrix (3 rows x 4 cols = 12 keys)
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
    u8g2.setCursor(60, 20);
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

    // Combine the two columns: column 0 is A, column 1 is B.
    uint8_t currentState = (cols[1] << 1) | cols[0];

    int rotation = 0;
    // Check for normal clockwise transitions.
    if ((prevKnobState == 0b00 && currentState == 0b01) ||
        (prevKnobState == 0b01 && currentState == 0b11) ||
        (prevKnobState == 0b11 && currentState == 0b10) ||
        (prevKnobState == 0b10 && currentState == 0b00)) {
        rotation = +1;
    }
    // Check for normal counterclockwise transitions.
    else if ((prevKnobState == 0b00 && currentState == 0b10) ||
             (prevKnobState == 0b10 && currentState == 0b11) ||
             (prevKnobState == 0b11 && currentState == 0b01) ||
             (prevKnobState == 0b01 && currentState == 0b00)) {
        rotation = -1;
    }

    // Update knob rotation atomically using the mutex.
    xSemaphoreTake(sysState.mutex, portMAX_DELAY);
    sysState.knob3Rotation = constrain(sysState.knob3Rotation + rotation, 0, 8);
    xSemaphoreGive(sysState.mutex);

    prevKnobState = currentState;
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
    // Initialize knob3Rotation to maximum (e.g., 8 means no attenuation).
    sysState.knob3Rotation = 8;

    // Create tasks: scanKeysTask has higher priority than displayUpdateTask.
    xTaskCreate(scanKeysTask, "scanKeys", 128, NULL, 2, NULL);
    xTaskCreate(displayUpdateTask, "displayUpdate", 256, NULL, 1, NULL);
    vTaskStartScheduler();
}

void scanKeysTask(void *pvParameters) {
    const TickType_t xFrequency = 50 / portTICK_PERIOD_MS;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    while (1) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        // Scan keys from rows 0-2.
        std::bitset<12> localKeys = scanKeys();
        
        // Decode the knob (row 3) for volume control.
        decodeKnob();
        
        // Update shared key states.
        xSemaphoreTake(sysState.mutex, portMAX_DELAY);
        sysState.keyStates = localKeys;
        xSemaphoreGive(sysState.mutex);
        
        // Update step size from key presses (last pressed key wins).
        updateStepSizeFromKeys(localKeys);
    }
}

void displayUpdateTask(void *pvParameters) {
    const TickType_t xFrequency = 100 / portTICK_PERIOD_MS;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    while (1) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    
        // Retrieve the current key states and knob value.
        xSemaphoreTake(sysState.mutex, portMAX_DELAY);
        std::bitset<12> localKeys = sysState.keyStates;
        int localKnob = sysState.knob3Rotation;
        xSemaphoreGive(sysState.mutex);
    
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_ncenB08_tr);
        u8g2.drawStr(2, 10, "Keys:");
        u8g2.setCursor(2, 20);
        u8g2.print(localKeys.to_ulong(), HEX);
        displayCurrentNote(localKeys);
        // Optionally display knob rotation for debugging.
        u8g2.setCursor(2, 30);
        u8g2.print("Knob:");
        u8g2.print(localKnob);
    
        u8g2.sendBuffer();
    
        digitalToggle(LED_BUILTIN);
    }
}

void loop() {}  // FreeRTOS handles all tasks
