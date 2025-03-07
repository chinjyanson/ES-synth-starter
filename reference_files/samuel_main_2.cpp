#include <Arduino.h>
#include <U8g2lib.h>
#include <STM32FreeRTOS.h>
#include <iostream>
#include <ES_CAN.h>
#include <bitset>

// Constants
const uint32_t interval = 100; // Display update interval
volatile uint8_t keyArray[7];
volatile uint8_t finArray[12];
volatile uint8_t send_finArray[12]={1,1,1,1,1,1,1,1,1,1,1,1};
std::string keystrArray[7];
SemaphoreHandle_t keyArrayMutex; // a global handle for a FreeRTOS mutex that can be used by different threads to access the mutex object
SemaphoreHandle_t messageMutex;
SemaphoreHandle_t CAN_TX_Semaphore; // transmit thread will use a semaphore to check when it can place a message in the outgoing mailbox
volatile int32_t knob3Rotation = 4;
volatile int32_t knob2Rotation = 4;
volatile int32_t knob1Rotation = 1; 
volatile int32_t send_octave = 4;
volatile bool master = false; //change this boolean to set to sender or receiver
volatile uint8_t Message[8] = {0};
uint8_t RX_Message[8] = {0};
// volatile uint8_t note;
QueueHandle_t msgInQ;  // incomming queue from CAN hardware then put in decode thread
QueueHandle_t msgOutQ; // outgoing queue from any thread that wants to send a CAN message
const int NOTE_POSITIONS[12] = {60, 64, 68, 72, 76, 80, 84, 88, 92, 96, 100, 104};
const int y_POSITIONS[12] = {10, 10, 9, 9, 8, 7, 7, 6, 6, 5, 5, 4};


// Pin definitions
// Row select and enable
const int RA0_PIN = D3;
const int RA1_PIN = D6;
const int RA2_PIN = D12;
const int REN_PIN = A5;

// Matrix input and output
const int C0_PIN = A2;
const int C1_PIN = D9;
const int C2_PIN = A6;
const int C3_PIN = D1;
const int OUT_PIN = D11;

// Audio analogue out
const int OUTL_PIN = A4;
const int OUTR_PIN = A3;

// Joystick analogue in
const int JOYY_PIN = A0;
const int JOYX_PIN = A1;

// Output multiplexer bits
const int DEN_BIT = 3;
const int DRST_BIT = 4;
const int HKOW_BIT = 5;
const int HKOE_BIT = 6;

// Display driver object
U8G2_SSD1305_128X32_NONAME_F_HW_I2C u8g2(U8G2_R0);

// Function to set outputs using key matrix
void setOutMuxBit(const uint8_t bitIdx, const bool value)
{
  digitalWrite(REN_PIN, LOW);
  digitalWrite(RA0_PIN, bitIdx & 0x01);
  digitalWrite(RA1_PIN, bitIdx & 0x02);
  digitalWrite(RA2_PIN, bitIdx & 0x04);
  digitalWrite(OUT_PIN, value);
  digitalWrite(REN_PIN, HIGH);
  delayMicroseconds(2);
  digitalWrite(REN_PIN, LOW);
}

uint8_t readCols()
{

  int C0 = digitalRead(C0_PIN);
  int C1 = digitalRead(C1_PIN);
  int C2 = digitalRead(C2_PIN);
  int C3 = digitalRead(C3_PIN);

  uint8_t res = 0;
  res |= (C0 << 3);
  res |= (C1 << 2);
  res |= (C2 << 1);
  res |= C3;

  return res;
}

int32_t fss(int frequency)
{
  int32_t step = pow(2, 32) * frequency / 22000;
  return step;
}

void setRow(uint8_t rowIdx)
{
  digitalWrite(REN_PIN, LOW);
  digitalWrite(RA0_PIN, bitRead(rowIdx, 0));
  digitalWrite(RA1_PIN, bitRead(rowIdx, 1));
  digitalWrite(RA2_PIN, bitRead(rowIdx, 2));
  digitalWrite(REN_PIN, HIGH);
}

int mapindex(uint8_t keypress, int rowindex)
{

  int index;

  if (keypress == 7)
  {
    index = 0 + rowindex * 4;
  }
  else if (keypress == 11)
  {
    index = 1 + rowindex * 4;
  }
  else if (keypress == 13)
  {
    index = 2 + rowindex * 4;
  }
  else if (keypress == 14)
  {
    index = 3 + rowindex * 4;
  }
  else
  {
    index = 12;
  }
  return index;
}
class Knob
{
public:
  uint8_t knob;
  uint8_t preknob;
  uint8_t knob_val;
  bool increment;
  int32_t rotation_var = 4;
  int32_t rotation_var_slave = 1;


  int32_t detectknob3rot()
  {

    uint8_t knob = 0;
    knob |= (bitRead(knob_val, 2) << 1);
    knob |= bitRead(knob_val, 3);

    if ((knob == 01 && preknob == 00) || (knob == 10 && preknob == 11))
    {
      rotation_var = rotation_var + 1;
      increment = true;
    }
    else if ((knob == 00 && preknob == 01) || (knob == 11 && preknob == 10))
    {
      rotation_var = rotation_var - 1;
      increment = false;
    }
    else if ((knob == 11 && preknob == 00) || (knob == 10 && preknob == 01) || (knob == 01 && preknob == 10) || (knob == 00 && preknob == 11))
    {
      if (increment == true)
      {
        rotation_var = rotation_var + 1;
        increment = true;
      }
      else
      {
        rotation_var = rotation_var - 1;
        increment = false;
      }
    }
    preknob = knob;
    return rotation_var = constrain(rotation_var, 0, 8);
  }
  int32_t detectknob2rot()
  {

    uint8_t knob = 0;
    knob |= (bitRead(knob_val, 0) << 1);
    knob |= bitRead(knob_val, 1);

    if ((knob == 01 && preknob == 00) || (knob == 10 && preknob == 11))
    {
      rotation_var = rotation_var + 1;
      increment = true;
    }
    else if ((knob == 00 && preknob == 01) || (knob == 11 && preknob == 10))
    {
      rotation_var = rotation_var - 1;
      increment = false;
    }
    else if ((knob == 11 && preknob == 00) || (knob == 10 && preknob == 01) || (knob == 01 && preknob == 10) || (knob == 00 && preknob == 11))
    {
      if (increment == true)
      {
        rotation_var = rotation_var + 1;
        increment = true;
      }
      else
      {
        rotation_var = rotation_var - 1;
        increment = false;
      }
    }
    preknob = knob;
    return rotation_var = constrain(rotation_var, 0, 8);
  }
  int32_t detectknob1rot()
  {

    uint8_t knob = 0;
    knob |= (bitRead(knob_val, 2) << 1);
    knob |= bitRead(knob_val, 3);

    if ((knob == 01 && preknob == 00) || (knob == 10 && preknob == 11))
    {
      rotation_var_slave = rotation_var_slave + 1;
      increment = true;
    }
    else if ((knob == 00 && preknob == 01) || (knob == 11 && preknob == 10))
    {
      rotation_var_slave = rotation_var_slave - 1;
      increment = false;
    }
    else if ((knob == 11 && preknob == 00) || (knob == 10 && preknob == 01) || (knob == 01 && preknob == 10) || (knob == 00 && preknob == 11))
    {
      if (increment == true)
      {
        rotation_var_slave = rotation_var_slave + 1;
        increment = true;
      }
      else
      {
        rotation_var_slave = rotation_var_slave - 1;
        increment = false;
      }
    }
    preknob = knob;
    rotation_var_slave = constrain(rotation_var_slave, 0, 1);
    if(rotation_var_slave==0){
      master = false;
    }
    else{
      master = true;
    }
    return rotation_var_slave;
  }
};


// Function to draw a bar for the volume
void drawVolume(int volume) {
  u8g2.setCursor(10,7);
  u8g2.print("Vol:");
  u8g2.drawFrame(10, 10, 30, 8); // Draw a border around the bar
  u8g2.drawBox(10, 10, volume * 3.9, 8); // Draw the bar with a width proportional to the volume
}

// Function to draw the octave as a number to the power of 8
void drawOctave(int octave) {
  u8g2.setFont(u8g2_font_profont12_tf); // Use a small font
  u8g2.setCursor(10, 30); // Position the cursor
  u8g2.print("octave:"); // Print the base
  u8g2.print(octave); // Print the exponent
}

// Function to draw a note on the staff
void drawNote(int note) {
  int x = NOTE_POSITIONS[note]; // Get the x position of the note
  int y = y_POSITIONS[note];
  char notes[12]={'C','C','D','D','E','F','F','G','G','A','A','B'};
  u8g2.drawCircle(x, y, 1.5); // Draw a circle at the note position
  u8g2.setCursor(x,20);
  u8g2.print(notes[note]);
  if(note==1||note==3||note==6||note==8||note==10){
    u8g2.print('#');
  }
}

// Function to draw the staff
void drawStaff() {
  for (int i = 0; i < 5; i++) {
    int y = 0 + i * 2; // Calculate the y position of the staff line
    u8g2.drawLine(60, y, 110, y); // Draw a line across the display
  }
 // u8g2.drawLine(60, 10, 110, 10); // Draw a line for the bottom of the staff
}

void sampleISR() //interupt
{
  static uint32_t phaseAcc[24] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  const int32_t keys[13] = {fss(261.63), fss(277.18), fss(293.66), fss(311.13), fss(329.63), fss(349.23), fss(369.99), fss(392), fss(415.3), fss(440), fss(466.16), fss(493.88), 0};
  int32_t Vfin = 0;
  int temp;
  int octave;
  for (int i=0; i<24; i++){
    int note;
      if(i<12){
        note = i;
         temp = finArray[i];
         octave = knob2Rotation;}
      else{
        note = i-12;
        temp = send_finArray[i-12];
        octave = send_octave;
      }
      if(temp==0){
        int32_t currStepsize = keys[note];
        if(octave>4){
          currStepsize = currStepsize << (octave-4);
        }
        else if(octave<4){
          currStepsize = currStepsize >> (4-octave);
        } 
        phaseAcc[i] += currStepsize; //sawtooth
      }
      int32_t Vout = (phaseAcc[i] >> 24) - 128;
      Vout = Vout >> (11 - knob3Rotation);
      Vfin += Vout;
    }

  if(master){
  analogWrite(OUTR_PIN, Vfin + 128);}
  // time += 1;
}



void scanKeysTask(void *pvParameters)
{
  const TickType_t xFrequency = 20 / portTICK_PERIOD_MS;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  uint8_t prestate2;
  uint8_t prestate3;
  uint8_t prestate1;

  Knob knob3;
  knob3.preknob = prestate3;
  knob3.increment = false;

  Knob knob2;
  knob2.preknob = prestate2;
  knob2.increment = false;

  Knob knob1;
  knob1.preknob = prestate1;
  knob1.increment = false;

  bool flag = false;
  bool set_master;
  uint8_t local_TX_Message[8] = {0};
  uint8_t send_TX_Message[8] = {0};
  while (1)
  {
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
    const int32_t keys[13] = {fss(261.63), fss(277.18), fss(293.66), fss(311.13), fss(329.63), fss(349.23), fss(369.99), fss(392), fss(415.3), fss(440), fss(466.16), fss(493.88), 0};

    int32_t stepSizes;
    int32_t prestepSizes;
    int32_t localknob3;
    int32_t localknob2;
    int32_t localknob1;
    bool press_key;
    press_key = false;
    int preindex;
    // uint8_t TX_Message[8] = {0};

    for (int i = 0; i < 5; i++)
    {
      uint8_t rowindex = i;
      setRow(rowindex);
      delayMicroseconds(3);
      keyArray[i] = readCols();
      // std::bitset<4> KeyBits(keys);
      // std::string keystr = KeyBits.to_string();
      // // change the name of this
      // keystrArray[i] = keystr;
    }

    // getting finArray
    xSemaphoreTake(keyArrayMutex, portMAX_DELAY);
    uint32_t temp1 = keyArray[0];
    uint32_t temp2 = keyArray[1];
    uint32_t temp3 = keyArray[2];
    xSemaphoreGive(keyArrayMutex);
    uint32_t tmp = temp3;
    for (int i = 0; i < 12; i++)
    {
      if (i == 4)
      {
        tmp = temp2;
      }
      else if (i == 8)
      {
        tmp = temp1;
      }
      finArray[11 - i] = tmp % 2;
      tmp = tmp >> 1;
    }
    

    for (int i = 0; i < 5; i++)
    {
      xSemaphoreTake(keyArrayMutex, portMAX_DELAY);
      uint8_t keypresse = keyArray[i];
      int32_t octave = knob2Rotation;
      xSemaphoreGive(keyArrayMutex);
      if (mapindex(keypresse, i) != 12 && press_key == false)
      {
        stepSizes = keys[mapindex(keypresse, i)];
        press_key = true;
      }
      else if (mapindex(keypresse, i) == 12)
      {
        press_key = false;
      }
      if (keyArray[0] == 15 && keyArray[1] == 15 && keyArray[2] == 15)
      {
        press_key = false;
        stepSizes = 0;
      }

        if(!master){
          if ((prestepSizes != stepSizes) && press_key == true)
          {
            send_TX_Message[0] = 'P';
            send_TX_Message[2] = mapindex(keypresse, i);
            preindex = mapindex(keypresse, i);
          }
          if ((prestepSizes != stepSizes) && press_key == false)
          {
            send_TX_Message[0] = 'R';
            send_TX_Message[2] = preindex;
          }
          prestepSizes = stepSizes;
          
          send_TX_Message[1] = octave;
          send_TX_Message[3] = temp1;
          send_TX_Message[4] = temp2;
          send_TX_Message[5] = temp3;
        }
        
      if (i == 3)
      {
        xSemaphoreTake(keyArrayMutex, portMAX_DELAY);
        knob3.knob_val = keyArray[i];
        knob2.knob_val = keyArray[i];
        if(master){
          localknob3 = knob3.detectknob3rot();
          prestate3 = knob3.preknob;
        }
        else{
          localknob3 = 0;
        }
        localknob2 = knob2.detectknob2rot();
        prestate2 = knob2.preknob;
        xSemaphoreGive(keyArrayMutex);
        __atomic_store_n(&knob3Rotation, localknob3, __ATOMIC_RELAXED);
        __atomic_store_n(&knob2Rotation, localknob2, __ATOMIC_RELAXED);
      }
      if (i == 4)
      {
        
        xSemaphoreTake(keyArrayMutex, portMAX_DELAY);
        knob1.knob_val = keyArray[i];
        localknob1 = knob1.detectknob1rot();
        prestate1 = knob1.preknob;

        xSemaphoreGive(keyArrayMutex);
        __atomic_store_n(&knob1Rotation, localknob1, __ATOMIC_RELAXED);
      }
    }

    // send the message over the bus using the CAN library
    // CAN_TX(0x123, TX_Message);
    // if you send a lot of messages at the same time, scanKeysTask()
    // might get stuck waiting for the bus to become available.
    // It also not a thread safe function and its behaviour could be undefined
    // if messages are being sent from two different threads
    if(!master){
       xQueueSend(msgOutQ, send_TX_Message, portMAX_DELAY);
    }
    // messages can be queued up for transmission
    // queue allows multiple threads to send messages
    // because the function for putting a message on the queue is thread-safe
    /*
     for (int i = 0; i < 8; i++)
    {
      xSemaphoreTake(keyArrayMutex, portMAX_DELAY);
      // Copy the contents of the local array to the global array
      TX_Message[i] = local_TX_Message[i];
      // Release the mutex
      xSemaphoreGive(keyArrayMutex);
    }
    */
  }
}



void displayUpdateTask(void *pvParameters)
{

  const TickType_t xFrequency = 100 / portTICK_PERIOD_MS;
  TickType_t xLastWakeTime = xTaskGetTickCount();

  while (1)
  {
    vTaskDelayUntil(&xLastWakeTime, xFrequency);

    bool press_key;
    press_key = false;
    uint32_t ID;

    u8g2.clearBuffer();                 // clear the internal memory
    u8g2.setFont(u8g2_font_ncenB08_tr); // choose a suitable font

    for (int i = 0; i < 3; i++)
    {
      xSemaphoreTake(keyArrayMutex, portMAX_DELAY);
      uint8_t keypresse = keyArray[i];
      int32_t vol = knob3Rotation;
      int32_t oct = knob2Rotation;
      uint8_t index = Message[1];

      u8g2.setCursor(60, 30);
      if(!master){
        u8g2.print("sender");
      }
      else{
        u8g2.print("receiver");
      }
    
      xSemaphoreGive(keyArrayMutex);
      drawVolume(vol);
      drawOctave(oct);
      drawStaff();
      
      for(int i=0;i<12;i++){
        if(finArray[i]==0){
          drawNote(i);
        }
      }
      

    }

    // this method uses polling
    /*
    while (CAN_CheckRXLevel())
    */
    if(master){

      // print RX_Message
      xSemaphoreTake(messageMutex, portMAX_DELAY);
      // Copy the contents of the local array to the global array
      u8g2.setCursor(40, 10);
      u8g2.print((char)RX_Message[0]);
      u8g2.print(RX_Message[1]);
      u8g2.print(RX_Message[2]);

      
      // Release the mutex
      xSemaphoreGive(messageMutex);
    }

    u8g2.sendBuffer();

    // Toggle LED
    digitalToggle(LED_BUILTIN);
  }
}


// write incoming message into the queue in an ISR
void CAN_RX_ISR(void)
{
  uint8_t RX_Message_ISR[8];
  uint32_t ID;
  uint8_t local_RX_Message[8];
  CAN_RX(ID, RX_Message_ISR);
  xQueueSendFromISR(msgInQ, RX_Message_ISR, NULL); // place data in the queue
}

// decode thread to process messages on the queue
void decodeText(void *pvParameters)
{
  uint8_t local_RX_Message[8] = {0};
  while (1)
  {
    xQueueReceive(msgInQ, local_RX_Message, portMAX_DELAY); // initiate the thread by the availability of data on the queue
    // QueueReceive will block and yield the CPU to other tasks until a message is available in the queue
    //  Take the mutex to protect access to the global RX_Message array
    for (int i = 0; i < 8; i++)
    {
      xSemaphoreTake(messageMutex, portMAX_DELAY);
      // Copy the contents of the local array to the global array
      RX_Message[i] = local_RX_Message[i];
      // Release the mutex
      xSemaphoreGive(messageMutex);
    }
    const int32_t keys[13] = {fss(261.63), fss(277.18), fss(293.66), fss(311.13), fss(329.63), fss(349.23), fss(369.99), fss(392), fss(415.3), fss(440), fss(466.16), fss(493.88), 0};
    int32_t convert_stepSizes;
      uint32_t temp1 = RX_Message[3];
      uint32_t temp2 = RX_Message[4];
      uint32_t temp3 = RX_Message[5];
      
      uint32_t tmp = temp3;
      for (int i = 0; i < 12; i++)
      {
        if (i == 4)
        {
          tmp = temp2;
        }
        else if (i == 8)
        {
          tmp = temp1;
        }
        send_finArray[11 - i] = tmp % 2;
        tmp = tmp >> 1;
      }
     

    //__atomic_store_n(&send_finArray, tmp_finArray, __ATOMIC_RELAXED);
    __atomic_store_n(&send_octave, RX_Message[1], __ATOMIC_RELAXED);
  }
}

void CAN_TX_Task (void * pvParameters) {
  uint8_t msgOut[8];
  while (1) {
    xQueueReceive(msgOutQ, msgOut, portMAX_DELAY);
    xSemaphoreTake(CAN_TX_Semaphore, portMAX_DELAY);
    CAN_TX(0x123, msgOut);
  }
}

void CAN_TX_ISR (void) {
  xSemaphoreGiveFromISR(CAN_TX_Semaphore, NULL);
}

void setup()
{
  // put your setup code here, to run once:

  // Set pin directions
  pinMode(RA0_PIN, OUTPUT);
  pinMode(RA1_PIN, OUTPUT);
  pinMode(RA2_PIN, OUTPUT);
  pinMode(REN_PIN, OUTPUT);
  pinMode(OUT_PIN, OUTPUT);
  pinMode(OUTL_PIN, OUTPUT);
  pinMode(OUTR_PIN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  pinMode(C0_PIN, INPUT);
  pinMode(C1_PIN, INPUT);
  pinMode(C2_PIN, INPUT);
  pinMode(C3_PIN, INPUT);
  pinMode(JOYX_PIN, INPUT);
  pinMode(JOYY_PIN, INPUT);

  // Initialise display
  setOutMuxBit(DRST_BIT, LOW); // Assert display logic reset
  delayMicroseconds(2);
  setOutMuxBit(DRST_BIT, HIGH); // Release display logic reset
  u8g2.begin();
  setOutMuxBit(DEN_BIT, HIGH); // Enable display power supply

  // Initialise UART
  Serial.begin(9600);
  Serial.println("Hello World");

  // Create the mutex and assign its handle
  keyArrayMutex = xSemaphoreCreateMutex();
  messageMutex = xSemaphoreCreateMutex();
  //  initialise queue handler
  msgInQ = xQueueCreate(36, 8);
  CAN_TX_Semaphore = xSemaphoreCreateCounting(3,3);//The STM32 CAN hardware has three mailbox slots for outgoing messages
  msgOutQ = xQueueCreate(36, 8);


  // initialise CAN
  CAN_Init(false);                // place CAN haredware in loopback mode it will receive and acknoledge its own message
  CAN_RegisterRX_ISR(CAN_RX_ISR); // passe a pointer to the relevant library function to set the ISR to be called whenever a CAN message is received
  CAN_RegisterTX_ISR(CAN_TX_ISR);

  setCANFilter(0x123, 0x7ff);     // initialises the reception ID filter
  CAN_Start();

  // creat a timer
  
  TIM_TypeDef *Instance = TIM1;
  HardwareTimer *sampleTimer = new HardwareTimer(Instance);
  sampleTimer->setOverflow(22000, HERTZ_FORMAT);
  sampleTimer->attachInterrupt(sampleISR);
  sampleTimer->resume();


  TaskHandle_t scanKeysHandle = NULL; // 20ms
  xTaskCreate(
      scanKeysTask,     /* Function that implements the task */
      "scanKeys",       /* Text name for the task */
      64,               /* Stack size in words, not bytes */
      NULL,             /* Parameter passed into the task */
      4,                /* Task priority */
      &scanKeysHandle); /* Pointer to store the task handle */

  TaskHandle_t displayHandle = NULL; // 100ms
  xTaskCreate(
      displayUpdateTask,   /* Function that implements the task */
      "displayupdatetask", /* Text name for the task */
      256,                 /* Stack size in words, not bytes */
      NULL,                /* Parameter passed into the task */
      1,                   /* Task priority */
      &displayHandle);     /* Pointer to store the task handle */

  
  TaskHandle_t decodeHandle = NULL; // 25ms
  xTaskCreate(
      decodeText,     /* Function that implements the task */
      "decodeText",   /* Text name for the task */
      256,            /* Stack size in words, not bytes */
      NULL,           /* Parameter passed into the task */
      3,              /* Task priority */
      &decodeHandle); /* Pointer to store the task handle */

  
  TaskHandle_t CAN_TXHandle = NULL;//60ms
  xTaskCreate(
      CAN_TX_Task,     // Function that implements the task
      "CAN_TX_Task",       // Text name for the task
      64,               //Stack size in words, not bytes
      NULL,             //Parameter passed into the task
      2,                // Task priority
      &CAN_TXHandle); // Pointer to store the task handle

                  
  vTaskStartScheduler();
}

void loop(){}