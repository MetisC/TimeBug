#include "text.h"

char game_title[32] = "TIME BUG";

static const char *const text_table[TEXT_COUNT] = {
    "HISTORIA",
    "EXTRA",
    "HIGH SCORES",
    "OPCIONES",
    "DEBUG",
    "SALIR",
    game_title,
    "OPCIONES",
    "PULSA UNA TECLA",
    "DEDICADO AL MS-DOS CLUB",
    "SELECCIONA JUEGO",
    "ESC - VOLVER",
    "ENTER - SELECCIONAR",
    "DIFICULTAD",
    "SONIDO",
    "VELOCIDAD DE JUEGO",
    "ENTRADA",
    "VOLVER",
    "FACIL",
    "NORMAL",
    "DIFICIL",
    "ACTIVADO",
    "DESACTIVADO",
    "NORMAL",
    "TURBO",
    "TECLADO",
    "JOYSTICK",
    "IZQ/DER CAMBIAR  ENTER/ESC VOLVER",
    "ENTER SELECCIONAR  ESC VOLVER",
    "JOYSTICK NO ENCONTRADO",
    "NO SE PUDO GUARDAR OPCIONES"
};

const char *text_get(TextId id)
{
    if (id < 0 || id >= TEXT_COUNT) {
        return "";
    }
    return text_table[id];
}
