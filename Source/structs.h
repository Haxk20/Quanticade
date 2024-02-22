#ifndef STRUCTS_H
#define STRUCTS_H

#include "bitboards.h"
#include <stdint.h>

typedef struct tt_entry {
  uint64_t hash_key; // "almost" unique chess position identifier
  int depth;         // current search depth
  int flag;          // flag the type of node (fail-low/fail-high/PV)
  int score;         // score (alpha/beta/PV)
  int move;
  uint16_t age;
} tt_entry_t;

typedef struct move {
  int move;
  int score;
} move_t;

// move list structure
typedef struct moves {
  move_t entry[280];
  uint32_t count;
} moves;

typedef struct keys {
  uint64_t piece_keys[12][64];
  uint64_t enpassant_keys[64];
  uint64_t castle_keys[16];
  uint64_t side_key;
} keys_t;

typedef struct position {
  uint64_t bitboards[12];
  uint64_t occupancies[3];
  uint64_t hash_key;
  uint64_t repetition_table[1000];
  uint32_t repetition_index;
  uint32_t ply;
  uint32_t fifty;
  int killer_moves[2][max_ply];
  int history_moves[12][64];
  int pv_length[max_ply];
  int pv_table[max_ply][max_ply];
  uint8_t follow_pv;
  uint8_t score_pv;
  uint8_t side;
  uint8_t enpassant;
  uint8_t castle;
} position_t;

typedef struct searchinfo {
  uint64_t starttime;
  uint64_t stoptime;
  uint64_t nodes;
  int64_t time;
  int32_t inc;
  uint16_t movestogo;
  uint8_t timeset;
  uint8_t stopped;
  uint8_t quit;
} searchinfo_t;

typedef struct searchthread {
  position_t* pos;
  searchinfo_t* searchinfo;
  char line[10000];
} searchthread_t;

typedef struct engine {
  keys_t keys;
  char *nnue_file;
  uint32_t random_state;
  uint8_t nnue;
} engine_t;

#endif
