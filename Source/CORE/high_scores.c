#include "high_scores.h"

#include "options.h"

#include <stdio.h>
#include <string.h>

#define HIGH_SCORES_VERSION 3
#define HIGH_SCORE_ENTRY_BYTES 11
#define HIGH_SCORE_HEADER_BYTES_V2 3
#define HIGH_SCORE_HEADER_BYTES_V3 2
#define HIGH_SCORES_DIFFICULTY_COUNT 3
#define HIGH_SCORE_TABLE_BYTES (1 + (HIGH_SCORES_MAX_ENTRIES * HIGH_SCORE_ENTRY_BYTES))
#define HIGH_SCORE_FILE_BYTES (HIGH_SCORE_HEADER_BYTES_V3 + (HIGH_SCORES_DIFFICULTY_COUNT * HIGH_SCORE_TABLE_BYTES))

static const unsigned char high_scores_difficulties[HIGH_SCORES_DIFFICULTY_COUNT] = {
    DIFFICULTY_EASY,
    DIFFICULTY_NORMAL,
    DIFFICULTY_HARD
};

static int high_scores_difficulty_index(unsigned char difficulty)
{
    int i;

    for (i = 0; i < HIGH_SCORES_DIFFICULTY_COUNT; ++i) {
        if (high_scores_difficulties[i] == difficulty) {
            return i;
        }
    }

    return -1;
}

static char high_scores_difficulty_code(unsigned char difficulty)
{
    switch (difficulty) {
    case DIFFICULTY_EASY:
        return 'E';
    case DIFFICULTY_HARD:
        return 'H';
    case DIFFICULTY_NORMAL:
    default:
        return 'N';
    }
}

static const char *high_scores_filename(HighScoreGame game, int year, unsigned char difficulty)
{
    static char filename[32];

    (void)difficulty;

    if (game == HIGH_SCORE_GAME_STORY) {
        snprintf(filename, sizeof(filename), "HS_STORY.DAT");
        return filename;
    }

    if (game == HIGH_SCORE_GAME_NONE || year <= 0) {
        return NULL;
    }

    snprintf(filename, sizeof(filename), "HS_%d_.DAT", year);
    return filename;
}

static const char *high_scores_legacy_filename(HighScoreGame game, int year, unsigned char difficulty)
{
    static char filename[32];

    if (game == HIGH_SCORE_GAME_STORY) {
        snprintf(filename, sizeof(filename), "HS_STORY_%c.DAT", high_scores_difficulty_code(difficulty));
        return filename;
    }

    if (game == HIGH_SCORE_GAME_NONE || year <= 0) {
        return NULL;
    }

    snprintf(filename, sizeof(filename), "HS_%d_%c.DAT", year, high_scores_difficulty_code(difficulty));
    return filename;
}

static uint64_t high_scores_read_u64(const unsigned char *data)
{
    uint64_t value = 0;
    int i;

    for (i = 0; i < 8; ++i) {
        value |= ((uint64_t)data[i]) << (8 * i);
    }

    return value;
}

static void high_scores_write_u64(unsigned char *data, uint64_t value)
{
    int i;

    for (i = 0; i < 8; ++i) {
        data[i] = (unsigned char)((value >> (8 * i)) & 0xFFu);
    }
}

void high_scores_table_init(HighScoreTable *table)
{
    int i;

    if (!table) {
        return;
    }

    table->count = 0;
    for (i = 0; i < HIGH_SCORES_MAX_ENTRIES; ++i) {
        table->entries[i].score = 0;
        memcpy(table->entries[i].initials, "   ", HIGH_SCORES_INITIALS_LEN);
        table->entries[i].initials[HIGH_SCORES_INITIALS_LEN] = '\0';
    }
}

static void high_scores_sort(HighScoreTable *table)
{
    int i;
    int j;

    if (!table) {
        return;
    }

    for (i = 0; i < (int)table->count - 1; ++i) {
        for (j = i + 1; j < (int)table->count; ++j) {
            if (table->entries[j].score > table->entries[i].score) {
                HighScoreEntry temp = table->entries[i];
                table->entries[i] = table->entries[j];
                table->entries[j] = temp;
            }
        }
    }
}

static int high_scores_load_legacy_file(const char *filename, unsigned char difficulty, HighScoreTable *table)
{
    FILE *file;
    unsigned char data[HIGH_SCORE_HEADER_BYTES_V2 + (HIGH_SCORES_MAX_ENTRIES * HIGH_SCORE_ENTRY_BYTES)];
    size_t read_bytes;
    unsigned char count;
    int i;

    if (!filename) {
        return 0;
    }

    file = fopen(filename, "rb");
    if (!file) {
        return 0;
    }

    read_bytes = fread(data, 1, sizeof(data), file);
    fclose(file);

    if (read_bytes != sizeof(data)) {
        return 0;
    }

    if (data[0] != 2) {
        return 0;
    }

    if (data[1] != difficulty) {
        return 0;
    }

    count = data[2];
    if (count > HIGH_SCORES_MAX_ENTRIES) {
        return 0;
    }

    table->count = count;
    for (i = 0; i < HIGH_SCORES_MAX_ENTRIES; ++i) {
        int offset = HIGH_SCORE_HEADER_BYTES_V2 + (i * HIGH_SCORE_ENTRY_BYTES);
        uint64_t score = high_scores_read_u64(&data[offset]);

        table->entries[i].score = score;
        table->entries[i].initials[0] = data[offset + 8] ? (char)data[offset + 8] : ' ';
        table->entries[i].initials[1] = data[offset + 9] ? (char)data[offset + 9] : ' ';
        table->entries[i].initials[2] = data[offset + 10] ? (char)data[offset + 10] : ' ';
        table->entries[i].initials[HIGH_SCORES_INITIALS_LEN] = '\0';
    }

    high_scores_sort(table);
    return 1;
}

static int high_scores_load_all(HighScoreGame game, int year, HighScoreTable tables[HIGH_SCORES_DIFFICULTY_COUNT])
{
    FILE *file;
    const char *filename = high_scores_filename(game, year, DIFFICULTY_NORMAL);
    unsigned char data[HIGH_SCORE_FILE_BYTES];
    size_t read_bytes;
    int i;
    int j;
    int offset;

    if (!filename) {
        return 0;
    }

    for (i = 0; i < HIGH_SCORES_DIFFICULTY_COUNT; ++i) {
        high_scores_table_init(&tables[i]);
    }

    file = fopen(filename, "rb");
    if (!file) {
        return 0;
    }

    read_bytes = fread(data, 1, sizeof(data), file);
    fclose(file);

    if (read_bytes != sizeof(data)) {
        return 0;
    }

    if (data[0] != HIGH_SCORES_VERSION) {
        return 0;
    }

    if (data[1] != HIGH_SCORES_DIFFICULTY_COUNT) {
        return 0;
    }

    offset = HIGH_SCORE_HEADER_BYTES_V3;
    for (i = 0; i < HIGH_SCORES_DIFFICULTY_COUNT; ++i) {
        unsigned char count = data[offset];
        offset += 1;
        if (count > HIGH_SCORES_MAX_ENTRIES) {
            return 0;
        }

        tables[i].count = count;
        for (j = 0; j < HIGH_SCORES_MAX_ENTRIES; ++j) {
            uint64_t score = high_scores_read_u64(&data[offset]);

            tables[i].entries[j].score = score;
            tables[i].entries[j].initials[0] = data[offset + 8] ? (char)data[offset + 8] : ' ';
            tables[i].entries[j].initials[1] = data[offset + 9] ? (char)data[offset + 9] : ' ';
            tables[i].entries[j].initials[2] = data[offset + 10] ? (char)data[offset + 10] : ' ';
            tables[i].entries[j].initials[HIGH_SCORES_INITIALS_LEN] = '\0';
            offset += HIGH_SCORE_ENTRY_BYTES;
        }
        high_scores_sort(&tables[i]);
    }

    return 1;
}

static void high_scores_load_legacy_all(HighScoreGame game, int year,
                                        HighScoreTable tables[HIGH_SCORES_DIFFICULTY_COUNT])
{
    int i;

    for (i = 0; i < HIGH_SCORES_DIFFICULTY_COUNT; ++i) {
        const char *filename = high_scores_legacy_filename(game, year, high_scores_difficulties[i]);
        high_scores_table_init(&tables[i]);
        if (filename) {
            high_scores_load_legacy_file(filename, high_scores_difficulties[i], &tables[i]);
        }
    }
}

static int high_scores_save_all(HighScoreGame game, int year,
                                const HighScoreTable tables[HIGH_SCORES_DIFFICULTY_COUNT])
{
    FILE *file;
    const char *filename = high_scores_filename(game, year, DIFFICULTY_NORMAL);
    unsigned char data[HIGH_SCORE_FILE_BYTES];
    size_t written;
    int i;
    int j;
    int offset;

    if (!filename || !tables) {
        return 0;
    }

    data[0] = HIGH_SCORES_VERSION;
    data[1] = HIGH_SCORES_DIFFICULTY_COUNT;

    offset = HIGH_SCORE_HEADER_BYTES_V3;
    for (i = 0; i < HIGH_SCORES_DIFFICULTY_COUNT; ++i) {
        unsigned char count = tables[i].count;
        if (count > HIGH_SCORES_MAX_ENTRIES) {
            count = HIGH_SCORES_MAX_ENTRIES;
        }
        data[offset] = count;
        offset += 1;
        for (j = 0; j < HIGH_SCORES_MAX_ENTRIES; ++j) {
            char c0 = tables[i].entries[j].initials[0] ? tables[i].entries[j].initials[0] : ' ';
            char c1 = tables[i].entries[j].initials[1] ? tables[i].entries[j].initials[1] : ' ';
            char c2 = tables[i].entries[j].initials[2] ? tables[i].entries[j].initials[2] : ' ';

            high_scores_write_u64(&data[offset], tables[i].entries[j].score);
            data[offset + 8] = (unsigned char)c0;
            data[offset + 9] = (unsigned char)c1;
            data[offset + 10] = (unsigned char)c2;
            offset += HIGH_SCORE_ENTRY_BYTES;
        }
    }

    file = fopen(filename, "wb");
    if (!file) {
        return 0;
    }

    written = fwrite(data, 1, sizeof(data), file);
    fclose(file);

    return written == sizeof(data);
}

int high_scores_load(HighScoreGame game, int year, unsigned char difficulty, HighScoreTable *table)
{
    HighScoreTable tables[HIGH_SCORES_DIFFICULTY_COUNT];
    const char *legacy_filename;
    int index;

    if (!table) {
        return 0;
    }

    high_scores_table_init(table);

    index = high_scores_difficulty_index(difficulty);
    if (index < 0) {
        return 0;
    }

    if (high_scores_load_all(game, year, tables)) {
        *table = tables[index];
        return 1;
    }

    legacy_filename = high_scores_legacy_filename(game, year, difficulty);
    if (!legacy_filename) {
        return 0;
    }

    return high_scores_load_legacy_file(legacy_filename, difficulty, table);
}

int high_scores_save(HighScoreGame game, int year, unsigned char difficulty, const HighScoreTable *table)
{
    HighScoreTable tables[HIGH_SCORES_DIFFICULTY_COUNT];
    int index;

    if (!table) {
        return 0;
    }

    index = high_scores_difficulty_index(difficulty);
    if (index < 0) {
        return 0;
    }

    if (!high_scores_load_all(game, year, tables)) {
        high_scores_load_legacy_all(game, year, tables);
    }

    tables[index] = *table;
    return high_scores_save_all(game, year, tables);
}

int high_scores_load_story(unsigned char difficulty, HighScoreTable *table)
{
    return high_scores_load(HIGH_SCORE_GAME_STORY, 0, difficulty, table);
}

int high_scores_save_story(unsigned char difficulty, const HighScoreTable *table)
{
    return high_scores_save(HIGH_SCORE_GAME_STORY, 0, difficulty, table);
}

int high_scores_qualifies(const HighScoreTable *table, uint64_t score)
{
    if (!table) {
        return 0;
    }

    if (table->count < HIGH_SCORES_MAX_ENTRIES) {
        return 1;
    }

    return score > table->entries[HIGH_SCORES_MAX_ENTRIES - 1].score;
}

int high_scores_insert(HighScoreTable *table, uint64_t score, const char initials[HIGH_SCORES_INITIALS_LEN + 1])
{
    int i;
    int insert_at;
    int count;

    if (!table) {
        return 0;
    }

    if (!high_scores_qualifies(table, score)) {
        return 0;
    }

    count = table->count;
    if (count < HIGH_SCORES_MAX_ENTRIES) {
        table->count = (unsigned char)(count + 1);
    } else {
        count = HIGH_SCORES_MAX_ENTRIES - 1;
    }

    insert_at = 0;
    while (insert_at < count && score <= table->entries[insert_at].score) {
        insert_at++;
    }

    for (i = HIGH_SCORES_MAX_ENTRIES - 1; i > insert_at; --i) {
        table->entries[i] = table->entries[i - 1];
    }

    table->entries[insert_at].score = score;
    if (initials) {
        table->entries[insert_at].initials[0] = initials[0] ? initials[0] : ' ';
        table->entries[insert_at].initials[1] = initials[1] ? initials[1] : ' ';
        table->entries[insert_at].initials[2] = initials[2] ? initials[2] : ' ';
    } else {
        memcpy(table->entries[insert_at].initials, "   ", HIGH_SCORES_INITIALS_LEN);
    }
    table->entries[insert_at].initials[HIGH_SCORES_INITIALS_LEN] = '\0';

    return 1;
}

int high_scores_is_empty(const HighScoreTable *table)
{
    return !table || table->count == 0;
}

HighScoreGame high_scores_game_for_year(int year)
{
    switch (year) {
    case 1972:
        return HIGH_SCORE_GAME_PONG;
    case 1978:
        return HIGH_SCORE_GAME_INVADERS;
    case 1979:
        return HIGH_SCORE_GAME_BREAKOUT;
    case 1981:
        return HIGH_SCORE_GAME_FROG;
    case 1982:
        return HIGH_SCORE_GAME_TRON;
    case 1983:
        return HIGH_SCORE_GAME_TAPP;
    case 1989:
        return HIGH_SCORE_GAME_PANG;
    case 1991:
        return HIGH_SCORE_GAME_GORI;
    case 2013:
        return HIGH_SCORE_GAME_FLAPPY;
    default:
        return HIGH_SCORE_GAME_NONE;
    }
}

void high_scores_format_score(char *buffer, size_t buffer_size, uint64_t score)
{
    if (!buffer || buffer_size == 0) {
        return;
    }

    snprintf(buffer, buffer_size, "%0*llu", HIGH_SCORES_SCORE_DIGITS, (unsigned long long)score);
}
