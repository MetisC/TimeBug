#ifndef GAME_SETTINGS_H
#define GAME_SETTINGS_H

typedef struct {
    unsigned char difficulty;
    unsigned char sound_enabled;
    unsigned char input_mode;
    unsigned char game_speed;
    float speed_multiplier;
} GameSettings;

#endif
