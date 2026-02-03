#ifndef MENU_H
#define MENU_H

#include "../main.h"

typedef enum {
    MENU_HISTORIA = 0,
    MENU_EXTRA,
    MENU_HIGH_SCORES,
    MENU_OPCIONES,
    MENU_SALIR,
    MENU_COUNT
} MenuOption;

MenuOption menu_run(void);

#endif
