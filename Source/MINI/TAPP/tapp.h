#ifndef TAPP_H
#define TAPP_H

#include "../game_settings.h"
#include <stdint.h>

void Tapp_Init(const GameSettings *settings);
void Tapp_StorePreviousState(void);
void Tapp_Update(void);
void Tapp_DrawInterpolated(float alpha);
void Tapp_End(void);
int Tapp_IsFinished(void);
int Tapp_DidWin(void);
const char *Tapp_GetEndDetail(void);
uint64_t Tapp_GetScore(void);

#endif
