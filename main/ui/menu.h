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
