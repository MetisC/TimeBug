#ifndef RECORDS_H
#define RECORDS_H

typedef struct {
    unsigned long best_time_ms;
    unsigned long best_score;
} Records;

void records_init(void);
void records_set_difficulty(unsigned char difficulty);
const Records *records_get(void);
void records_mark_dirty(void);
int records_save_if_dirty(void);

#endif
