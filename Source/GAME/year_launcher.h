#ifndef YEAR_LAUNCHER_H
#define YEAR_LAUNCHER_H

#include <stdint.h>

typedef enum {
    LAUNCH_MODE_HISTORIA = 0,
    LAUNCH_MODE_EXTRA = 1,
    LAUNCH_MODE_DEBUG = 2
} LaunchMode;

void launch_year_game(int year, LaunchMode mode);
int launch_year_game_story(int year, uint64_t *out_score, uint32_t *out_retries);

#endif
