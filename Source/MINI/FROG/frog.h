#ifndef FROG_H
#define FROG_H

#include "../game_settings.h"
#include <stdint.h>

void Frog_Init(const GameSettings *settings);
void Frog_StorePreviousState(void);
void Frog_Update(void);
void Frog_DrawInterpolated(float alpha);
void Frog_End(void);
int Frog_IsFinished(void);
int Frog_DidWin(void);
const char *Frog_GetEndDetail(void);
uint64_t Frog_GetScore(void);

#endif
