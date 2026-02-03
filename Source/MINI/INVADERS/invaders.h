#ifndef INVADERS_H
#define INVADERS_H

#include "../game_settings.h"
#include <stdint.h>

#define INVADER_ROWS 5

#define INV_COLOR_PLAYER_SHIP_BASE 18
#define INV_COLOR_PLAYER_SHIP_DOTS 15
#define INV_COLOR_PLAYER_TURRET 7
#define INV_COLOR_BULLET_PLAYER 14
#define INV_COLOR_BULLET_ENEMY 4
#define INV_COLOR_EXPLOSION_0 52
#define INV_COLOR_EXPLOSION_1 35
#define INV_COLOR_HUD 7
#define INV_COLOR_TIMER 15

static const unsigned char INV_COLOR_ALIEN_ROW[INVADER_ROWS] = {
    87, 86, 85, 84, 83
};

void Invaders_Init(const GameSettings *settings);
void Invaders_StorePreviousState(void);
void Invaders_Update(void);
void Invaders_DrawInterpolated(float alpha);
void Invaders_End(void);
int Invaders_IsFinished(void);
int Invaders_DidWin(void);
const char *Invaders_GetEndDetail(void);
uint64_t Invaders_GetScore(void);

#endif
