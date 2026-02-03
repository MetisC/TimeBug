#ifndef TRON_H
#define TRON_H

#include "../game_settings.h"
#include <stdint.h>

void Tron_Init(const GameSettings *settings);
void Tron_StorePreviousState(void);
void Tron_Update(void);
void Tron_DrawInterpolated(float alpha);
void Tron_End(void);
int Tron_IsFinished(void);
int Tron_DidWin(void);
const char *Tron_GetEndDetail(void);
uint64_t Tron_GetScore(void);

#endif
