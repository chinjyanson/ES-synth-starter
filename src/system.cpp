#include "system.hpp"
#include "pindef.hpp"
#include <U8g2lib.h>
#include <STM32FreeRTOS.h>

uint8_t globalRXMessage[8] = {0};
SemaphoreHandle_t sysMutex;
QueueHandle_t msgInQ ;
QueueHandle_t msgOutQ;
SemaphoreHandle_t CAN_TX_Semaphore;
U8G2_SSD1305_128X32_ADAFRUIT_F_HW_I2C u8g2(U8G2_R0);
uint8_t prevKnobState = 0;

void setOutMuxBit(const uint8_t bitIdx, const bool value) {
    digitalWrite(REN_PIN, LOW);
    digitalWrite(RA0_PIN, (bitIdx & 0x01) ? HIGH : LOW);
    digitalWrite(RA1_PIN, (bitIdx & 0x02) ? HIGH : LOW);
    digitalWrite(RA2_PIN, (bitIdx & 0x04) ? HIGH : LOW);
    digitalWrite(OUT_PIN, value ? HIGH : LOW);
    digitalWrite(REN_PIN, HIGH);
    delayMicroseconds(2);
    digitalWrite(REN_PIN, LOW);
}

void setRow(uint8_t row) {
    digitalWrite(REN_PIN, LOW);
    digitalWrite(RA0_PIN, (row & 0x01) ? HIGH : LOW);
    digitalWrite(RA1_PIN, (row & 0x02) ? HIGH : LOW);
    digitalWrite(RA2_PIN, (row & 0x04) ? HIGH : LOW);
    digitalWrite(REN_PIN, HIGH);
}

std::bitset<4> readCols() {
    std::bitset<4> result;
    result[0] = !digitalRead(C0_PIN);
    result[1] = !digitalRead(C1_PIN);
    result[2] = !digitalRead(C2_PIN);
    result[3] = !digitalRead(C3_PIN);
    return result;
}

void initSystem() {
    u8g2.begin();
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(2, 10, "Keys:");

    sysMutex = xSemaphoreCreateMutex();
    msgInQ = xQueueCreate(36, 8);
    msgOutQ = xQueueCreate(36, 8);
    CAN_TX_Semaphore = xSemaphoreCreateCounting(3, 3);

    pinMode(RA0_PIN, OUTPUT); pinMode(RA1_PIN, OUTPUT); pinMode(RA2_PIN, OUTPUT);
    pinMode(REN_PIN, OUTPUT); pinMode(OUT_PIN, OUTPUT); pinMode(OUTL_PIN, OUTPUT);
    pinMode(OUTR_PIN, OUTPUT); pinMode(LED_BUILTIN, OUTPUT);
    pinMode(C0_PIN, INPUT); pinMode(C1_PIN, INPUT); pinMode(C2_PIN, INPUT); pinMode(C3_PIN, INPUT);
    pinMode(JOYX_PIN, INPUT); pinMode(JOYY_PIN, INPUT);

    setOutMuxBit(DRST_BIT, LOW);
    delayMicroseconds(2);
    setOutMuxBit(DRST_BIT, HIGH);
    setOutMuxBit(DEN_BIT, HIGH);

    sysState.mutex = xSemaphoreCreateMutex();
    sysState.knob3Rotation = 8;  // Initialize knob rotation to maximum (no attenuation)
}
