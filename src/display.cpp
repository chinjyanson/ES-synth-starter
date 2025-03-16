#include "config.hpp"
#include "display.hpp"
#include "system.hpp"
#include <Arduino.h>
#include <bitset>
#include <string>

U8G2_SSD1305_128X32_ADAFRUIT_F_HW_I2C u8g2(U8G2_R0);

bool waiting_for_user = false;
bool playing_music = false;
bool correct_guess = false;
std::string correct_answer = "";

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
    u8g2.begin();
    delayMicroseconds(10);
    const TickType_t xFrequency = 30 / portTICK_PERIOD_MS;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    while (1) {
        #ifndef TEST_DISPLAY
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        #endif

        xSemaphoreTake(sysState.mutex, portMAX_DELAY);
        std::bitset<12> localKeys = sysState.keyStates;
        bool isGame = sysState.areAllKnobSPressed;
        int localKnob = sysState.knob3Rotation;
        bool gameOverride = sysState.gameActiveOverride;
        xSemaphoreGive(sysState.mutex);
        
        u8g2.clearBuffer();

        if (!isGame){
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
            u8g2.print(canTxSuccess ? "OK" : "Fail");

            u8g2.setCursor(70, 20);
            u8g2.print("RX: ");
            u8g2.print(canRxSuccess ? "OK" : "Fail");

            u8g2.sendBuffer();
        
            digitalToggle(LED_BUILTIN);

        } else if (!isGame) {
            u8g2.setFont(u8g2_font_ncenB08_tr);
            u8g2.drawStr(4, 10, "Welcome to our");
            u8g2.drawStr(4, 20, "Hidden Game!");
            u8g2.sendBuffer();

            digitalToggle(LED_BUILTIN);
        } else if (waiting_for_user){
            u8g2.setFont(u8g2_font_ncenB08_tr);
            u8g2.drawStr(4, 10, "Please make ");
            u8g2.drawStr(4, 20, "your guess");
            u8g2.sendBuffer();

            digitalToggle(LED_BUILTIN);
        } else if (playing_music){
            u8g2.clearBuffer();
            u8g2.setFont(u8g2_font_ncenB08_tr);
            u8g2.drawStr(4, 10, "Playing music");
            u8g2.sendBuffer();

            digitalToggle(LED_BUILTIN);
        } else if (correct_guess) {
            u8g2.clearBuffer();
            u8g2.setFont(u8g2_font_ncenB08_tr);
            u8g2.drawStr(4, 10, "Correct Guess");
            u8g2.sendBuffer();

            digitalToggle(LED_BUILTIN);
        } else if (!correct_guess){
            u8g2.clearBuffer();
            u8g2.setFont(u8g2_font_ncenB08_tr);
            u8g2.drawStr(4, 10, "Wrong Guess");
            u8g2.drawStr(4, 20, "Correct Guess was");
            u8g2.drawStr(4, 30, correct_answer.c_str());
            u8g2.sendBuffer();

            digitalToggle(LED_BUILTIN);
        } else {
            u8g2.clearBuffer();
            u8g2.setFont(u8g2_font_ncenB08_tr);
            u8g2.drawStr(4, 10, "how are you here");
            u8g2.sendBuffer();

            digitalToggle(LED_BUILTIN);
        }

        #ifdef TEST_DISPLAY
        break;
        #endif
    }
}