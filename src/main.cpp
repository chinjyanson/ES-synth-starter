#include <Arduino.h>
#include <U8g2lib.h>
#include <bitset>
#include <STM32FreeRTOS.h>
#include <ES_CAN.h>  // Provided CAN library

#include "pindef.hpp"
#include "system.hpp"
#include "can_bus.hpp"
#include "keys.hpp"
#include "audio.hpp"
#include "display.hpp"
#include "knob.hpp"
#include "config.hpp"

HardwareTimer sampleTimer(TIM1);

void setup() {
    Serial.begin(9600);
    Serial.println("Initializing System...");
    initSystem();
    initCAN();

    sampleTimer.setOverflow(22000, HERTZ_FORMAT);
    #ifndef DISABLE_SAMPLE_ISR
    sampleTimer.attachInterrupt(sampleISR);
    #endif
    sampleTimer.resume();

    // Test display update timing
    #ifdef TEST_DISPLAY
    float startTime = micros();  // Start timing
    Serial.println("Testing Display Update Time...");
    for (int iter = 0; iter < 32; iter++) {
        displayUpdateTask(NULL);  // Call the display update task to measure its execution time
    }
    float final_time = micros() - startTime;  // Calculate total time
    Serial.print("Worst Case Time for Display Update (ms): ");
    Serial.println(final_time / 32000);  // Print the average time per update
    while(1);
    #endif

    #ifdef TEST_SCAN_KEYS
    float startTime = micros();
    for (int iter = 0; iter < 32; iter++) {
    scanKeysTask(NULL);
    }
    float final_time = micros() - startTime;
    Serial.print("Worst Case Time for ScanKeys (ms): ");
    Serial.println(final_time/32000);
    while(1);
    #endif

    #ifdef TEST_ISR
    float startTime = micros();
    for (int iter = 0; iter < 32; iter++) {
      sampleISR();
    }
    float final_time = micros() - startTime;
    Serial.print("Worst Case Time for ISR (ms): ");
    Serial.println(final_time/32000);
    while(1);
    #endif

    #ifdef TEST_CAN_TX
    float startTime = micros();
    for (int iter = 0; iter < 32; iter++) {
      CAN_TX_Task(NULL);
    }
    float final_time = micros() - startTime;
    Serial.print("Worst Case Time for CAN_TX (ms): ");
    Serial.println(final_time/32000);
    while(1);
    #endif

    #ifdef TEST_CAN_RX
    float startTime = micros();
    for (int iter = 0; iter < 32; iter++) {
      CAN_RX_Task(NULL);
    }
    float final_time = micros() - startTime;
    Serial.print("Worst Case Time for CAN_RX (ms): ");
    Serial.println(final_time/32000);
    while(1);
    #endif



    #ifndef DISABLE_THREADS
    xTaskCreate(scanKeysTask, "scanKeys", 128, NULL, 2, NULL);
    xTaskCreate(displayUpdateTask, "displayUpdate", 256, NULL, 1, NULL);
    xTaskCreate(CAN_TX_Task, "CAN_TX", 128, NULL, 3, NULL);
    xTaskCreate(CAN_RX_Task, "CAN_RX", 128, NULL, 3, NULL);
    #endif

    vTaskStartScheduler();
}

void loop() {}  // FreeRTOS handles all tasks
