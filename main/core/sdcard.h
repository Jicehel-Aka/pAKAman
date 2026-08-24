/*
  core/sdcard.h — Petit wrapper SD au-dessus du composant gamebuino.
  Le composant expose desormais gb_ll_sd_init() (appele par g_core.init(),
  ne pas le rappeler ici) et gb_ll_sd_is_mounted() pour lire l'etat reel du
  montage. mkdir/exists restent faits en POSIX : la carte est montee sur
  /sdcard (MOUNT_POINT du SDK).
*/
#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool sd_init();                       // monte la carte (gb_ll_sd_init)
bool sd_mkdir(const char* path);      // cree un dossier (ok s'il existe deja)
bool sd_exists(const char* path);     // vrai si le fichier/dossier existe

#ifdef __cplusplus
}
#endif
