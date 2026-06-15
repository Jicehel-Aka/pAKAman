/*
============================================================
  app_main.cpp — Point d’entrée, multitâche FreeRTOS
------------------------------------------------------------
 - Initialisation hardware
 - Création des tâches (jeu, input, audio, tests)
 - Boucle idle
------------------------------------------------------------
Toute la logique du jeu est dans task_game.cpp
Toute la logique audio est dans task_audio.cpp
Toute la logique input est dans task_input.cpp
============================================================
*/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

// Hardware libs
#include "lib/expander.h"
#include "lib/LCD.h"
#include "lib/sdcard.h"
#include "lib/audio_sfx.h"
#include "lib/audio_player.h"
#include "lib/audio_sfx_cache.h"

// Core
#include "core/input.h"
#include "core/graphics.h"
#include "core/audio.h"
#include "core/persist.h"
#include "ui/highscores.h"

// Game
#include "game/game.h"
#include "game/level.h"
#include "assets/assets.h"

// Tasks
#include "tasks/task_game.h"
#include "tasks/task_audio.h"
#include "tasks/task_input.h"

// Tests (optionnel)
#include "tests/tests.h"

/*
============================================================
  INITIALISATION HARDWARE
============================================================
*/
static void hardware_init()
{
    // LCD / rétroéclairage
    lcd_init_pwm();
    lcd_update_pwm(64);

    // Entrées / ADC / expander
    adc_init();
    expander_init();
    LCD_init();

    // SD / persistance
    sd_init();

    // Audio
    audio_settings_load();
    audio_init();
    vTaskDelay(pdMS_TO_TICKS(300));
    sfx_cache_preload_all();

    // Core
    input_init();
    assets_init();
    highscores_init();
}

/*
============================================================
  app_main — POINT D’ENTRÉE
============================================================
*/
extern "C" void app_main(void)
{
    hardware_init();

    // --- Création des tâches ---
    // Jeu : logique + rendu
    xTaskCreatePinnedToCore(
        task_game,
        "GameTask",
        8192,
        nullptr,
        5,
        nullptr,
        1
    );

    // Audio : sfx / musique (si activé)
    // À réactiver si tu veux un thread audio dédié
    // xTaskCreatePinnedToCore(
    //     task_audio,
    //     "AudioTask",
    //     8192,
    //     nullptr,
    //     6,
    //     nullptr,
    //     0
    // );

    // Input : lecture des touches, états held, combos (HOME+MENU)
    xTaskCreatePinnedToCore(
        task_input,
        "InputTask",
        2048,
        nullptr,
        4,
        nullptr,
        1
    );

#ifdef ENABLE_TESTS
    xTaskCreatePinnedToCore(
        task_tests,
        "TestsTask",
        4096,
        nullptr,
        1,
        nullptr,
        1
    );
#endif

    // --- Boucle idle ---
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
