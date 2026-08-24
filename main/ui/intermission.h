#pragma once
/*
============================================================
  ui/intermission.h — Intermede narratif entre deux niveaux
------------------------------------------------------------
  A la maniere des cutscenes de l'arcade Pac-Man original
  (apres les niveaux 2, 5 et 9, puis periodiquement) : une
  courte scene animee avec legende, purement cosmetique
  (aucune regle de jeu n'est affectee). Casse la monotonie
  entre deux niveaux par un petit fil narratif recurrent.

  Appele par app_main.cpp sur GameState::State::LevelComplete,
  avant game_advance_to_next_level(). Ecran bloquant, meme
  convention que menu_open()/highscores_submit().
============================================================
*/

// completed_level = numero du niveau qui vient d'etre termine
// (GameState::last_completed_level). Determine le "chapitre"
// de l'histoire et le fantome mis en scene.
void intermission_show(int completed_level);
