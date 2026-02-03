#ifndef OPTIONS_H
#define OPTIONS_H

typedef enum {
    DIFFICULTY_EASY = 0,
    DIFFICULTY_NORMAL = 1,
    DIFFICULTY_HARD = 2
} DifficultyLevel;

typedef enum {
    GAME_SPEED_NORMAL = 0,
    GAME_SPEED_TURBO = 1
} GameSpeed;

typedef enum {
    INPUT_KEYBOARD = 0,
    INPUT_JOYSTICK = 1
} InputMode;

typedef struct {
    unsigned char difficulty;
    unsigned char sound_enabled;
    unsigned char game_speed;
    unsigned char input_mode;
} GameOptions;

void options_init(void);
const GameOptions *options_get(void);
void options_set_difficulty(unsigned char difficulty);
void options_set_sound_enabled(unsigned char enabled);
void options_set_game_speed(unsigned char speed);
void options_set_input_mode(unsigned char mode);
float options_speed_multiplier(void);
int options_is_dirty(void);
int options_save_if_dirty(void);

#endif
