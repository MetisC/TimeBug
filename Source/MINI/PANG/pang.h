#ifndef PANG_H
#define PANG_H

#include "../game_settings.h"
#include <stdint.h>

void Pang_Init(const GameSettings *settings);
void Pang_StorePreviousState(void);
void Pang_Update(void);
void Pang_DrawInterpolated(float alpha);
void Pang_End(void);
int Pang_IsFinished(void);
int Pang_DidWin(void);
const char *Pang_GetEndDetail(void);
uint64_t Pang_GetScore(void);

#endif
