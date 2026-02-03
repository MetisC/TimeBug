#include "options.h"
#include "sound.h"

#include <stdio.h>

#define OPTIONS_FILENAME "OPTIONS.DAT"
#define OPTIONS_VERSION 1

static GameOptions g_options;
static int g_dirty = 0;
static int g_force_save = 0;

static void options_set_defaults(GameOptions *options)
{
    options->difficulty = DIFFICULTY_NORMAL;
    options->sound_enabled = 1;
    options->game_speed = GAME_SPEED_NORMAL;
    options->input_mode = INPUT_KEYBOARD;
}

static unsigned char options_checksum(const unsigned char *data, int length)
{
    unsigned int sum = 0;
    int i;

    for (i = 0; i < length; ++i) {
        sum += data[i];
    }

    return (unsigned char)(sum & 0xFFu);
}

static int options_try_load(GameOptions *options, int *force_save)
{
    FILE *file = fopen(OPTIONS_FILENAME, "rb");
    unsigned char data[6];
    size_t read_bytes;
    unsigned char checksum;

    if (!file) {
        *force_save = 0;
        return 0;
    }

    read_bytes = fread(data, 1, sizeof(data), file);
    fclose(file);

    if (read_bytes != sizeof(data)) {
        *force_save = 1;
        return 0;
    }

    if (data[4] != OPTIONS_VERSION) {
        *force_save = 1;
        return 0;
    }

    checksum = options_checksum(data, 5);
    if (checksum != data[5]) {
        *force_save = 1;
        return 0;
    }

    if (data[0] > DIFFICULTY_HARD || data[1] > 1 || data[2] > GAME_SPEED_TURBO || data[3] > INPUT_JOYSTICK) {
        *force_save = 1;
        return 0;
    }

    options->difficulty = data[0];
    options->sound_enabled = data[1];
    options->game_speed = data[2];
    options->input_mode = data[3];

    *force_save = 0;
    return 1;
}

static int options_write_file(const GameOptions *options)
{
    FILE *file = fopen(OPTIONS_FILENAME, "wb");
    unsigned char data[6];
    size_t written;

    if (!file) {
        return 0;
    }

    data[0] = options->difficulty;
    data[1] = options->sound_enabled;
    data[2] = options->game_speed;
    data[3] = options->input_mode;
    data[4] = OPTIONS_VERSION;
    data[5] = options_checksum(data, 5);

    written = fwrite(data, 1, sizeof(data), file);
    fclose(file);

    return written == sizeof(data);
}

void options_init(void)
{
    if (!options_try_load(&g_options, &g_force_save)) {
        options_set_defaults(&g_options);
    }
    g_dirty = 0;
    sound_set_enabled(g_options.sound_enabled);
}

const GameOptions *options_get(void)
{
    return &g_options;
}

void options_set_difficulty(unsigned char difficulty)
{
    if (difficulty > DIFFICULTY_HARD) {
        difficulty = DIFFICULTY_HARD;
    }
    if (g_options.difficulty != difficulty) {
        g_options.difficulty = difficulty;
        g_dirty = 1;
    }
}

void options_set_sound_enabled(unsigned char enabled)
{
    enabled = enabled ? 1 : 0;
    if (g_options.sound_enabled != enabled) {
        g_options.sound_enabled = enabled;
        g_dirty = 1;
    }
    sound_set_enabled(g_options.sound_enabled);
}

void options_set_game_speed(unsigned char speed)
{
    if (speed > GAME_SPEED_TURBO) {
        speed = GAME_SPEED_TURBO;
    }
    if (g_options.game_speed != speed) {
        g_options.game_speed = speed;
        g_dirty = 1;
    }
}

void options_set_input_mode(unsigned char mode)
{
    if (mode > INPUT_JOYSTICK) {
        mode = INPUT_JOYSTICK;
    }
    if (g_options.input_mode != mode) {
        g_options.input_mode = mode;
        g_dirty = 1;
    }
}

float options_speed_multiplier(void)
{
    if (g_options.game_speed == GAME_SPEED_TURBO) {
        return 1.25f;
    }
    return 1.0f;
}

int options_is_dirty(void)
{
    return g_dirty || g_force_save;
}

int options_save_if_dirty(void)
{
    if (!options_is_dirty()) {
        return 1;
    }

    if (!options_write_file(&g_options)) {
        return 0;
    }

    g_dirty = 0;
    g_force_save = 0;
    return 1;
}
