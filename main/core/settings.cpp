#include "settings.h"
#include "i18n.h"
#include "sdcard.h"
#include "audio.h"      // g_audio_settings
#include <cstdio>
#include <cstdint>

extern int volume;      // global (app_main), 0..255

#define SETTINGS_DIR  "/sdcard/PAKAMAN"
#define SETTINGS_FILE "/sdcard/PAKAMAN/SETTINGS.DAT"

// Bloc binaire simple avec octet magique (evite de lire un fichier etranger).
struct SettingsBlob {
    uint8_t magic;
    uint8_t language;
    uint8_t volume;
    uint8_t music_on;
};
static const uint8_t SETTINGS_MAGIC = 0xAC;   // pAKAman

void settings_load() {
    FILE* f = fopen(SETTINGS_FILE, "rb");
    if (!f) return;                      // pas de fichier -> on garde les defauts
    SettingsBlob b{};
    size_t n = fread(&b, 1, sizeof b, f);
    fclose(f);
    if (n == sizeof b && b.magic == SETTINGS_MAGIC) {
        if (b.language < i18n::LANG_COUNT) i18n::set_language((i18n::Lang)b.language);
        volume = b.volume;
        g_audio_settings.music_enabled = (b.music_on != 0);
    }
    printf("settings: charges (langue=%d volume=%d musique=%d)\n",
           (int)i18n::get_language(), volume, (int)g_audio_settings.music_enabled);
}

void settings_save() {
    sd_mkdir(SETTINGS_DIR);
    FILE* f = fopen(SETTINGS_FILE, "wb");
    if (!f) { printf("settings: ECHEC ecriture %s\n", SETTINGS_FILE); return; }
    SettingsBlob b{ SETTINGS_MAGIC,
                    (uint8_t)i18n::get_language(),
                    (uint8_t)volume,
                    (uint8_t)(g_audio_settings.music_enabled ? 1 : 0) };
    fwrite(&b, 1, sizeof b, f);
    fflush(f);
    fclose(f);
}
