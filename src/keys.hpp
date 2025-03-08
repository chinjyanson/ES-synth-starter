#ifndef KEYS_HPP
#define KEYS_HPP

#include <bitset>

std::bitset<12> scanKeys();
void updateStepSizeFromKeys(const std::bitset<12>& keyStates);
void scanKeysTask(void *pvParameters);

#endif // KEYS_HPP
