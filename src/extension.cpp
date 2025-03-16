#include "extension.hpp"
#include <Arduino.h>
#include "audio.hpp"
#include "system.hpp"
#include <STM32FreeRTOS.h>
#include <random>
#include <cmath>
#include <string>

int getRandomNote() {
    static bool seeded = false;
    if (!seeded) {
        srand(analogRead(A0));  // Seed using an unconnected analog pin for randomness
        seeded = true;
    }

    std::array<uint32_t, 12> randomNotes = getArray();
    if (randomNotes.empty()) return -1;  // Handle edge case

    size_t randomIndex = rand() % randomNotes.size();  // Generate a random index
    return randomNotes[randomIndex];
}


std::string frequencyToNoteName(double frequency) {
    if (frequency <= 0) return "Invalid";  // Handle invalid cases
    // Compute number of semitones away from A4 (440 Hz)
    int n = round(12 * log2(frequency / 440.0));
    // Determine the note name and octave
    std::string noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

    // A4 is MIDI note 69, so add n to get MIDI note number
    int midiNote = 69 + n;

    // Get the note name from the MIDI note number
    std::string note = noteNames[midiNote % 12];

    return note;
}

int noteNameToIndex(const std::string& note) {
    // Define the mapping of note names to their indices
    std::unordered_map<std::string, int> noteMap = {
        {"C", 0}, {"C#", 1}, {"D", 2}, {"D#", 3}, {"E", 4}, {"F", 5},
        {"F#", 6}, {"G", 7}, {"G#", 8}, {"A", 9}, {"A#", 10}, {"B", 11}
    };

    // Check if the note exists in the map
    auto it = noteMap.find(note);
    if (it != noteMap.end()) {
        return it->second;  // Return the index if found
    } else {
        return -1;  // Return -1 for an invalid note
    }
}

void gameTask(void *pvParameters) {
    std::array<uint32_t, 12> randomNotes = getArray();

    while (true) {
        xSemaphoreTake(sysState.mutex, portMAX_DELAY);
        bool gameActive = sysState.areAllKnobSPressed;
        xSemaphoreGive(sysState.mutex);
        bool firstRun = true;
        uint32_t randomNote = 0;
        int noteIndex = -1;

        while (gameActive) {
            if (firstRun) {
                Serial.println("Game started!");
                randomNote = getRandomNote();
                std::string noteName = frequencyToNoteName(randomNote);
                noteIndex = noteNameToIndex(noteName);
                xSemaphoreTake(sysState.mutex, portMAX_DELAY);
                sysState.gameActiveOverride = true;
                xSemaphoreGive(sysState.mutex);
                firstRun = false;
            }

            // **Play sound for 3 seconds**
            int stepSize = randomNote;  // Set the step size to generate the sound
            __atomic_store_n(&currentStepSize, stepSize, __ATOMIC_RELAXED);  // Play the sound

            // Wait for 3 seconds while the sound plays
            vTaskDelay(pdMS_TO_TICKS(3000));  // 3 seconds

            // **Stop sound after 3 seconds**
            __atomic_store_n(&currentStepSize, 0, __ATOMIC_RELAXED);  // Stop the sound

            // **Allow the user to guess**
            // Temporarily disable the gameActiveOverride flag to allow key input to produce sound
            xSemaphoreTake(sysState.mutex, portMAX_DELAY);
            sysState.gameActiveOverride = false;  // Re-enable key scanning and sound production
            std::bitset<12> userKeys = sysState.keyStates;  // Get the current key states
            xSemaphoreGive(sysState.mutex);

            while (userKeys.none() and gameActive) {  // .none() returns true if all bits are 0 (i.e., no key is pressed)
                Serial.println("Waiting for key press...");
                xSemaphoreTake(sysState.mutex, portMAX_DELAY);
                userKeys = sysState.keyStates;  // Refresh the key states
                gameActive = sysState.areAllKnobSPressed;  // Check if the game should continue or exit
                xSemaphoreGive(sysState.mutex);

                vTaskDelay(pdMS_TO_TICKS(10));  // Short delay to prevent the loop from hogging the CPU
            }

            if (noteIndex >= 0 && noteIndex < 12 and gameActive) {  // Ensure valid index
                if (userKeys[noteIndex] == 1) {  // Check if the key is pressed
                    Serial.println("Correct key pressed!");
                } else {
                    Serial.println("Wrong key pressed!");
                }
                firstRun = true;  // Reset the game for the next round
            }

            // **Prepare for the next round**
            Serial.println("Next round starting...");
            vTaskDelay(pdMS_TO_TICKS(2000));  // Allow a small delay before the next round starts

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
