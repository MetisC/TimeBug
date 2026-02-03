#include "records.h"

#include "options.h"

#include <stdio.h>

#define RECORDS_VERSION 1

static Records g_records;
static unsigned char g_difficulty = 0;
static int g_dirty = 0;
static int g_force_save = 0;

static const char *records_filename(unsigned char difficulty)
{
    switch (difficulty) {
    case DIFFICULTY_EASY:
        return "HS_EASY.DAT";
    case DIFFICULTY_HARD:
        return "HS_HARD.DAT";
    case DIFFICULTY_NORMAL:
    default:
        return "HS_NORM.DAT";
    }
}

static void records_set_defaults(Records *records)
{
    records->best_time_ms = 0;
    records->best_score = 0;
}

static unsigned char records_checksum(const unsigned char *data, int length)
{
    unsigned int sum = 0;
    int i;

    for (i = 0; i < length; ++i) {
        sum += data[i];
    }

    return (unsigned char)(sum & 0xFFu);
}

static int records_try_load(unsigned char difficulty, Records *records, int *force_save)
{
    const char *filename = records_filename(difficulty);
    FILE *file = fopen(filename, "rb");
    unsigned char data[10];
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

    if (data[8] != RECORDS_VERSION) {
        *force_save = 1;
        return 0;
    }

    checksum = records_checksum(data, 9);
    if (checksum != data[9]) {
        *force_save = 1;
        return 0;
    }

    records->best_time_ms = ((unsigned long)data[0]) |
                            ((unsigned long)data[1] << 8) |
                            ((unsigned long)data[2] << 16) |
                            ((unsigned long)data[3] << 24);
    records->best_score = ((unsigned long)data[4]) |
                          ((unsigned long)data[5] << 8) |
                          ((unsigned long)data[6] << 16) |
                          ((unsigned long)data[7] << 24);

    *force_save = 0;
    return 1;
}

static int records_write_file(unsigned char difficulty, const Records *records)
{
    const char *filename = records_filename(difficulty);
    FILE *file = fopen(filename, "wb");
    unsigned char data[10];
    size_t written;

    if (!file) {
        return 0;
    }

    data[0] = (unsigned char)(records->best_time_ms & 0xFFu);
    data[1] = (unsigned char)((records->best_time_ms >> 8) & 0xFFu);
    data[2] = (unsigned char)((records->best_time_ms >> 16) & 0xFFu);
    data[3] = (unsigned char)((records->best_time_ms >> 24) & 0xFFu);
    data[4] = (unsigned char)(records->best_score & 0xFFu);
    data[5] = (unsigned char)((records->best_score >> 8) & 0xFFu);
    data[6] = (unsigned char)((records->best_score >> 16) & 0xFFu);
    data[7] = (unsigned char)((records->best_score >> 24) & 0xFFu);
    data[8] = RECORDS_VERSION;
    data[9] = records_checksum(data, 9);

    written = fwrite(data, 1, sizeof(data), file);
    fclose(file);

    return written == sizeof(data);
}

void records_init(void)
{
    const GameOptions *options = options_get();
    g_difficulty = options->difficulty;
    if (!records_try_load(g_difficulty, &g_records, &g_force_save)) {
        records_set_defaults(&g_records);
    }
    g_dirty = 0;
}

void records_set_difficulty(unsigned char difficulty)
{
    if (difficulty == g_difficulty) {
        return;
    }

    records_save_if_dirty();
    g_difficulty = difficulty;
    if (!records_try_load(g_difficulty, &g_records, &g_force_save)) {
        records_set_defaults(&g_records);
    }
    g_dirty = 0;
}

const Records *records_get(void)
{
    return &g_records;
}

void records_mark_dirty(void)
{
    g_dirty = 1;
}

int records_save_if_dirty(void)
{
    if (!(g_dirty || g_force_save)) {
        return 1;
    }

    if (!records_write_file(g_difficulty, &g_records)) {
        return 0;
    }

    g_dirty = 0;
    g_force_save = 0;
    return 1;
}
