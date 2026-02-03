#include "options_menu.h"

#include "../CORE/input.h"
#include "../CORE/options.h"
#include "../CORE/records.h"
#include "../CORE/text.h"
#include "../CORE/timer.h"
#include "../CORE/video.h"
#include "../CORE/colors.h"

#include <stdio.h>
#include <string.h>

typedef enum {
    OPTIONS_ITEM_DIFFICULTY = 0,
    OPTIONS_ITEM_SOUND,
    OPTIONS_ITEM_SPEED,
    OPTIONS_ITEM_INPUT,
    OPTIONS_ITEM_BACK,
    OPTIONS_ITEM_COUNT
} OptionsItem;

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

static const char *difficulty_label(unsigned char difficulty)
{
    switch (difficulty) {
    case DIFFICULTY_EASY:
        return text_get(TEXT_DIFFICULTY_EASY);
    case DIFFICULTY_HARD:
        return text_get(TEXT_DIFFICULTY_HARD);
    case DIFFICULTY_NORMAL:
    default:
        return text_get(TEXT_DIFFICULTY_NORMAL);
    }
}

static const char *sound_label(unsigned char sound_enabled)
{
    if (sound_enabled) {
        return text_get(TEXT_SOUND_ON);
    }
    return text_get(TEXT_SOUND_OFF);
}

static const char *speed_label(unsigned char speed)
{
    if (speed == GAME_SPEED_TURBO) {
        return text_get(TEXT_SPEED_TURBO);
    }
    return text_get(TEXT_SPEED_NORMAL);
}

static const char *input_label(unsigned char input_mode)
{
    if (input_mode == INPUT_JOYSTICK) {
        return text_get(TEXT_INPUT_JOYSTICK);
    }
    return text_get(TEXT_INPUT_KEYBOARD);
}

static void options_draw(int selected, const char *status_text)
{
    const GameOptions *options = options_get();
    int start_y = 70;
    int gap = 14;
    int y;
    int x;
    char line[64];

    v_clear(0);
    draw_center_text(text_get(TEXT_TITLE_OPTIONS), 24, 15);

    snprintf(line, sizeof(line), "%s: %s", text_get(TEXT_OPTIONS_DIFFICULTY), difficulty_label(options->difficulty));
    y = start_y + (OPTIONS_ITEM_DIFFICULTY * gap);
    x = (VIDEO_WIDTH - ((int)strlen(line) * 8)) / 2;
    v_puts(x, y, line, (selected == OPTIONS_ITEM_DIFFICULTY) ? COL_TEXT_SELECTED : COL_TEXT_NORMAL);

    snprintf(line, sizeof(line), "%s: %s", text_get(TEXT_OPTIONS_SOUND), sound_label(options->sound_enabled));
    y = start_y + (OPTIONS_ITEM_SOUND * gap);
    x = (VIDEO_WIDTH - ((int)strlen(line) * 8)) / 2;
    v_puts(x, y, line, (selected == OPTIONS_ITEM_SOUND) ? COL_TEXT_SELECTED : COL_TEXT_NORMAL);

    snprintf(line, sizeof(line), "%s: %s", text_get(TEXT_OPTIONS_SPEED), speed_label(options->game_speed));
    y = start_y + (OPTIONS_ITEM_SPEED * gap);
    x = (VIDEO_WIDTH - ((int)strlen(line) * 8)) / 2;
    v_puts(x, y, line, (selected == OPTIONS_ITEM_SPEED) ? COL_TEXT_SELECTED : COL_TEXT_NORMAL);

    snprintf(line, sizeof(line), "%s: %s", text_get(TEXT_OPTIONS_INPUT), input_label(options->input_mode));
    y = start_y + (OPTIONS_ITEM_INPUT * gap);
    x = (VIDEO_WIDTH - ((int)strlen(line) * 8)) / 2;
    v_puts(x, y, line, (selected == OPTIONS_ITEM_INPUT) ? COL_TEXT_SELECTED : COL_TEXT_NORMAL);

    y = start_y + (OPTIONS_ITEM_BACK * gap);
    x = (VIDEO_WIDTH - ((int)strlen(text_get(TEXT_OPTIONS_BACK)) * 8)) / 2;
    v_puts(x, y, text_get(TEXT_OPTIONS_BACK),
           (selected == OPTIONS_ITEM_BACK) ? COL_TEXT_SELECTED : COL_TEXT_NORMAL);

    draw_center_text(text_get(TEXT_OPTIONS_FOOTER), 180, 8);

    if (status_text && status_text[0] != '\0') {
        draw_center_text(status_text, 8, COL_TEXT_ERROR);
    }

    v_present();
}

static void adjust_difficulty(int direction)
{
    const GameOptions *options = options_get();
    int value = options->difficulty;

    if (direction < 0) {
        value--;
        if (value < DIFFICULTY_EASY) {
            value = DIFFICULTY_HARD;
        }
    } else {
        value++;
        if (value > DIFFICULTY_HARD) {
            value = DIFFICULTY_EASY;
        }
    }

    options_set_difficulty((unsigned char)value);
    records_set_difficulty((unsigned char)value);
}

static void adjust_sound(void)
{
    const GameOptions *options = options_get();
    options_set_sound_enabled(options->sound_enabled ? 0 : 1);
}

static void adjust_speed(int direction)
{
    const GameOptions *options = options_get();
    int value = options->game_speed;

    if (direction < 0) {
        value--;
        if (value < GAME_SPEED_NORMAL) {
            value = GAME_SPEED_TURBO;
        }
    } else {
        value++;
        if (value > GAME_SPEED_TURBO) {
            value = GAME_SPEED_NORMAL;
        }
    }

    options_set_game_speed((unsigned char)value);
}

static int adjust_input(int direction, const char **status_text, unsigned long *status_until)
{
    const GameOptions *options = options_get();
    int value = options->input_mode;

    if (direction < 0) {
        value--;
        if (value < INPUT_KEYBOARD) {
            value = INPUT_JOYSTICK;
        }
    } else {
        value++;
        if (value > INPUT_JOYSTICK) {
            value = INPUT_KEYBOARD;
        }
    }

    if (value == INPUT_JOYSTICK && !in_joystick_available()) {
        options_set_input_mode(INPUT_KEYBOARD);
        *status_text = text_get(TEXT_JOYSTICK_NOT_FOUND);
        *status_until = t_now_ms() + 1500UL;
        return 0;
    }

    options_set_input_mode((unsigned char)value);
    return 1;
}

void options_menu_run(void)
{
    int selected = 0;
    int key;
    const char *status_text = NULL;
    unsigned long status_until = 0;

    options_draw(selected, status_text);

    while (1) {
        if (status_text && t_now_ms() > status_until) {
            status_text = NULL;
            options_draw(selected, status_text);
        }

        key = in_poll();
        if (key == IN_KEY_NONE) {
            continue;
        }

        if (key == IN_KEY_UP) {
            if (selected > 0) {
                selected--;
                options_draw(selected, status_text);
            }
        } else if (key == IN_KEY_DOWN) {
            if (selected < OPTIONS_ITEM_COUNT - 1) {
                selected++;
                options_draw(selected, status_text);
            }
        } else if (key == IN_KEY_LEFT || key == IN_KEY_RIGHT) {
            int direction = (key == IN_KEY_LEFT) ? -1 : 1;

            if (selected == OPTIONS_ITEM_DIFFICULTY) {
                adjust_difficulty(direction);
                options_draw(selected, status_text);
            } else if (selected == OPTIONS_ITEM_SOUND) {
                adjust_sound();
                options_draw(selected, status_text);
            } else if (selected == OPTIONS_ITEM_SPEED) {
                adjust_speed(direction);
                options_draw(selected, status_text);
            } else if (selected == OPTIONS_ITEM_INPUT) {
                adjust_input(direction, &status_text, &status_until);
                options_draw(selected, status_text);
            }
        } else if (key == IN_KEY_ENTER) {
            if (selected == OPTIONS_ITEM_BACK) {
                break;
            }
        } else if (key == IN_KEY_ESC) {
            break;
        }
    }

    if (!options_save_if_dirty()) {
        status_text = text_get(TEXT_OPTIONS_SAVE_FAILED);
        options_draw(selected, status_text);
        t_wait_ms(1500UL);
    }
}
