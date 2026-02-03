#include <dos.h>
#include <conio.h>
#include "input.h"
#include "keyboard.h"

// Estado de teclas
static volatile unsigned char g_keys[128];

// Cola de eventos
#define KB_QSIZE 64  // Tamaño potencia de 2
static volatile int g_q[KB_QSIZE];
static volatile unsigned char g_qhead = 0;
static volatile unsigned char g_qtail = 0;

// Prefijo E0 pendiente
static volatile unsigned char g_e0 = 0;

// Vector anterior de INT 9
static void (interrupt far *old_int9)();

static void kb_qpush(int k)
{
    unsigned char next = (unsigned char)((g_qhead + 1) & (KB_QSIZE - 1));
    if (next != g_qtail) {
        g_q[g_qhead] = k;
        g_qhead = next;
    }
}

int kb_keyhit(void)
{
    return g_qhead != g_qtail;
}

int kb_poll(void)
{
    int k;
    if (g_qhead == g_qtail) return IN_KEY_NONE;
    k = g_q[g_qtail];
    g_qtail = (unsigned char)((g_qtail + 1) & (KB_QSIZE - 1));
    return k;
}

int kb_any_down(void)
{
    int i;
    for (i = 0; i < 128; ++i) {
        if (g_keys[i]) {
            return 1;
        }
    }
    return 0;
}

int kb_down(unsigned char sc)
{
    if (sc >= 128) return 0;
    return g_keys[sc] != 0;
}

// Traduce scancode a tecla lógica para menús
static int kb_translate_make(unsigned char sc, unsigned char e0)
{
    // Extendidas E0
    if (e0) {
        switch (sc) {
            case SC_UP:    return IN_KEY_UP;
            case SC_DOWN:  return IN_KEY_DOWN;
            case SC_LEFT:  return IN_KEY_LEFT;
            case SC_RIGHT: return IN_KEY_RIGHT;
            case SC_ENTER: return IN_KEY_ENTER; // Enter del keypad
            default: break;
        }
    }

    switch (sc) {
        case SC_ESC:       return IN_KEY_ESC;
        case SC_ENTER:     return IN_KEY_ENTER;
        case SC_SPACE:     return IN_KEY_SPACE;
        default: break;
    }

    return IN_KEY_NONE;
}

static void interrupt far kb_int9()
{
    unsigned char sc = inp(0x60);

    // Prefijo E0
    if (sc == 0xE0) {
        g_e0 = 1;
        // OJO: ACK + EOI siempre para no bloquear el teclado
        {
            unsigned char a = inp(0x61);
            outp(0x61, (unsigned char)(a | 0x80));
            outp(0x61, a);
        }
        outp(0x20, 0x20);
        return;
    }

    {
        unsigned char is_break = (unsigned char)(sc & 0x80);
        unsigned char code = (unsigned char)(sc & 0x7F);
        unsigned char e0 = g_e0;
        g_e0 = 0;

        if (code < 128) g_keys[code] = is_break ? 0 : 1;

        // Solo en make para la cola
        if (!is_break) {
            int k = kb_translate_make(code, e0);
            if (k != IN_KEY_NONE) kb_qpush(k);
        }

        // ACK teclado
        {
            unsigned char a = inp(0x61);
            outp(0x61, (unsigned char)(a | 0x80));
            outp(0x61, a);
        }

        // EOI PIC
        outp(0x20, 0x20);
    }
}

void kb_init(void)
{
    int i;
    for (i = 0; i < 128; ++i) g_keys[i] = 0;
    g_qhead = g_qtail = 0;
    g_e0 = 0;

    old_int9 = _dos_getvect(0x09);
    _dos_setvect(0x09, kb_int9);
}

void kb_shutdown(void)
{
    if (old_int9) _dos_setvect(0x09, old_int9);
}
