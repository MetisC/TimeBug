#include "sound.h"

#include "timer.h"

#include <conio.h>
#include <dos.h>

#define PIT_FREQ 1193182UL
#define SOUND_QUEUE_MAX 32

static SoundNote g_queue[SOUND_QUEUE_MAX];
static int g_queue_len = 0;
static int g_queue_pos = 0;
static unsigned long g_note_end_ms = 0;
static int g_playing = 0;
static int g_enabled = 1;
static SoundBackend g_backend = SOUND_BACKEND_PC_SPEAKER;

static void pc_speaker_stop(void)
{
    unsigned char value = inp(0x61);
    value &= (unsigned char)~0x03;
    outp(0x61, value);
}

static void pc_speaker_start(unsigned int freq)
{
    unsigned long divisor;
    unsigned char value;

    if (freq == 0) {
        pc_speaker_stop();
        return;
    }

    divisor = PIT_FREQ / (unsigned long)freq;
    if (divisor == 0) {
        divisor = 1;
    }

    outp(0x43, 0xB6);
    outp(0x42, (unsigned char)(divisor & 0xFF));
    outp(0x42, (unsigned char)((divisor >> 8) & 0xFF));

    value = inp(0x61);
    value |= 0x03;
    outp(0x61, value);
}

static void sound_backend_start(unsigned int freq)
{
    switch (g_backend) {
    case SOUND_BACKEND_PC_SPEAKER:
        pc_speaker_start(freq);
        break;
    case SOUND_BACKEND_NONE:
    default:
        pc_speaker_stop();
        break;
    }
}

static void sound_backend_stop(void)
{
    switch (g_backend) {
    case SOUND_BACKEND_PC_SPEAKER:
        pc_speaker_stop();
        break;
    case SOUND_BACKEND_NONE:
    default:
        pc_speaker_stop();
        break;
    }
}

void sound_init(void)
{
    g_queue_len = 0;
    g_queue_pos = 0;
    g_note_end_ms = 0;
    g_playing = 0;
    g_enabled = 1;
    g_backend = SOUND_BACKEND_PC_SPEAKER;
    sound_backend_stop();
}

void sound_shutdown(void)
{
    g_queue_len = 0;
    g_queue_pos = 0;
    g_note_end_ms = 0;
    g_playing = 0;
    sound_backend_stop();
}

void sound_set_enabled(int enabled)
{
    g_enabled = enabled ? 1 : 0;
    if (!g_enabled) {
        g_queue_len = 0;
        g_queue_pos = 0;
        g_playing = 0;
        sound_backend_stop();
    }
}

void sound_set_backend(SoundBackend backend)
{
    g_backend = backend;
    sound_backend_stop();
    g_playing = 0;
    g_queue_len = 0;
    g_queue_pos = 0;
}

static void sound_start_note(const SoundNote *note)
{
    if (!note) {
        return;
    }

    sound_backend_start(note->freq);
    g_note_end_ms = t_now_ms() + (unsigned long)note->duration_ms;
    g_playing = 1;
}

void sound_update(void)
{
    if (!g_enabled) {
        return;
    }

    if (g_queue_len <= 0) {
        if (g_playing) {
            sound_backend_stop();
            g_playing = 0;
        }
        return;
    }

    if (!g_playing) {
        g_queue_pos = 0;
        sound_start_note(&g_queue[g_queue_pos]);
        return;
    }

    if (t_now_ms() >= g_note_end_ms) {
        g_queue_pos++;
        if (g_queue_pos >= g_queue_len) {
            g_queue_len = 0;
            g_queue_pos = 0;
            g_playing = 0;
            sound_backend_stop();
            return;
        }
        sound_start_note(&g_queue[g_queue_pos]);
    }
}

void sound_play_tone(unsigned int freq, unsigned int duration_ms)
{
    SoundNote note;

    if (!g_enabled) {
        return;
    }

    if (g_queue_len >= SOUND_QUEUE_MAX) {
        return;
    }

    note.freq = freq;
    note.duration_ms = duration_ms;

    if (g_queue_len == 0) {
        g_queue[0] = note;
        g_queue_len = 1;
        g_queue_pos = 0;
        g_playing = 0;
        sound_update();
        return;
    }

    g_queue[g_queue_len++] = note;
}

void sound_play_melody(const SoundNote *notes, int count)
{
    int i;

    if (!g_enabled || !notes || count <= 0) {
        return;
    }

    if (count > SOUND_QUEUE_MAX) {
        count = SOUND_QUEUE_MAX;
    }

    for (i = 0; i < count; ++i) {
        g_queue[i] = notes[i];
    }

    g_queue_len = count;
    g_queue_pos = 0;
    g_playing = 0;
    sound_update();
}

int sound_is_playing(void)
{
    return g_playing;
}
