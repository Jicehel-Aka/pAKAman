#pragma once

// Menu moderne ouvert par MENU. Modal : gere lui-meme musique, volume, langue,
// scores, commandes, recalibrage stick, retour loader. Rend la main via une
// action que la boucle principale applique.
enum class MenuAction {
    Resume,       // fermer le menu, reprendre/continuer
    StartGame,    // lancer une nouvelle partie
    ReturnTitle   // revenir a l'ecran-titre
};

// in_game = true si une partie est en cours (affiche "Reprendre" au lieu de "Jouer").
MenuAction menu_open(bool in_game);

// Ecran de confirmation bloquant (A = confirmer, B = annuler) avant une action
// destructrice (retour au loader = perte de la partie en cours). Utilise a la
// fois par l'entree de menu "Retour au loader" et par le combo global
// RUN+MENU (app_main.cpp), pour eviter qu'un appui accidentel ne fasse
// perdre la partie en cours sans avertissement.
bool confirm_return_to_loader();
