#ifndef UTILS_H
#define UTILS_H

#include "structs.h"
uint64_t get_time_ms(void);
int input_waiting(void);
void read_input(thread_t *thread);

#endif
