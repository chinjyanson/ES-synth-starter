#ifndef AUDIO_HPP
#define AUDIO_HPP

#include <Arduino.h>

extern volatile uint32_t currentStepSize;
extern std::array<uint32_t, 12> stepSizes;

std::array<uint32_t, 12> getArray();
void sampleISR();

#endif // AUDIO_HPP
