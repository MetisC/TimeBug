#include "select_year.h"

#include "../CORE/input.h"
#include "../CORE/options.h"
#include "../CORE/video.h"
#include "../CORE/text.h"
#include "../CORE/colors.h"
#include "../CORE/high_scores.h"

#include <stdio.h>

static const int year_options[] = {1972, 1978, 1979, 1981, 1982, 1983, 1989, 1991, 2013};
#define COLUMN_WIDTH (VIDEO_WIDTH / 2)
#define COLUMN_LEFT_X 0
#define COLUMN_RIGHT_X (VIDEO_WIDTH / 2)

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

static void draw_column_center_text(const char *text, int y, unsigned char color, int column_x)
{
    int len = 0;
    int x;

    while (text[len] != '\0') {
        len++;
    }

    x = column_x + (COLUMN_WIDTH - (len * 8)) / 2;
    v_puts(x, y, text, color);
}

static void select_year_draw(int selected)
{
    int i;
    int count = (int)(sizeof(year_options) / sizeof(year_options[0]));
    int start_y = 54;
    int gap = 12;
    int y;
    char year_text[8];
    HighScoreTable table;
    HighScoreGame game;
    const GameOptions *options = options_get();

    v_clear(0);

    draw_column_center_text("HIGH SCORES", 32, 15, COLUMN_RIGHT_X);

    for (i = 0; i < count; ++i) {
        unsigned char color = (i == selected)
            ? COL_TEXT_SELECTED
            : COL_TEXT_NORMAL;
        int year = year_options[i];

        year_text[0] = '\0';
        snprintf(year_text, sizeof(year_text), "%d", year);
        y = start_y + (i * gap);
        draw_column_center_text(year_text, y, color, COLUMN_LEFT_X);
    }

    game = high_scores_game_for_year(year_options[selected]);
    high_scores_load(game, year_options[selected], options->difficulty, &table);
    if (table.count > 0) {
        int line_y = start_y;
        int line_x = COLUMN_RIGHT_X + 8;
        for (i = 0; i < table.count; ++i) {
            char score_text[16];
            char line[32];
            high_scores_format_score(score_text, sizeof(score_text), table.entries[i].score);
            snprintf(line, sizeof(line), "%s - %.3s", score_text, table.entries[i].initials);
            v_puts(line_x, line_y, line, COL_TEXT_NORMAL);
            line_y += gap;
        }
    }

    draw_center_text(text_get(TEXT_EXTRA_FOOTER), 180, 8);

    v_present();
}

int select_year_run(void)
{
    int selected = 0;
    int count = (int)(sizeof(year_options) / sizeof(year_options[0]));
    int key;

    select_year_draw(selected);

    while (1) {
        key = in_poll();
        if (key == IN_KEY_NONE) {
            continue;
        }

        if (key == IN_KEY_UP) {
            if (selected > 0) {
                selected--;
                select_year_draw(selected);
            }
        } else if (key == IN_KEY_DOWN) {
            if (selected < count - 1) {
                selected++;
                select_year_draw(selected);
            }
        } else if (key == IN_KEY_ENTER) {
            return year_options[selected];
        } else if (key == IN_KEY_ESC) {
            return 0;
        }
    }
}
