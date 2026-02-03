#ifndef GORI_H
#define GORI_H

#include "../game_settings.h"
#include <stdint.h>

void Gori_Init(const GameSettings *settings);
void Gori_StorePreviousState(void);
void Gori_Update(void);
void Gori_DrawInterpolated(float alpha);
void Gori_End(void);
int Gori_IsFinished(void);
int Gori_DidWin(void);
const char *Gori_GetEndDetail(void);
uint64_t Gori_GetScore(void);

#endif
