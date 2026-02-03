#ifndef PONG_H
#define PONG_H

#include "../game_settings.h"
#include <stdint.h>

void Pong_Init(const GameSettings *settings);
void Pong_StorePreviousState(void);
void Pong_Update(void);
void Pong_DrawInterpolated(float alpha);
void Pong_End(void);
int Pong_IsFinished(void);
int Pong_DidWin(void);
const char *Pong_GetEndDetail(void);
uint64_t Pong_GetScore(void);

#endif
