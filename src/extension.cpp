#include "extension.hpp"
#include <Arduino.h>
#include "audio.hpp"
#include "system.hpp"
#include "display.hpp"
#include <STM32FreeRTOS.h>
#include <random>
#include <cmath>
#include <string>
#include <array>
#include <cstdlib>
#include <utility>
#include <vector>

std::pair<int, size_t> getRandomNote() {
    static bool seeded = false;
    if (!seeded) {
        srand(analogRead(A0));  // Seed using an unconnected analog pin for randomness
        seeded = true;
    }

    std::array<uint32_t, 12> randomNotes = getArray();
    // if (randomNotes.empty()) return -1;  // Handle edge case

    size_t randomIndex = rand() % randomNotes.size();  // Generate a random index
    return {randomNotes[randomIndex], randomIndex};
}

std::string indexToNoteName(size_t index) {
    std::vector<std::string> noteNames = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    return noteNames[index];
}

void gameTask(void *pvParameters) {
    std::array<uint32_t, 12> randomNotes = getArray();

    while (true) {
        xSemaphoreTake(sysState.mutex, portMAX_DELAY);
        bool gameActive = sysState.areAllKnobSPressed;
        xSemaphoreGive(sysState.mutex);
        bool firstRun = true;
        uint32_t randomNote = 0;
        size_t noteIndex = 13;
        // int noteIndex = -1;

        while (gameActive) {
            if (firstRun) {
                Serial.println("Game started!");
                std::pair<int, size_t> randomNoteResult = getRandomNote();
                randomNote = randomNoteResult.first;
                noteIndex = randomNoteResult.second;
                // std::string noteName = frequencyToNoteName(randomNote);
                // noteIndex = noteNameToIndex(noteName);
                xSemaphoreTake(sysState.mutex, portMAX_DELAY);
                sysState.gameActiveOverride = true;
                xSemaphoreGive(sysState.mutex);
                firstRun = false;
            }
            playing_music = true;
            // **Play sound for 3 seconds**
            int stepSize = randomNote;  // Set the step size to generate the sound
            __atomic_store_n(&currentStepSize, stepSize, __ATOMIC_RELAXED);  // Play the sound

            // Wait for 3 seconds while the sound plays
            vTaskDelay(pdMS_TO_TICKS(2000));  // 2 seconds

            // **Stop sound after 3 seconds**
            __atomic_store_n(&currentStepSize, 0, __ATOMIC_RELAXED);  // Stop the sound
            playing_music = false;

            // **Allow the user to guess**
            // Temporarily disable the gameActiveOverride flag to allow key input to produce sound
            xSemaphoreTake(sysState.mutex, portMAX_DELAY);
            sysState.gameActiveOverride = false;  // Re-enable key scanning and sound production
            std::bitset<12> userKeys = sysState.keyStates;  // Get the current key states
            xSemaphoreGive(sysState.mutex);

            while (userKeys.none() and gameActive) {  // .none() returns true if all bits are 0 (i.e., no key is pressed)
                Serial.println("Waiting for key press...");
                waiting_for_user = true;
                xSemaphoreTake(sysState.mutex, portMAX_DELAY);
                userKeys = sysState.keyStates;  // Refresh the key states
                gameActive = sysState.areAllKnobSPressed;  // Check if the game should continue or exit
                xSemaphoreGive(sysState.mutex);

                vTaskDelay(pdMS_TO_TICKS(10));  // Short delay to prevent the loop from hogging the CPU
            }

            if (noteIndex >= 0 && noteIndex < 12 and gameActive) {  // Ensure valid index
                waiting_for_user = false;
                if (userKeys[noteIndex] == 1) {  // Check if the key is pressed
                    correct_guess = true;
                    Serial.println("Correct key pressed!");
                } else {
                    correct_guess = false;
                    Serial.println("Wrong key pressed!");
                }
                correct_answer = indexToNoteName(noteIndex);
                firstRun = true;  // Reset the game for the next round
            }

            // **Prepare for the next round**
            Serial.println("Next round starting...");
            vTaskDelay(pdMS_TO_TICKS(3000));  // Allow a small delay before the next round starts

             // **Check if the user pressed all knobs to exit game**
             xSemaphoreTake(sysState.mutex, portMAX_DELAY);
             gameActive = sysState.areAllKnobSPressed;  // Check if the game should continue or exit
             xSemaphoreGive(sysState.mutex);
        }

        // **Exit game if knobs are pressed**
        // Serial.println("Game exited, returning to other mode.");
        vTaskDelay(pdMS_TO_TICKS(50));  // Prevent CPU overuse
    }
}
