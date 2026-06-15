#include "input.h"
#include "lib/expander.h"
#include "driver/gpio.h"
#include "esp_timer.h"

static uint16_t prev = 0;
Keys g_keys;

void input_init() {
    prev = 0;
    for (int i = 0; i < 32; i++)
        g_keys.holdStart[i] = 0;
    g_keys.held = 0;
}

void input_poll(Keys& k) {
    uint32_t now = esp_timer_get_time() / 1000; // ms
    uint16_t raw = expander_read();
    k.raw = raw;

    k.pressed  = raw & ~prev;
    k.released = prev & ~raw;
    prev = raw;

    // --- Gestion des timestamps d’appui ---
    for (int bit = 0; bit < 16; bit++) {
        uint32_t mask = (1 << bit);

        if (raw & mask) {
            if (k.holdStart[bit] == 0)
                k.holdStart[bit] = now;
        } else {
            k.holdStart[bit] = 0;
        }
    }

    // --- Calcul des touches maintenues (held > 500 ms) ---
    k.held = 0;
    for (int bit = 0; bit < 16; bit++) {
        if (k.holdStart[bit] != 0 && (now - k.holdStart[bit] >= 500))
            k.held |= (1 << bit);
    }

    // --- Mapping boutons ---
    k.up    = raw & EXPANDER_KEY_UP;
    k.down  = raw & EXPANDER_KEY_DOWN;
    k.left  = raw & EXPANDER_KEY_LEFT;
    k.right = raw & EXPANDER_KEY_RIGHT;

    k.A   = raw & EXPANDER_KEY_A;
    k.B   = raw & EXPANDER_KEY_B;
    k.C   = raw & EXPANDER_KEY_C;
    k.D   = raw & EXPANDER_KEY_D;
    k.RUN = raw & EXPANDER_KEY_RUN;
    k.MENU= raw & EXPANDER_KEY_MENU;
    k.R1  = raw & EXPANDER_KEY_R1;
    k.L1  = raw & EXPANDER_KEY_L1;

    k.joxx = adc_read_joyx();
    k.joxy = adc_read_joyy();

    g_keys = k;
}

bool keyHeld(const Keys& k, uint32_t keyMask, uint32_t durationMs) {
    uint32_t now = esp_timer_get_time() / 1000;
    for (int bit = 0; bit < 16; bit++) {
        if (keyMask & (1 << bit)) {
            if (k.holdStart[bit] != 0 &&
                (now - k.holdStart[bit] >= durationMs))
                return true;
        }
    }
    return false;
}
