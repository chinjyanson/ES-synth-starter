#ifndef SYSTEM_HPP
#define SYSTEM_HPP
#include <Arduino.h>
#include <U8g2lib.h>
#include <bitset>

#include <bitset>
#include <STM32FreeRTOS.h>

// Global System State
extern uint8_t globalRXMessage[8];  
extern SemaphoreHandle_t sysMutex;

// Queues and Semaphores
extern QueueHandle_t msgInQ;
extern QueueHandle_t msgOutQ;
extern SemaphoreHandle_t CAN_TX_Semaphore;

extern uint8_t prevKnobState;

void initSystem();  // Function to initialize all system components
void setRow(uint8_t row);
std::bitset<4> readCols();

struct SystemState {
    std::bitset<32> inputs;
    SemaphoreHandle_t mutex;
    int knob3Rotation;
    std::bitset<12> keyStates;
};

extern SystemState sysState;

extern bool canTxSuccess;
extern bool canRxSuccess;

#endif // SYSTEM_HPP
