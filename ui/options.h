#pragma once
#include "core/input.h"
#include "game/game.h"

extern int nav_cooldown;

void draw_options_menu(GameState::State state, int index);
// ATTENTION : signature modifiée — state est maintenant requis pour le wrap du curseur
void handle_audio_options_navigation(const Keys& k, int& index, GameState::State state);
