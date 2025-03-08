#include "knob.hpp"
#include "system.hpp"

#include <bitset>
#include <Arduino.h>

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
