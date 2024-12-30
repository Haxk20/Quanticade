#include "move.h"
#include "utils.h"
#include <stdlib.h>

int CAPTURE_HISTORY_BONUS_MAX = 1228;
int QUIET_HISTORY_BONUS_MAX = 1400;
int CONT_HISTORY_BONUS_MAX = 1257;
int CAPTURE_HISTORY_MALUS_MAX = 1288;
int QUIET_HISTORY_MALUS_MAX = 1232;
int CONT_HISTORY_MALUS_MAX = 1232;
int CAPTURE_HISTORY_BONUS_MIN = 1323;
int QUIET_HISTORY_BONUS_MIN = 1285;
int CONT_HISTORY_BONUS_MIN = 1291;
int CAPTURE_HISTORY_MALUS_MIN = 1167;
int QUIET_HISTORY_MALUS_MIN = 1314;
int CONT_HISTORY_MALUS_MIN = 1260;
int HISTORY_MAX = 8192;

static inline void update_quiet_history(thread_t *thread,
                                        int move, uint8_t depth,
                                        uint8_t is_best_move) {
  int target = get_move_target(move);
  int source = get_move_source(move);
  int bonus = 16 * depth * depth + 32 * depth + 16;
  int clamped_bonus =
      clamp(bonus, -QUIET_HISTORY_BONUS_MIN, QUIET_HISTORY_BONUS_MAX);
  int clamped_malus =
      clamp(bonus, -QUIET_HISTORY_MALUS_MIN, QUIET_HISTORY_MALUS_MAX);
  int adjust = is_best_move ? clamped_bonus : -clamped_malus;
  thread->quiet_history[thread->pos.mailbox[source]][source][target] +=
      adjust -
      thread->quiet_history[thread->pos.mailbox[source]][source][target] * abs(adjust) / HISTORY_MAX;
}

static inline void update_capture_history(thread_t *thread,
                                          int move, uint8_t depth,
                                          uint8_t is_best_move) {
  int from = get_move_source(move);
  int target = get_move_target(move);
  int bonus = 16 * depth * depth + 32 * depth + 16;
  int clamped_bonus =
      clamp(bonus, -CAPTURE_HISTORY_BONUS_MIN, CAPTURE_HISTORY_BONUS_MAX);
  int clamped_malus =
      clamp(bonus, -CAPTURE_HISTORY_MALUS_MIN, CAPTURE_HISTORY_MALUS_MAX);
  int adjust = is_best_move ? clamped_bonus : -clamped_malus;
  thread->capture_history[thread->pos.mailbox[from]][thread->pos.mailbox[target]][from][target] +=
      adjust - thread->capture_history[thread->pos.mailbox[from]][thread->pos.mailbox[target]][from]
                                      [target] *
                   abs(adjust) / HISTORY_MAX;
}

static inline void update_continuation_history(thread_t *thread,
                                               searchstack_t *ss, int move,
                                               uint8_t depth,
                                               uint8_t is_best_move) {
  int prev_piece = ss->piece;
  int prev_target = get_move_target(ss->move);
  int piece = thread->pos.mailbox[get_move_source(move)];
  int target = get_move_target(move);
  int bonus = 16 * depth * depth + 32 * depth + 16;
  int clamped_bonus =
      clamp(bonus, -CONT_HISTORY_BONUS_MIN, CONT_HISTORY_BONUS_MAX);
  int clamped_malus =
      clamp(bonus, -CONT_HISTORY_MALUS_MIN, CONT_HISTORY_MALUS_MAX);
  int adjust = is_best_move ? clamped_bonus : -clamped_malus;
  thread->continuation_history[prev_piece][prev_target][piece][target] +=
      adjust -
      thread->continuation_history[prev_piece][prev_target][piece][target] *
          abs(adjust) / HISTORY_MAX;
}

void update_quiet_history_moves(thread_t *thread,
                                moves *quiet_moves, int best_move,
                                uint8_t depth) {
  for (uint32_t i = 0; i < quiet_moves->count; ++i) {
    if (quiet_moves->entry[i].move == best_move) {
      update_quiet_history(thread, best_move, depth, 1);
    } else {
      update_quiet_history(thread, quiet_moves->entry[i].move, depth,
                           0);
    }
  }
}

void update_capture_history_moves(thread_t *thread,
                                  moves *capture_moves, int best_move,
                                  uint8_t depth) {
  for (uint32_t i = 0; i < capture_moves->count; ++i) {
    if (capture_moves->entry[i].move == best_move) {
      update_capture_history(thread, best_move, depth, 1);
    } else {
      update_capture_history(thread, capture_moves->entry[i].move,
                             depth, 0);
    }
  }
}

void update_continuation_history_moves(thread_t *thread, searchstack_t *ss,
                                       moves *quiet_moves, int best_move,
                                       uint8_t depth) {
  for (uint32_t i = 0; i < quiet_moves->count; ++i) {
    if (quiet_moves->entry[i].move == best_move) {
      update_continuation_history(thread, ss - 1, best_move, depth, 1);
      update_continuation_history(thread, ss - 2, best_move, depth, 1);
    } else {
      update_continuation_history(thread, ss - 1, quiet_moves->entry[i].move,
                                  depth, 0);
      update_continuation_history(thread, ss - 2, quiet_moves->entry[i].move,
                                  depth, 0);
    }
  }
}
