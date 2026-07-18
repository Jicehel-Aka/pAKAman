/*
  core/audio.cpp — Implementation de l'adaptateur audio sur le composant gamebuino.
  Une tache dediee (coeur 0) est le SEUL appelant de player.pool() : elle alimente
  le mixeur tres souvent, sinon le FIFO I2S se vide et le son devient inaudible.
*/
#include "audio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "gb_ll_audio.h"

AudioSettings g_audio_settings;

audio_player player;
SfxBus       g_sfx;
AudioPMF     audioPMF;

SfxBus& snd_keypress = g_sfx;

// Fin (en ms) d'occupation "bloquante" de l'audio (jingle debut ou son de mort) :
// audio_wav_is_playing() s'y refere pour synchroniser le jeu sur la fin du son.
static volatile uint32_t s_busy_until_ms = 0;

static inline uint32_t now_ms() { return (uint32_t)(esp_timer_get_time() / 1000); }

// ---------------------------------------------------------------------------
//  Tache de mixage : unique appelant de player.pool()
// ---------------------------------------------------------------------------
static void audio_task(void*) {
    while (true) {
        player.pool();
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

void audio_game_init() {
    gb_ll_audio_set_volume(200);   // volume MATERIEL du codec
    for (int i = 0; i < SfxBus::VOICES; ++i) {
        player.add_track(&g_sfx.voices[i]);
        g_sfx.voices[i].set_track_volume(g_audio_settings.sfx_volume / 255.0f);
    }
    xTaskCreatePinnedToCore(audio_task, "AudioTask", 4096, nullptr, 5, nullptr, 0);
}

void audio_set_volume(int v) {
    if (v < 0)   v = 0;
    if (v > 255) v = 255;
    g_audio_settings.master_volume = (uint8_t)v;
    player.set_master_volume((uint8_t)v);
}

// ---------------------------------------------------------------------------
//  Effets Pac-Man (synthetises)
// ---------------------------------------------------------------------------
void audio_play_pacgomme(void) {
    // "waka" : on alterne deux petits blips a chaque gomme.
    static bool hi = false;
    hi = !hi;
    g_sfx.play_tone(hi ? 520.0f : 380.0f, 45, 0.45f);
}

void audio_play_power(void) {
    // pastille de puissance : balayage montant bref.
    g_sfx.play_sweep(300.0f, 900.0f, 0.5f, 0.7f, 260);
}

void audio_play_eatghost(void) {
    // fantome mange : arpege montant rapide.
    g_sfx.play_sweep(400.0f, 1200.0f, 0.7f, 0.7f, 220);
}

void audio_play_death(void) {
    // Mort facon Pac-Man : un long balayage descendant continu (~1.5 s).
    // Les voix SfxBus jouent en parallele : on utilise donc UNE seule voix avec
    // une longue duree plutot que d'enchainer des notes (qui se superposeraient).
    // On occupe l'audio ~2.1 s pour laisser jouer toute l'animation de mort
    // (12 frames, derniere atteinte a t=66 ; ~2.0 s a 33 FPS) : la sequence de
    // mort attend audio_wav_is_playing() avant de reprendre.
    g_sfx.play_sweep(900.0f, 90.0f, 0.85f, 0.4f, 1700);
    s_busy_until_ms = now_ms() + 2100;
}

void audio_play_begin(void) {
    // jingle de debut : 3 notes montantes. On note la duree totale pour que
    // audio_wav_is_playing() retienne le lancement de la musique jusqu'a la fin.
    g_sfx.play_tone(392.0f, 120, 0.6f);   // Sol
    g_sfx.play_tone(523.0f, 120, 0.6f);   // Do
    g_sfx.play_tone(659.0f, 180, 0.6f);   // Mi
    s_busy_until_ms = now_ms() + 500;
}

bool audio_wav_is_playing(void) {
    return now_ms() < s_busy_until_ms;
}

void audio_play_fruit(void) {
    // Bonus fruit : petit arpege ascendant joyeux (3 notes).
    g_sfx.play_tone(660.0f, 70, 0.6f);   // Mi
    g_sfx.play_tone(880.0f, 70, 0.6f);   // La
    g_sfx.play_tone(1175.0f, 110, 0.6f); // Re aigu
}

// ---------------------------------------------------------------------------
//  AudioPMF — musique de fond via gb_audio_track_pmf
// ---------------------------------------------------------------------------
void AudioPMF::init(const uint8_t* data) {
    _data = data;
    _track.load_pmf(data);
    if (!_added) {
        player.add_track(&_track, g_audio_settings.music_volume / 255.0f);
        _track.set_track_volume(g_audio_settings.music_volume / 255.0f);
        _added = true;
    }
}

void AudioPMF::start(uint32_t /*sample_rate*/, uint16_t /*playlist_pos*/) {
    // gb_audio_track_pmf lit a GB_AUDIO_SAMPLE_RATE (parametre du composant).
    if (_data) _track.play_pmf();
}

void AudioPMF::stop() {
    _track.stop_playing();
}

bool AudioPMF::isPlaying() {
    return _added && _track.is_playing();
}
