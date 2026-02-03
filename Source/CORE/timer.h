#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

uint32_t timer_now_us(void);
unsigned long t_now_ms(void);
void t_wait_ms(unsigned long ms);
void t_wait_us(uint32_t us);

#endif
