#ifndef PVTABLE_H
#define PVTABLE_H

#include "structs.h"

typedef struct tt {
  tt_entry_t *hash_entry;
  uint32_t num_of_entries;
  uint16_t current_age;
} tt_t;

extern tt_t tt;

#define no_hash_entry 100000

// transposition table hash flags
#define hash_flag_exact 0
#define hash_flag_alpha 1
#define hash_flag_beta 2

void clear_hash_table();
int read_hash_entry(position_t *pos, int alpha, int *move,
                    int beta, int depth);
void write_hash_entry(position_t *pos, int score, int depth,
                      int move, int hash_flag);
void init_hash_table(engine_t *engine, uint64_t mb);
uint64_t generate_hash_key(engine_t *engine, position_t *pos);
#endif
