#include "story_mode.h"

#include "../CORE/options.h"
#include "cutscene.h"
#include "story_end_screen.h"
#include "year_launcher.h"

#include <stdint.h>

void StoryMode_Run(void)
{
    const GameOptions *options = options_get();
    unsigned char difficulty = options ? options->difficulty : DIFFICULTY_NORMAL;
    int sound_enabled = options ? options->sound_enabled : 0;
    uint64_t total_score = 0;
    uint32_t total_retries = 0;
    const int years[] = {1972, 1978, 1979, 1981, 1982, 1983, 1989, 1991, 2013};
    const char *pre_ids[] = {
        "pre1978",
        "pre1979",
        "pre1981",
        "pre1982",
        "pre1983",
        "pre1989",
        "pre1991",
        "pre2013"
    };
    int i;

    Cutscene_Play("intro");

    for (i = 0; i < (int)(sizeof(years) / sizeof(years[0])); ++i) {
        uint64_t score = 0;
        uint32_t retries = 0;

        if (!launch_year_game_story(years[i], &score, &retries)) {
            return;
        }

        total_score += score;
        total_retries += retries;

        if (i < (int)(sizeof(pre_ids) / sizeof(pre_ids[0]))) {
            Cutscene_Play(pre_ids[i]);
        }
    }

    Cutscene_Play("ending");
    Story_ShowFinalScore(total_score, total_retries, difficulty, sound_enabled);
}
