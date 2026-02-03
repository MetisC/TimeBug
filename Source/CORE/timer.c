#include "timer.h"

#include <conio.h>
#include <dos.h>

#define PIT_PORT_DATA 0x40
#define PIT_PORT_CTRL 0x43
#define PIT_CTRL_LATCH 0x00
#define PIT_BASE_FREQ 1193182UL

uint32_t timer_now_us(void)
{
    const unsigned long far *bios_ticks = (unsigned long far *)MK_FP(0x40, 0x6C);
    unsigned long tick_before;
    unsigned long tick_after;
    unsigned int pit_count;

    do {
        tick_before = *bios_ticks;
        outp(PIT_PORT_CTRL, PIT_CTRL_LATCH);
        pit_count = (unsigned int)inp(PIT_PORT_DATA);
        pit_count |= (unsigned int)inp(PIT_PORT_DATA) << 8;
        tick_after = *bios_ticks;
    } while (tick_before != tick_after);

    {
        unsigned int pit_elapsed = (unsigned int)(0x10000U - pit_count);
        uint64_t total_ticks = ((uint64_t)tick_before << 16) + pit_elapsed;
        uint64_t us = (total_ticks * 1000000ULL) / PIT_BASE_FREQ;
        return (uint32_t)us;
    }
}

unsigned long t_now_ms(void)
{
    return (unsigned long)(timer_now_us() / 1000UL);
}

void t_wait_ms(unsigned long ms)
{
    unsigned long start = t_now_ms();

    while ((t_now_ms() - start) < ms) {
        // Espera activa en DOS, suficiente aquí
    }
}

void t_wait_us(uint32_t us)
{
    // Espera BIOS: INT 15h, AH=86h, CX:DX en microsegundos
    union REGS r;
    r.h.ah = 0x86;
    r.x.cx = (unsigned int)(us >> 16);
    r.x.dx = (unsigned int)(us & 0xFFFF);

    int86(0x15, &r, &r);

    // OJO: si CF=1, la BIOS no soporta o falla
    if (r.x.cflag) {
        uint32_t start = timer_now_us();
        while ((uint32_t)(timer_now_us() - start) < us) {
        }
    }
}
