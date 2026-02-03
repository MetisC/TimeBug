#include "end_screen.h"

#include "../CORE/input.h"
#include "../CORE/sound.h"
#include "../CORE/timer.h"
#include "../CORE/video.h"
#include "../CORE/colors.h"
#include "../CORE/keyboard.h"
#include "../CORE/high_scores.h"

#include <stdint.h>
#include <string.h>

static const SoundNote g_end_win_melody[] = {
    { 523, 120 },
    { 659, 120 },
    { 784, 180 }
};

static const SoundNote g_end_lose_melody[] = {
    { 392, 140 },
    { 330, 140 },
    { 262, 220 }
};

static void draw_center_text(const char *text, int y, unsigned char color)
{
    int len = 0;
    int x;

    while (text && text[len] != '\0') {
        len++;
    }

    x = (VIDEO_WIDTH - (len * 8)) / 2;
    v_puts(x, y, text, color);
}

#define SCORE_CHAR_COUNT 37
#define SCORE_CHAR_END SCORE_CHAR_COUNT

static const char g_score_chars[SCORE_CHAR_COUNT + 1] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ";

static const unsigned char g_direct_scancodes[] = {
    SC_A, SC_B, SC_C, SC_D, SC_E, SC_F, SC_G, SC_H, SC_I, SC_J, SC_K, SC_L, SC_M,
    SC_N, SC_O, SC_P, SC_Q, SC_R, SC_S, SC_T, SC_U, SC_V, SC_W, SC_X, SC_Y, SC_Z,
    SC_0, SC_1, SC_2, SC_3, SC_4, SC_5, SC_6, SC_7, SC_8, SC_9
};

static const char g_direct_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

static void draw_slot_text(int x, int y, int index, unsigned char color)
{
    char label[4] = " ";
    int label_len = 1;

    if (index == SCORE_CHAR_END) {
        strcpy(label, "END");
        label_len = 3;
    } else {
        label[0] = g_score_chars[index];
        label[1] = '\0';
    }

    v_puts(x + (24 - (label_len * 8)) / 2, y, label, color);
}

static void draw_record_screen(const char *message, const char *detail,
                               const int indices[3], int position)
{
    int base_x = (VIDEO_WIDTH - (24 * 4)) / 2;
    int y = 150;
    int i;

    v_clear(0);
    draw_center_text(message, 80, 15);
    if (detail && detail[0] != '\0') {
        draw_center_text(detail, 100, 7);
    }
    draw_center_text("RECORD", 130, 15);

    for (i = 0; i < 3; ++i) {
        unsigned char color = (position == i) ? COL_TEXT_SELECTED : COL_TEXT_NORMAL;
        draw_slot_text(base_x + (i * 24), y, indices[i], color);
    }

    {
        unsigned char color = (position == 3) ? COL_TEXT_SELECTED : COL_TEXT_NORMAL;
        v_puts(base_x + (3 * 24), y, "END", color);
    }

    v_present();
}

static char poll_direct_char(unsigned char *prev_state)
{
    int i;
    int count = (int)(sizeof(g_direct_scancodes) / sizeof(g_direct_scancodes[0]));

    for (i = 0; i < count; ++i) {
        int down = kb_down(g_direct_scancodes[i]);
        if (down && !prev_state[i]) {
            prev_state[i] = 1;
            return g_direct_chars[i];
        }
        prev_state[i] = (unsigned char)down;
    }

    return '\0';
}

static int score_char_index(char c)
{
    int i;

    for (i = 0; i < SCORE_CHAR_COUNT; ++i) {
        if (g_score_chars[i] == c) {
            return i;
        }
    }

    return -1;
}

static void high_scores_capture_initials(HighScoreGame game, int year, unsigned char difficulty,
                                         const char *detail, int sound_enabled, uint64_t score)
{
    HighScoreTable table;
    int indices[3] = {0, 0, 0};
    int position = 0;
    unsigned char prev_state[sizeof(g_direct_scancodes) / sizeof(g_direct_scancodes[0])] = {0};
    int done = 0;
    int i;

    (void)sound_enabled;

    high_scores_load(game, year, difficulty, &table);

    in_clear();
    draw_record_screen("HAS GANADO", detail, indices, position);

    while (!done) {
        int key = in_poll();
        char direct = poll_direct_char(prev_state);

        if (direct && position < 3) {
            int idx = score_char_index(direct);
            if (idx >= 0) {
                indices[position] = idx;
                draw_record_screen("HAS GANADO", detail, indices, position);
            }
        }

        if (key == IN_KEY_UP && position < 3) {
            if (indices[position] == 0) {
                indices[position] = SCORE_CHAR_END;
            } else {
                indices[position]--;
            }
            draw_record_screen("HAS GANADO", detail, indices, position);
        } else if (key == IN_KEY_DOWN && position < 3) {
            if (indices[position] == SCORE_CHAR_END) {
                indices[position] = 0;
            } else {
                indices[position]++;
            }
            draw_record_screen("HAS GANADO", detail, indices, position);
        } else if (key == IN_KEY_LEFT) {
            position = (position + 3) % 4;
            draw_record_screen("HAS GANADO", detail, indices, position);
        } else if (key == IN_KEY_RIGHT) {
            position = (position + 1) % 4;
            draw_record_screen("HAS GANADO", detail, indices, position);
        } else if (key == IN_KEY_ENTER) {
            if (position == 3) {
                done = 1;
            } else if (indices[position] == SCORE_CHAR_END) {
                if (position == 0 && high_scores_is_empty(&table)) {
                    char initials[HIGH_SCORES_INITIALS_LEN + 1] = "YOU";
                    high_scores_insert(&table, score, initials);
                    high_scores_save(game, year, difficulty, &table);
                    return;
                }
                indices[position] = SCORE_CHAR_END;
                position = 3;
                draw_record_screen("HAS GANADO", detail, indices, position);
            } else {
                if (position < 2) {
                    position++;
                } else {
                    position = 3;
                }
                draw_record_screen("HAS GANADO", detail, indices, position);
            }
        }
    }

    {
        char initials[HIGH_SCORES_INITIALS_LEN + 1] = "   ";
        for (i = 0; i < HIGH_SCORES_INITIALS_LEN; ++i) {
            if (indices[i] == SCORE_CHAR_END) {
                initials[i] = ' ';
            } else {
                initials[i] = g_score_chars[indices[i]];
            }
        }
        initials[HIGH_SCORES_INITIALS_LEN] = '\0';
        high_scores_insert(&table, score, initials);
        high_scores_save(game, year, difficulty, &table);
    }
}

void Game_ShowEndScreen(GameEndResult result, const char *detail, int sound_enabled,
                        HighScoreGame game, int year, unsigned char difficulty, uint64_t score)
{
    const char *message = (result == GAME_END_WIN) ? "HAS GANADO" : "HAS PERDIDO";
    uint32_t start;
    int wants_record = 0;
    HighScoreTable table;

    if (result == GAME_END_WIN && game != HIGH_SCORE_GAME_NONE) {
        high_scores_load(game, year, difficulty, &table);
        wants_record = high_scores_qualifies(&table, score);
    }

    v_clear(0);
    draw_center_text(message, wants_record ? 80 : 90, 15);
    if (detail && detail[0] != '\0') {
        draw_center_text(detail, wants_record ? 100 : 110, 7);
    }
    if (wants_record) {
        draw_center_text("RECORD", 130, 15);
    }
    v_present();

    if (sound_enabled) {
        if (result == GAME_END_WIN) {
            sound_play_melody(g_end_win_melody, (int)(sizeof(g_end_win_melody) / sizeof(g_end_win_melody[0])));
        } else {
            sound_play_melody(g_end_lose_melody, (int)(sizeof(g_end_lose_melody) / sizeof(g_end_lose_melody[0])));
        }
    }

    if (wants_record) {
        high_scores_capture_initials(game, year, difficulty, detail, sound_enabled, score);
        return;
    }

    in_clear();
    start = timer_now_us();
    while ((timer_now_us() - start) < 2000000UL) {
        sound_update();
        if (in_keyhit()) {
            in_clear();
        }
    }

    in_clear();
    while (in_any_down()) {
        sound_update();
    }
    in_clear();
    for (;;) {
        int key = in_poll();
        if (key == IN_KEY_ENTER) {
            break;
        }
    }
    in_clear();
}

void Game_PlayLoseMelody(int sound_enabled)
{
    if (!sound_enabled) {
        return;
    }

    sound_play_melody(g_end_lose_melody, (int)(sizeof(g_end_lose_melody) / sizeof(g_end_lose_melody[0])));
}
