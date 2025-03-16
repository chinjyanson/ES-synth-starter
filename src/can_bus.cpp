#include "can_bus.hpp"
#include "system.hpp"
#include "audio.hpp"
#include "config.hpp"
#include <ES_CAN.h>
#include <Arduino.h>

// CAN RX ISR: Reads incoming CAN message and enqueues it.
void CAN_RX_ISR(void) {
    uint8_t RX_Message_ISR[8];
    uint32_t id;
    CAN_RX(id, RX_Message_ISR);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(msgInQ, RX_Message_ISR, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// CAN TX ISR: Releases a transmit mailbox slot.
void CAN_TX_ISR(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(CAN_TX_Semaphore, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void CAN_RX_Task(void *pvParameters) {
    uint8_t msgIn[8];
    uint32_t id;
    
    #ifdef TEST_CAN_RX
    // In test mode, simulate receiving CAN messages
    uint8_t simulatedMessage[8] = {0};  // Simulated message (can be filled with test data)
    simulatedMessage[0] = 1;  // Example test data
    simulatedMessage[1] = 2;  // Example test data
    // You can fill the rest with any values to simulate a message.
    memcpy(msgIn, simulatedMessage, 8);  // Copy the simulated message
    #endif
    
    while (1) {
        #ifndef TEST_CAN_RX
        // In normal operation, receive a message from the CAN queue
        xQueueReceive(msgInQ, msgIn, portMAX_DELAY);
        #endif
        
        noInterrupts();
        memcpy(globalRXMessage, msgIn, 8);
        interrupts();

        xSemaphoreTake(sysMutex, portMAX_DELAY);
        canRxSuccess = true;
        stepSizes = getArray();
        xSemaphoreGive(sysMutex);

        #ifdef TEST_CAN_RX
        // Optionally, print or log the message if in test mode
        Serial.print("Simulated CAN RX Message: ");
        for (int i = 0; i < 8; i++) {
            Serial.print(simulatedMessage[i], HEX);
            Serial.print(" ");
        }
        Serial.println();
        #endif
    }
}

// NOT SURE HOW TO TEST THIS FUNCTION
void CAN_TX_Task(void *pvParameters) {
    uint8_t msgOut[8];

    while (1) {
        #ifdef TEST_CAN_TX
        // In test mode, simulate a CAN message
        uint8_t simulatedMessage[8] = {0};  // Simulated CAN message
        simulatedMessage[0] = 1;  // Example test data
        simulatedMessage[1] = 2;  // Example test data
        // Fill in the rest with values you want to test with
        memcpy(msgOut, simulatedMessage, 8);  // Copy simulated message into msgOut
        #else
        // In normal operation, block until a message is available on the transmit queue
        xQueueReceive(msgOutQ, msgOut, portMAX_DELAY);
        #endif

        // Wait for an available transmit mailbox (or simulate CAN transmission in test mode)
        xSemaphoreTake(CAN_TX_Semaphore, portMAX_DELAY);

        #ifdef TEST_CAN_TX
        // In test mode, simulate the CAN transmission
        Serial.print("Simulated CAN TX Message: ");
        for (int i = 0; i < 8; i++) {
            Serial.print(msgOut[i], HEX);
            Serial.print(" ");
        }
        Serial.println();
        canTxSuccess = true;  // Message successfully "sent" in test mode
        #else
        // In normal operation, send the CAN message
        if (CAN_TX(0x123, msgOut) == 0) {
            canTxSuccess = true;  // Message successfully sent
        }
        #endif
    }
}

void initCAN() {
    #ifdef SINGLE_PIANO
        CAN_Init(true);
    #endif
    #ifdef DUAL_PIANO
        CAN_Init(false);
    #endif
    setCANFilter(0, 0);
    #ifndef DISABLE_CAN_RX_ISR
    CAN_RegisterRX_ISR(CAN_RX_ISR);
    #endif
    #ifndef DISABLE_CAN_TX_ISR
    CAN_RegisterTX_ISR(CAN_TX_ISR);
    #endif
    CAN_Start();
}