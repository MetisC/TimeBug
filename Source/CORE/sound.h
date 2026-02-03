#ifndef SOUND_H
#define SOUND_H

typedef enum {
    SOUND_BACKEND_NONE = 0,
    SOUND_BACKEND_PC_SPEAKER = 1
} SoundBackend;

typedef struct {
    unsigned int freq;
    unsigned int duration_ms;
} SoundNote;

void sound_init(void);
void sound_shutdown(void);
void sound_set_enabled(int enabled);
void sound_set_backend(SoundBackend backend);
void sound_update(void);
void sound_play_tone(unsigned int freq, unsigned int duration_ms);
void sound_play_melody(const SoundNote *notes, int count);
int sound_is_playing(void);

#endif
