#include "can_bus.hpp"
#include "system.hpp"
#include "audio.hpp"
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
    while (1) {
        xQueueReceive(msgInQ, msgIn, portMAX_DELAY);
        noInterrupts();
        memcpy(globalRXMessage, msgIn, 8);
        interrupts();

        xSemaphoreTake(sysMutex, portMAX_DELAY);
        canRxSuccess = true;
        stepSizes = getArray();
        xSemaphoreGive(sysMutex);
    }
}

void CAN_TX_Task(void *pvParameters) {
    uint8_t msgOut[8];
    while (1) {
        // Block until a message is available on the transmit queue.
        xQueueReceive(msgOutQ, msgOut, portMAX_DELAY);
        // Wait for an available transmit mailbox.
        xSemaphoreTake(CAN_TX_Semaphore, portMAX_DELAY);
        CAN_TX(0x123, msgOut);
        if (CAN_TX(0x123, msgOut) == 0) {
            canTxSuccess = true;  // ✅ Message successfully sent
        }
    }
}

void initCAN() {
    CAN_Init(true);
    setCANFilter(0, 0);
    #ifndef DISABLE_CAN_RX_ISR
    CAN_RegisterRX_ISR(CAN_RX_ISR);
    #endif
    #ifndef DISABLE_CAN_TX_ISR
    CAN_RegisterTX_ISR(CAN_TX_ISR);
    #endif
    CAN_Start();
}