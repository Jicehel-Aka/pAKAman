#include "sdcard.h"
#include "gb_ll_sdcard.h"
#include "gb_err.h"
#include <sys/stat.h>
#include <errno.h>

bool sd_init() {
    // NE PAS remonter la carte ici : g_core.init() a deja appele gb_ll_sd_init()
    // (qui gere lui-meme le cas "deja monte" via GB_ERR_BUSY) - un second appel
    // reinitialiserait le peripherique SDMMC sous une carte deja montee et
    // casserait l'acces fichiers (echec silencieux des fopen -> "Aucun score").
    //
    // Depuis la mise a jour du composant, l'etat reel du montage est expose
    // directement par la bibliotheque (gb_ll_sd_is_mounted()) : plus besoin
    // d'un test d'ecriture manuel cote application pour le savoir.
    return gb_ll_sd_is_mounted();
}

bool sd_mkdir(const char* path) {
    if (!path) return false;
    if (mkdir(path, 0777) == 0) return true;
    return errno == EEXIST;   // deja present = succes
}

bool sd_exists(const char* path) {
    struct stat st;
    return path && stat(path, &st) == 0;
}
