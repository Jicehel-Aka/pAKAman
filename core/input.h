#pragma once
#include <cstdint>

struct Keys {
    uint32_t raw;       // état brut des touches (bitmask)
    uint32_t pressed;   // touches pressées ce cycle
    uint32_t released;  // touches relâchées ce cycle

    uint32_t holdStart[32]; // timestamp d’appui par bit
    uint32_t held;          // bitmask des touches maintenues

    int joxx, joxy;

    bool up, down, left, right;
    bool A, B, C, D, RUN, MENU, R1, L1;
};

extern Keys g_keys;

void input_init();
void input_poll(Keys& k);

// utilitaires
bool keyHeld(const Keys& k, uint32_t keyMask, uint32_t durationMs);
