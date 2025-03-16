#include "keys.hpp"
#include "system.hpp"
#include "audio.hpp"
#include "knob.hpp"
#include "config.hpp"

#include <Arduino.h>
#include <bitset>

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
    // In worst-case testing mode, generate a press for every key (regardless of physical state)
    #ifdef TEST_SCAN_KEYS
    for (int i = 0; i < 12; i++) {
        allKeys[i] = 1;  // Force all keys to be "pressed" in test mode
    }
    #endif
    return allKeys;
}

void scanKeysTask(void *pvParameters) {
    #ifndef TEST_SCAN_KEYS
    const TickType_t xFrequency = 50 / portTICK_PERIOD_MS;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    #endif
    static std::bitset<12> previousKeys;  // To detect transitions

    while (1) {
        #ifndef TEST_SCAN_KEYS
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        #endif
        std::bitset<12> localKeys = scanKeys();
        decodeKnob();
        
        // Detect key state changes and send a CAN message for each transition.
        for (int i = 0; i < 12; i++) {
            if (localKeys[i] != previousKeys[i]) {
                uint8_t TX_Message[8] = {0};
                // Use 'P' for press, 'R' for release.
                TX_Message[0] = localKeys[i] ? 'P' : 'R';
                #ifdef LEFT_PIANO
                TX_Message[1] = 4;  // Example octave number; adjust as needed.
                #endif
                #ifdef RIGHT_PIANO
                TX_Message[1] = 5;  // Example octave number; adjust as needed.
                #endif
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

        #ifdef TEST_SCAN_KEYS
        break;
        #endif
    }
}

void updateStepSizeFromKeys(const std::bitset<12>& keyStates) {
    uint32_t localStepSize = 0;
    xSemaphoreTake(sysMutex, portMAX_DELAY);
    for (int i = 0; i < 12; i++) {
        if (keyStates[i]) {
            localStepSize = stepSizes[i];
        }
    }
    xSemaphoreGive(sysMutex);
    __atomic_store_n(&currentStepSize, localStepSize, __ATOMIC_RELAXED);
}
