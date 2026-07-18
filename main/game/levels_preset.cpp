#include "levels_preset.h"

Level build_level(int id){
    Level L; L.id=id;
    L.generate_grid(6,9,1);
    return L;
}
