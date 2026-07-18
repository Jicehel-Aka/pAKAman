/*
  core/audio.h — Adaptateur audio pAKAman sur le composant gamebuino.

  Le player du composant a 4 voix (AUDIO_PLAYER_TRACK_COUNT). Repartition :
    - 3 voix pour les effets (SfxBus, round-robin : un son n'en coupe plus un
      autre, ex. gomme + fantome mange en meme temps).
    - 1 voix pour la musique de fond PMF (pacman_pmf), pilotee par AudioPMF.

  Compat : le jeu (game.cpp / pacman.cpp) continue d'appeler audio_play_xxx() et
  audioPMF.init/start/stop/isPlaying() sans aucune modification.
*/
#pragma once
#include "gb_audio_player.h"
#include "gb_audio_track_tone.h"
#include "gb_audio_track_pmf.h"
#include <cstdint>

// -----------------------------------------------------------------------------
//  Reglages audio (conserves pour compat : game.cpp lit music_enabled)
// -----------------------------------------------------------------------------
struct AudioSettings {
    bool    music_enabled  = true;
    uint8_t music_volume   = 160;   // 0..255 (volume relatif piste musique)
    uint8_t sfx_volume     = 220;   // 0..255 (volume relatif pistes SFX)
    uint8_t master_volume  = 200;   // 0..255 (volume general)
};
extern AudioSettings g_audio_settings;

// -----------------------------------------------------------------------------
//  Bus d'effets : 3 voix tone en round-robin
// -----------------------------------------------------------------------------
struct SfxBus {
    static const int VOICES = 3;
    gb_audio_track_tone voices[VOICES];
    int next = 0;
    void play_tone(float f32_frequency, uint16_t u16_duration_ms, float gain = 1.0f) {
        if (gain < 0.0f) gain = 0.0f;
        if (gain > 1.0f) gain = 1.0f;
        voices[next].play_tone(f32_frequency, gain, u16_duration_ms);
        next = (next + 1) % VOICES;
    }
    // Version balayage (frequence + volume qui evoluent), pour les effets riches.
    void play_sweep(float f0, float f1, float g0, float g1, uint16_t ms) {
        voices[next].play_tone(f0, f1, g0, g1, ms);
        next = (next + 1) % VOICES;
    }
};

// -----------------------------------------------------------------------------
//  Musique PMF — meme interface que l'ancienne classe AudioPMF (lib/audio_pmf.h)
//  mais implementee sur gb_audio_track_pmf du composant.
// -----------------------------------------------------------------------------
class AudioPMF {
public:
    void init(const uint8_t* data);                              // charge + enregistre la piste (1x)
    void start(uint32_t sample_rate, uint16_t playlist_pos = 0); // (re)demarre la lecture
    void stop();                                                 // arrete la lecture
    bool isPlaying();                                            // etat de lecture
private:
    gb_audio_track_pmf _track;
    const uint8_t*     _data   {nullptr};
    bool               _added  {false};
};

// -----------------------------------------------------------------------------
//  Instances globales
// -----------------------------------------------------------------------------
using audio_player = gb_audio_player;
extern audio_player player;
extern SfxBus       g_sfx;
extern AudioPMF     audioPMF;

// Alias utilises par l'UI (menu moderne).
extern SfxBus& snd_keypress;

// -----------------------------------------------------------------------------
//  API
// -----------------------------------------------------------------------------
void audio_game_init();          // init codec + voix SFX + tache de mixage
void audio_set_volume(int v);    // volume general 0..255

// Effets Pac-Man (memes noms qu'avant ; synthetises en tons).
void audio_play_pacgomme(void);
void audio_play_power(void);
void audio_play_eatghost(void);
void audio_play_death(void);
void audio_play_begin(void);
void audio_play_fruit(void);   // bonus fruit ramasse

// Vrai tant que le jingle de debut (audio_play_begin) n'est pas termine :
// le jeu attend cette fin avant de lancer la musique de fond.
bool audio_wav_is_playing(void);
