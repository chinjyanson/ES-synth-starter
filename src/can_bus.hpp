#ifndef CAN_BUS_HPP
#define CAN_BUS_HPP

void CAN_TX_Task(void *pvParameters);
void CAN_RX_Task(void *pvParameters);
void CAN_TX_ISR();
void CAN_RX_ISR();
void initCAN();  // Initialize CAN bus

#endif // CAN_BUS_HPP
