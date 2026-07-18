/*
  core/settings.h — Reglages persistants sur la carte SD (PAS en NVS).
  Fichier : /sdcard/PAKAMAN/SETTINGS.DAT  (meme dossier que les scores).
  Contient : langue + volume + musique on/off. Extensible ensuite.
*/
#pragma once

void settings_load();   // lit le fichier et applique langue + volume + musique
void settings_save();   // ecrit langue + volume + musique courants
