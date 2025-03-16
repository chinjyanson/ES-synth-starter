#include "extension.hpp"
#include <Arduino.h>
#include "audio.hpp"
#include "system.hpp"
#include <STM32FreeRTOS.h>
#include <random> 


// int getRandomNote(){
//     // Available notes will be of one keyboard left or right
//     std::array<uint32_t, 12> randomNotes = getArray();

//     std::random_device rd;
//     std::mt19937 gen(rd());
//     std::uniform_int_distribution<size_t> dist(0, randomNotes.size() - 1); // Range [0, 11]

//     uint32_t randomNote = randomNotes[dist(gen)]; // get the random note 
//     return randomNote;
// }

// void playRandomNote(){
//     uint32_t randomNote = getRandomNote();
//     __atomic_store_n(&currentStepSize, randomNote, __ATOMIC_RELAXED);
// }



void gameTask(void *pvParameters) {
    std::array<uint32_t, 12> randomNotes = getArray();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dist(0, randomNotes.size() - 1);

    while (true) {
        xSemaphoreTake(sysState.mutex, portMAX_DELAY);
        bool gameActive = sysState.areAllKnobSPressed;
        if (gameActive) {
            
        }
        // Serial.println("game is currently");
        Serial.println(gameActive);
        xSemaphoreGive(sysState.mutex);
        if (gameActive) {  // Run only when game is active
            // Serial.println("game is active");
            for (int i = 0; i < 10; i++) {
                if (!gameActive) break;  // Stop game if toggled off

                Serial.println("Works");
                // uint32_t randomNote = randomNotes[dist(gen)];
                // xSemaphoreTake(sysMutex, portMAX_DELAY);
                // uint32_t stepSize = stepSizes[randomNote];
                // xSemaphoreGive(sysMutex);
                int stepSize = 60740010;
                __atomic_store_n(&currentStepSize, stepSize, __ATOMIC_RELAXED);

                vTaskDelay(pdMS_TO_TICKS(500));  // Allow other tasks to run
            }
            __atomic_store_n(&currentStepSize, 0, __ATOMIC_RELAXED);  // Stop sound
        }

        vTaskDelay(pdMS_TO_TICKS(50));  // Prevent CPU overuse
    }
}
