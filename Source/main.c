#include "main.h"

#include "CORE/video.h"
#include "CORE/input.h"
#include "CORE/keyboard.h"
#include "CORE/timer.h"
#include "CORE/options.h"
#include "CORE/records.h"
#include "CORE/sound.h"
#include "CORE/text.h"
#include "GAME/menu.h"
#include "GAME/options_menu.h"
#include "GAME/select_year.h"
#include "GAME/story_mode.h"
#include "GAME/story_high_scores.h"
#include "GAME/year_launcher.h"


static void draw_center_text(const char *text, int y, unsigned char color)
{
    int len = 0;
    int x;

    while (text[len] != '\0') {
        len++;
    }

    x = (VIDEO_WIDTH - (len * 8)) / 2;
    v_puts(x, y, text, color);
}

int main(void)
{
    unsigned long start;

    sound_init();
    options_init();
    records_init();
    kb_init();

    v_init_mode13();
    v_lock_palette("palette.dat");
    v_clear(0);
    draw_center_text(text_get(TEXT_PROGRAMMED_BY), 96, 15);
    v_present();

    start = t_now_ms();
    while ((t_now_ms() - start) < 2000UL) {
        if (in_keyhit()) {
            break;
        }
    }
    while (in_keyhit()) {
        in_poll();
    }

    while (1) {
        MenuOption choice = menu_run();
        int selected_year;

        if (choice == MENU_SALIR) {
            break;
        }

        if (choice == MENU_HISTORIA) {
            StoryMode_Run();
        } else if (choice == MENU_EXTRA) {
            selected_year = select_year_run();
            if (selected_year != 0) {
                launch_year_game(selected_year, LAUNCH_MODE_EXTRA);
            }
        } else if (choice == MENU_HIGH_SCORES) {
            const GameOptions *options = options_get();
            StoryHighScores_Run(options ? options->difficulty : DIFFICULTY_NORMAL);
        } else if (choice == MENU_OPCIONES) {
            options_menu_run();
        }
    }

    kb_shutdown();
    sound_shutdown();
    v_text_mode();
    return 0;
}
