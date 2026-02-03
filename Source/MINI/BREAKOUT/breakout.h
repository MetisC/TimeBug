#ifndef BREAKOUT_H
#define BREAKOUT_H

#include "../game_settings.h"
#include <stdint.h>

#define BRK_COLOR_PLAYER_SHIP_BASE 18
#define BRK_COLOR_PLAYER_SHIP_DOTS 15
#define BRK_COLOR_PLAYER_TURRET 7
#define BRK_COLOR_HUD 7

void Breakout_Init(const GameSettings *settings);
void Breakout_StorePreviousState(void);
void Breakout_Update(void);
void Breakout_DrawInterpolated(float alpha);
void Breakout_End(void);
int Breakout_IsFinished(void);
int Breakout_DidWin(void);
const char *Breakout_GetEndDetail(void);
uint64_t Breakout_GetScore(void);

#endif
