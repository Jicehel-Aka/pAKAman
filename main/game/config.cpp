#include "config.h"

// Définition unique
int debug = 0;   // 0 = off, 1 = on



/* Exemple d'usage du debug
#if 1   // on peut ajouter un #if DEBUG pour effectuer des macros DEBUG
    #define DBG(code) do { if (debug) { code; } } while(0)
#else
    #define DBG(code) do { } while(0)
#endif
                                                                       */