#ifndef GAME_END_SCREEN_H
#define GAME_END_SCREEN_H

#include "../CORE/high_scores.h"

typedef enum {
    GAME_END_WIN = 0,
    GAME_END_LOSE = 1
} GameEndResult;

void Game_ShowEndScreen(GameEndResult result, const char *detail, int sound_enabled,
                        HighScoreGame game, int year, unsigned char difficulty, uint64_t score);
void Game_PlayLoseMelody(int sound_enabled);

#endif
