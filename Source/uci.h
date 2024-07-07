#ifndef UCI_H
#define UCI_H

#include "structs.h"

#define version "0.6 Dev"

#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define MAX(A, B) ((A) > (B) ? (A) : (B))

extern limits_t limits;

extern const char *square_to_coordinates[];
extern char promoted_pieces[];

void uci_loop(position_t *pos, thread_t *thread);
void print_move(int move);

#endif
