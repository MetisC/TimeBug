#include "menu.h"

#include "../CORE/video.h"
#include "../CORE/input.h"
#include "../CORE/text.h"
#include "../CORE/colors.h"
#include "../CORE/sprite_dat.h"
#include "../CORE/high_scores.h"
#include "../CORE/options.h"
#include "../CORE/sound.h"

#include <string.h>

typedef struct {
    MenuOption option;
    TextId label;
} MenuEntry;

static int spr_loaded = 0;
static unsigned short spr_w = 0;
static unsigned short spr_h = 0;
static unsigned char spr_pixels[128 * 96];

// Jingle de menú, motivo repetido
static const SoundNote MENU_JINGLE[] = {
    // Motivo x2: C5 - G5 - A5
    {523, 220}, // C5
    {784, 220}, // G5
    {880, 260}, // A5
    {0,    90}, // Pausa

    {523, 220}, // C5
    {784, 220}, // G5
    {880, 260}, // A5
    {0,    90}, // Pausa

    // Remate
    {1046,300}, // C6
    {784, 220}, // G5
    {523, 520}, // C5 cierre
    {0,   200}  // Silencio final
};

static int menu_build_entries(MenuEntry *entries, int max_entries, unsigned char difficulty)
{
    HighScoreTable story_table;
    int count = 0;
    int show_locked_items = 0;

    if (!entries || max_entries <= 0) {
        return 0;
    }

    high_scores_load_story(difficulty, &story_table);
    show_locked_items = !high_scores_is_empty(&story_table);

    entries[count].option = MENU_HISTORIA;
    entries[count].label = TEXT_MENU_HISTORIA;
    count++;

    if (show_locked_items && count < max_entries) {
        entries[count].option = MENU_EXTRA;
        entries[count].label = TEXT_MENU_EXTRA;
        count++;
    }

    if (show_locked_items && count < max_entries) {
        entries[count].option = MENU_HIGH_SCORES;
        entries[count].label = TEXT_MENU_HIGH_SCORES;
        count++;
    }

    if (count < max_entries) {
        entries[count].option = MENU_OPCIONES;
        entries[count].label = TEXT_MENU_OPCIONES;
        count++;
    }

    if (count < max_entries) {
        entries[count].option = MENU_SALIR;
        entries[count].label = TEXT_MENU_SALIR;
        count++;
    }

    return count;
}

static void menu_try_load_sprite(void)
{
    if (spr_loaded) return;

    if (!sprite_dat_load_auto("SPRITES\\Logo.dat", &spr_w, &spr_h, (unsigned char far *)spr_pixels,
                              (unsigned long)sizeof(spr_pixels))) {
        spr_w = 0;
        spr_h = 0;
    }

    spr_loaded = 1;
}

static void menu_draw(const MenuEntry *entries, int count, int selected)
{
    int i;
    int start_y = 120;
    int gap = 12;
    int x;
    int y;
    const char *label;

    menu_try_load_sprite();

    v_clear(0);

    if (spr_w && spr_h) {
        v_blit_sprite((VIDEO_WIDTH - spr_w) / 2, 16, spr_w, spr_h, (const unsigned char far *)spr_pixels, 0);
    }

    for (i = 0; i < count; ++i) {
        unsigned char color = (i == selected)
            ? COL_TEXT_SELECTED
            : COL_TEXT_NORMAL;
        label = text_get(entries[i].label);
        y = start_y + (i * gap);
        x = (VIDEO_WIDTH - ((int)strlen(label) * 8)) / 2;
        v_puts(x, y, label, color);
    }

    v_present();
}

MenuOption menu_run(void)
{
    const GameOptions *options = options_get();
    MenuEntry entries[6];
    int selected = 0;
    int count = menu_build_entries(entries, (int)(sizeof(entries) / sizeof(entries[0])),
                                   options ? options->difficulty : DIFFICULTY_NORMAL);
    int key;

    menu_draw(entries, count, selected);
    sound_play_melody(MENU_JINGLE, (int)(sizeof(MENU_JINGLE) / sizeof(MENU_JINGLE[0])));

    while (1) {
        key = in_poll();
        if (key == IN_KEY_NONE) {
            continue;
        }

        if (key == IN_KEY_UP) {
            if (selected > 0) {
                selected--;
                menu_draw(entries, count, selected);
            }
        } else if (key == IN_KEY_DOWN) {
            if (selected < count - 1) {
                selected++;
                menu_draw(entries, count, selected);
            }
        } else if (key == IN_KEY_ENTER) {
            return entries[selected].option;
        } else if (key == IN_KEY_ESC) {
            return MENU_SALIR;
        }
    }
}
