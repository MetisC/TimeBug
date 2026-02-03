#include "story_high_scores.h"

#include "../CORE/high_scores.h"
#include "../CORE/input.h"
#include "../CORE/video.h"

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

void StoryHighScores_Run(unsigned char difficulty)
{
    HighScoreTable table;
    int i;

    high_scores_load_story(difficulty, &table);

    v_clear(0);
    draw_center_text("HIGH SCORES HISTORIA", 20, 15);

    for (i = 0; i < HIGH_SCORES_MAX_ENTRIES; ++i) {
        char line[64];
        char score_text[16];
        const char *initials = "---";

        if (i < table.count) {
            initials = table.entries[i].initials;
            high_scores_format_score(score_text, sizeof(score_text), table.entries[i].score);
        } else {
            strcpy(score_text, "-------");
        }

        snprintf(line, sizeof(line), "%2d. %s  %s", i + 1, initials, score_text);
        v_puts(64, 50 + (i * 12), line, 7);
    }

    draw_center_text("ENTER/ESC VOLVER", 185, 15);
    v_present();

    in_clear();
    while (1) {
        int key = in_poll();
        if (key == IN_KEY_ENTER || key == IN_KEY_ESC) {
            break;
        }
    }
}
