#ifndef FLAPPY_H
#define FLAPPY_H

#include "../game_settings.h"
#include <stdint.h>

void Flappy_Init(const GameSettings *settings);
void Flappy_StorePreviousState(void);
void Flappy_Update(void);
void Flappy_DrawInterpolated(float alpha);
void Flappy_End(void);
int Flappy_IsFinished(void);
int Flappy_DidWin(void);
const char *Flappy_GetEndDetail(void);
uint64_t Flappy_GetScore(void);

#endif
