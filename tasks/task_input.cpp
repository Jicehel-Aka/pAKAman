/*
============================================================
  task_input.cpp — Tâche input (100 Hz)
------------------------------------------------------------
Ajouts :
 - gestion des touches maintenues (held)
 - combo HOME+MENU → reboot loader
============================================================
*/

#include "task_input.h"
#include "core/input.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "lib/expander.h"   
#include "esp_system.h" 
#include <esp_ota_ops.h>
#include <esp_partition.h>

static uint32_t comboStart = 0;

static void checkReturnToLoader()
{
    bool homeHeld = keyHeld(g_keys, EXPANDER_KEY_RUN, 500);
    bool menuHeld = keyHeld(g_keys, EXPANDER_KEY_MENU, 500);

    uint32_t now = esp_timer_get_time() / 1000;

    if (homeHeld && menuHeld) {
        if (comboStart == 0)
            comboStart = now;
        else if (now - comboStart >= 500) {
            comboStart = 0;

            const esp_partition_t* loader =
                esp_partition_find_first(
                    ESP_PARTITION_TYPE_APP,
                    ESP_PARTITION_SUBTYPE_APP_OTA_1,
                    nullptr
                );

            if (loader) {
                esp_ota_set_boot_partition(loader);
                esp_restart();
            }
        }
    } else {
        comboStart = 0;
    }
}

void task_input(void* param)
{
    const int INPUT_US = 10000; // 100 Hz
    int64_t last = esp_timer_get_time();

    while (true)
    {
        int64_t now = esp_timer_get_time();
        int64_t dt = now - last;

        if (dt >= INPUT_US)
        {
            last = now;
            input_poll(g_keys);
            checkReturnToLoader();
        }
        else
        {
            esp_rom_delay_us(2000);
        }
    }
}
