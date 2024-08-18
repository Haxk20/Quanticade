#ifndef TRANSPOSITION_H
#define TRANSPOSITION_H

#include "structs.h"
#include <sched.h>

typedef struct tt {
  void* mem;
  tt_entry_t *hash_entry;
  size_t num_of_entries;
} tt_t;

extern tt_t tt;

#define no_hash_entry 100000

// transposition table hash flags
#define hash_flag_exact 0
#define hash_flag_alpha 1
#define hash_flag_beta 2

void clear_hash_table(void);
int read_hash_entry(position_t *pos, int alpha, int beta,
                    int depth, int *move, uint16_t *tt_score);
void write_hash_entry(position_t *pos, int score, int depth,
                      int move, int hash_flag);
void init_hash_table(uint64_t mb);
uint64_t generate_hash_key(position_t *pos);
int hash_full(void);
#endif
