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
    // // Octave number (A4 is octave 4, MIDI note 69)
    // int octave = (midiNote / 12) - 1;
    // Get note name (mod 12)
    std::string note = noteNames[midiNote % 12];

    // Return formatted note name
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
        // Serial.print("Current Key States: ");
        // for (int i = 0; i < 12; i++) {
        //     Serial.print(localKeys[i]);  // Print each bit (0 or 1)
        //     Serial.print(" ");  // Add space for readability
        // }
        // Serial.println();  // Newline for better formatting

        while (gameActive) {  // Run only when game is active
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

            xSemaphoreTake(sysState.mutex, portMAX_DELAY);
            std::bitset<12> localKeys = sysState.keyStates;
            xSemaphoreGive(sysState.mutex);

            if (noteIndex >= 0 && noteIndex < 12) {  // Ensure valid index
                if (localKeys[noteIndex] == 1) {  // Check if the key is pressed
                    Serial.println("Correct key pressed!");
                } else {
                    Serial.println("Wrong key or no key pressed.");
                }
            }

            int stepSize = randomNote;  // Placeholder step size
            __atomic_store_n(&currentStepSize, stepSize, __ATOMIC_RELAXED);

            vTaskDelay(pdMS_TO_TICKS(100));  // Allow other tasks to run
            // __atomic_store_n(&currentStepSize, 0, __ATOMIC_RELAXED);  // Stop the sound

            // **Re-check if game is still active**
            xSemaphoreTake(sysState.mutex, portMAX_DELAY);
            gameActive = sysState.areAllKnobSPressed;  // Update the game state
            if (!gameActive) {
                sysState.gameActiveOverride = false;  // Re-enable step size updates from scanKeysTask
                __atomic_store_n(&currentStepSize, 0, __ATOMIC_RELAXED);  // Stop sound after game ends
            }
            xSemaphoreGive(sysState.mutex);
        }

        vTaskDelay(pdMS_TO_TICKS(50));  // Prevent CPU overuse
    }
}

