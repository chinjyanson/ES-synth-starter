#include "audio.hpp"
#include "system.hpp"
#include "pindef.hpp"
#include "system.hpp"
#include <Arduino.h>

volatile uint32_t currentStepSize = 0;
std::array<uint32_t, 12> stepSizes = getArray();

std::array<uint32_t, 12> getArray() {
    std::array<uint32_t, 12> result = {0};
    xSemaphoreTake(sysMutex, portMAX_DELAY);
    int cur_octave = globalRXMessage[1] != 0 ? globalRXMessage[1] : 4;
    xSemaphoreGive(sysMutex);
    double freq_factor = pow(2, 1.0/12.0);
    
    for (size_t i = 0; i < 12; i++) {
        double freq = (i >= 9) ? 440 * pow(freq_factor, i - 9) : 440 / pow(freq_factor, 9 - i);
        result[i] = ((1 << (cur_octave - 4)) * (pow(2, 32) * freq)) / 22000;
    }
    return result;
}

void sampleISR() {
    // Serial.print("sample ISR is running");
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