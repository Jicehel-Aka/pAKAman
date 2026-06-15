/*
============================================================
  persist.cpp — Persistance SD card (/sdcard/PAKAMAN/)
------------------------------------------------------------
  1. Highscores  : binaire { char name[9] + int32_t score }
  2. AudioSettings : texte clé=valeur
------------------------------------------------------------
  Garantit la création du répertoire PAKAMAN à l'init.
============================================================
*/

#include "persist.h"
#include "core/audio.h"
#include <cstdio>
#include <cstring>
#include <vector>
#include <sys/stat.h>   // mkdir

static constexpr const char* PAKAMAN_DIR    = "/sdcard/PAKAMAN";
static constexpr const char* SETTINGS_PATH  = "/sdcard/PAKAMAN/settings.cfg";
static constexpr const char* SCORES_PATH    = "/sdcard/PAKAMAN/highscores.dat";

// ---------------------------------------------------------------------------
//  Assure l'existence du répertoire (appelé avant tout fopen en écriture)
// ---------------------------------------------------------------------------
static void ensure_dir()
{
    struct stat st;
    if (stat(PAKAMAN_DIR, &st) != 0)
        mkdir(PAKAMAN_DIR, 0755);
}

// ===========================================================================
//  HIGHSCORES
// ===========================================================================

void persist_load(std::vector<HighScore>& scores)
{
    scores.clear();

    FILE* f = fopen(SCORES_PATH, "rb");
    if (!f) return;   // fichier absent = pas encore de scores, silencieux

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || size % (long)sizeof(HighScore) != 0) {
        printf("[persist] highscores.dat corrompu (%ld octets)\n", size);
        fclose(f);
        return;
    }

    scores.resize((size_t)(size / sizeof(HighScore)));
    fread(scores.data(), sizeof(HighScore), scores.size(), f);
    fclose(f);
}

void persist_save(const std::vector<HighScore>& scores)
{
    ensure_dir();

    FILE* f = fopen(SCORES_PATH, "wb");
    if (!f) {
        printf("[persist] Impossible d'écrire %s\n", SCORES_PATH);
        return;
    }

    fwrite(scores.data(), sizeof(HighScore), scores.size(), f);
    fclose(f);
}

// ===========================================================================
//  RÉGLAGES AUDIO
// ===========================================================================

bool audio_settings_load()
{
    FILE* f = fopen(SETTINGS_PATH, "r");
    if (!f) return false;

    char key[64], value[64];
    while (fscanf(f, "%63[^=]=%63s\n", key, value) == 2)
    {
        if      (strcmp(key, "music_enabled")  == 0) g_audio_settings.music_enabled  = atoi(value);
        else if (strcmp(key, "music_volume")   == 0) g_audio_settings.music_volume   = atoi(value);
        else if (strcmp(key, "sfx_volume")     == 0) g_audio_settings.sfx_volume     = atoi(value);
        else if (strcmp(key, "master_volume")  == 0) g_audio_settings.master_volume  = atoi(value);
    }

    fclose(f);
    return true;
}

bool audio_settings_save()
{
    ensure_dir();

    FILE* f = fopen(SETTINGS_PATH, "w");
    if (!f) {
        printf("[persist] Impossible d'écrire %s\n", SETTINGS_PATH);
        return false;
    }

    fprintf(f, "music_enabled=%d\n",  g_audio_settings.music_enabled);
    fprintf(f, "music_volume=%d\n",   g_audio_settings.music_volume);
    fprintf(f, "sfx_volume=%d\n",     g_audio_settings.sfx_volume);
    fprintf(f, "master_volume=%d\n",  g_audio_settings.master_volume);

    fclose(f);
    return true;
}
