#include "display.hpp"
#include "system.hpp"
#include <Arduino.h>
#include <bitset>

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
        u8g2.setCursor(2, 20);
        u8g2.print(localKeys.to_ulong(), HEX);
        displayCurrentNote(localKeys);
        
        // Display knob rotation
        u8g2.setCursor(2, 30);
        u8g2.print("Knob:");
        u8g2.print(localKnob);
        
        // Display CAN status
        u8g2.setCursor(70, 10);
        u8g2.print("TX: ");
        u8g2.print(globalRXMessage[1]);

        u8g2.sendBuffer();
    
        digitalToggle(LED_BUILTIN);
    }
}