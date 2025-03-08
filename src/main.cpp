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
    Serial.println("Intializing System...");
    initSystem();
    initCAN();

    sampleTimer.setOverflow(22000, HERTZ_FORMAT);
    #ifndef DISABLE_SAMPLE_ISR
    sampleTimer.attachInterrupt(sampleISR);
    #endif
    sampleTimer.resume();
    
    #ifndef DISABLE_THREADS
    xTaskCreate(scanKeysTask, "scanKeys", 128, NULL, 2, NULL);
    xTaskCreate(displayUpdateTask, "displayUpdate", 256, NULL, 1, NULL);
    xTaskCreate(CAN_TX_Task, "CAN_TX", 128, NULL, 3, NULL);
    xTaskCreate(CAN_RX_Task, "CAN_RX", 128, NULL, 3, NULL);
    #endif
    vTaskStartScheduler();
}

void loop() {}  // FreeRTOS handles all tasks
