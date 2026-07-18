#include "sdcard.h"
#include "gb_ll_sdcard.h"
#include <sys/stat.h>
#include <errno.h>

bool sd_init() {
    // NE PAS remonter la carte ici : g_core.init() a deja appele gb_ll_sd_init()
    // qui monte /sdcard. Un second esp_vfs_fat_sdmmc_mount reinitialise le
    // peripherique SDMMC sous une carte deja montee et casse l'acces fichiers
    // (echec silencieux des fopen -> "Aucun score", sauvegarde impossible).
    return true;
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
