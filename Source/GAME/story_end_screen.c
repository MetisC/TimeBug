#include "story_end_screen.h"

#include "../CORE/colors.h"
#include "../CORE/high_scores.h"
#include "../CORE/input.h"
#include "../CORE/keyboard.h"
#include "../CORE/video.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

static void draw_record_screen(const char *message, const int indices[3], int position)
{
    int base_x = (VIDEO_WIDTH - (24 * 4)) / 2;
    int y = 150;
    int i;

    v_clear(0);
    draw_center_text(message, 80, 15);
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

static void story_capture_initials(unsigned char difficulty, uint64_t score)
{
    HighScoreTable table;
    int indices[3] = {0, 0, 0};
    int position = 0;
    unsigned char prev_state[sizeof(g_direct_scancodes) / sizeof(g_direct_scancodes[0])] = {0};
    int done = 0;
    int i;

    high_scores_load_story(difficulty, &table);

    in_clear();
    draw_record_screen("MODO HISTORIA COMPLETADO", indices, position);

    while (!done) {
        int key = in_poll();
        char direct = poll_direct_char(prev_state);

        if (direct && position < 3) {
            int idx = score_char_index(direct);
            if (idx >= 0) {
                indices[position] = idx;
                draw_record_screen("MODO HISTORIA COMPLETADO", indices, position);
            }
        }

        if (key == IN_KEY_UP && position < 3) {
            if (indices[position] == 0) {
                indices[position] = SCORE_CHAR_END;
            } else {
                indices[position]--;
            }
            draw_record_screen("MODO HISTORIA COMPLETADO", indices, position);
        } else if (key == IN_KEY_DOWN && position < 3) {
            if (indices[position] == SCORE_CHAR_END) {
                indices[position] = 0;
            } else {
                indices[position]++;
            }
            draw_record_screen("MODO HISTORIA COMPLETADO", indices, position);
        } else if (key == IN_KEY_LEFT) {
            position = (position + 3) % 4;
            draw_record_screen("MODO HISTORIA COMPLETADO", indices, position);
        } else if (key == IN_KEY_RIGHT) {
            position = (position + 1) % 4;
            draw_record_screen("MODO HISTORIA COMPLETADO", indices, position);
        } else if (key == IN_KEY_ENTER) {
            if (position == 3) {
                done = 1;
            } else if (indices[position] == SCORE_CHAR_END) {
                if (position == 0 && high_scores_is_empty(&table)) {
                    char initials[HIGH_SCORES_INITIALS_LEN + 1] = "YOU";
                    high_scores_insert(&table, score, initials);
                    high_scores_save_story(difficulty, &table);
                    return;
                }
                indices[position] = SCORE_CHAR_END;
                position = 3;
                draw_record_screen("MODO HISTORIA COMPLETADO", indices, position);
            } else {
                if (position < 2) {
                    position++;
                } else {
                    position = 3;
                }
                draw_record_screen("MODO HISTORIA COMPLETADO", indices, position);
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
        high_scores_save_story(difficulty, &table);
    }
}

static uint64_t story_bonus_for_retries(uint32_t retries)
{
    if (retries == 0) return 5000000ULL;
    if (retries <= 10) return 4000000ULL;
    if (retries <= 20) return 3200000ULL;
    if (retries <= 30) return 2600000ULL;
    if (retries <= 40) return 2100000ULL;
    if (retries <= 50) return 1700000ULL;
    if (retries <= 60) return 1300000ULL;
    if (retries <= 70) return 900000ULL;
    if (retries <= 80) return 600000ULL;
    if (retries <= 90) return 350000ULL;
    if (retries <= 100) return 150000ULL;
    return 12345ULL;
}

void Story_ShowFinalScore(uint64_t base_score, uint32_t retries, unsigned char difficulty, int sound_enabled)
{
    HighScoreTable table;
    uint64_t bonus = story_bonus_for_retries(retries);
    uint64_t final_score = base_score + bonus;
    int wants_record = 0;
    char buf[64];

    (void)sound_enabled;

    v_clear(0);
    draw_center_text("MODO HISTORIA COMPLETADO", 32, 15);

    snprintf(buf, sizeof(buf), "SCORE BASE: %llu", (unsigned long long)base_score);
    v_puts(32, 80, buf, 7);
    snprintf(buf, sizeof(buf), "REINICIOS: %lu", (unsigned long)retries);
    v_puts(32, 92, buf, 7);
    snprintf(buf, sizeof(buf), "BONUS: %llu", (unsigned long long)bonus);
    v_puts(32, 104, buf, 7);
    snprintf(buf, sizeof(buf), "SCORE FINAL: %llu", (unsigned long long)final_score);
    v_puts(32, 120, buf, 15);
    draw_center_text("PULSA ENTER", 160, 15);
    v_present();

    in_clear();
    while (1) {
        int key = in_poll();
        if (key == IN_KEY_ENTER || key == IN_KEY_ESC) {
            break;
        }
    }

    high_scores_load_story(difficulty, &table);
    wants_record = high_scores_qualifies(&table, final_score);

    if (wants_record) {
        story_capture_initials(difficulty, final_score);
        return;
    }

    v_clear(0);
    draw_center_text("NO ENTRA EN TOP 10", 100, 15);
    v_present();
    in_clear();
    while (1) {
        int key = in_poll();
        if (key == IN_KEY_ENTER || key == IN_KEY_ESC) {
            break;
        }
    }
}
