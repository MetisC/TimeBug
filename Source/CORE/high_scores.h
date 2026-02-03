#ifndef HIGH_SCORES_H
#define HIGH_SCORES_H

#include <stdint.h>
#include <stddef.h>

#define HIGH_SCORES_MAX_ENTRIES 10
#define HIGH_SCORES_INITIALS_LEN 3
#define HIGH_SCORES_SCORE_DIGITS 7

typedef enum {
    HIGH_SCORE_GAME_NONE = 0,
    HIGH_SCORE_GAME_STORY,
    HIGH_SCORE_GAME_PONG,
    HIGH_SCORE_GAME_INVADERS,
    HIGH_SCORE_GAME_BREAKOUT,
    HIGH_SCORE_GAME_FROG,
    HIGH_SCORE_GAME_TRON,
    HIGH_SCORE_GAME_TAPP,
    HIGH_SCORE_GAME_PANG,
    HIGH_SCORE_GAME_GORI,
    HIGH_SCORE_GAME_FLAPPY
} HighScoreGame;

typedef struct {
    uint64_t score;
    char initials[HIGH_SCORES_INITIALS_LEN + 1];
} HighScoreEntry;

typedef struct {
    HighScoreEntry entries[HIGH_SCORES_MAX_ENTRIES];
    unsigned char count;
} HighScoreTable;

void high_scores_table_init(HighScoreTable *table);
int high_scores_load(HighScoreGame game, int year, unsigned char difficulty, HighScoreTable *table);
int high_scores_save(HighScoreGame game, int year, unsigned char difficulty, const HighScoreTable *table);
int high_scores_load_story(unsigned char difficulty, HighScoreTable *table);
int high_scores_save_story(unsigned char difficulty, const HighScoreTable *table);
int high_scores_qualifies(const HighScoreTable *table, uint64_t score);
int high_scores_insert(HighScoreTable *table, uint64_t score, const char initials[HIGH_SCORES_INITIALS_LEN + 1]);
int high_scores_is_empty(const HighScoreTable *table);
HighScoreGame high_scores_game_for_year(int year);
void high_scores_format_score(char *buffer, size_t buffer_size, uint64_t score);

#endif
