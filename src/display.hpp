#ifndef DISPLAY_HPP
#define DISPLAY_HPP
#include <string>

void displayUpdateTask(void *pvParameters);
extern bool waiting_for_user;
extern bool playing_music;
extern bool correct_guess;
extern std::string correct_answer; 

#endif // DISPLAY_HPP