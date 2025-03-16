#include "knob.hpp"
#include "system.hpp"
#include "extension.hpp"
#include <STM32FreeRTOS.h>

#include <bitset>
#include <Arduino.h>

void decodeKnob() {
    static bool previouslyPressed = false;
    
    // First Check if there is rotation in the knob3 
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

    // Second Check if all 4 of the knobs are being pressed
    setRow(5);
    delayMicroseconds(3);
    std::bitset<4> row5Cols = readCols(); // Read the column states
    setRow(6);
    delayMicroseconds(3);
    std::bitset<4> row6Cols = readCols(); // Read the column states
    
    xSemaphoreTake(sysState.mutex, portMAX_DELAY);
    if (row5Cols[0] && row5Cols[1] && row6Cols[0] && row6Cols[1] && !previouslyPressed) {
        sysState.areAllKnobSPressed = !sysState.areAllKnobSPressed;
    }
    sysState.knob3Rotation = constrain(sysState.knob3Rotation + rotation, 0, 8);
    xSemaphoreGive(sysState.mutex);

    prevKnobState = currentState;
    previouslyPressed = row5Cols[0] && row5Cols[1] && row6Cols[0] && row6Cols[1];
}
