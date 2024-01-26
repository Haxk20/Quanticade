#include "search.h"
#include "attacks.h"
#include "bitboards.h"
#include "enums.h"
#include "evaluate.h"
#include "movegen.h"
#include "pvtable.h"
#include "structs.h"
#include "uci.h"
#include "utils.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const int full_depth_moves = 4;
const int reduction_limit = 3;

const int mvv_lva[12][12] = {
    {105, 205, 305, 405, 505, 605, 105, 205, 305, 405, 505, 605},
    {104, 204, 304, 404, 504, 604, 104, 204, 304, 404, 504, 604},
    {103, 203, 303, 403, 503, 603, 103, 203, 303, 403, 503, 603},
    {102, 202, 302, 402, 502, 602, 102, 202, 302, 402, 502, 602},
    {101, 201, 301, 401, 501, 601, 101, 201, 301, 401, 501, 601},
    {100, 200, 300, 400, 500, 600, 100, 200, 300, 400, 500, 600},

    {105, 205, 305, 405, 505, 605, 105, 205, 305, 405, 505, 605},
    {104, 204, 304, 404, 504, 604, 104, 204, 304, 404, 504, 604},
    {103, 203, 303, 403, 503, 603, 103, 203, 303, 403, 503, 603},
    {102, 202, 302, 402, 502, 602, 102, 202, 302, 402, 502, 602},
    {101, 201, 301, 401, 501, 601, 101, 201, 301, 401, 501, 601},
    {100, 200, 300, 400, 500, 600, 100, 200, 300, 400, 500, 600}};

/*  =======================
         Move ordering
    =======================

    1. PV move
    2. Captures in MVV/LVA
    3. 1st killer move
    4. 2nd killer move
    5. History moves
    6. Unsorted moves
*/

void communicate(searchinfo_t *searchinfo) {
  // if time is up break here
  if (searchinfo->timeset == 1 && get_time_ms() > searchinfo->stoptime) {
    // tell engine to stop calculating
    searchinfo->stopped = 1;
  }

  // read GUI input
  read_input(searchinfo);
}

// enable PV move scoring
static inline void enable_pv_scoring(board_t *board, moves *move_list) {
  // disable following PV
  board->follow_pv = 0;

  // loop over the moves within a move list
  for (uint32_t count = 0; count < move_list->count; count++) {
    // make sure we hit PV move
    if (board->pv_table[0][board->ply] == move_list->entry[count].move) {
      // enable move scoring
      board->score_pv = 1;

      // enable following PV
      board->follow_pv = 1;
    }
  }
}

// score moves
static inline void score_move(board_t *board, move_t *move_entry,
                              int hash_move) {
  int move = move_entry->move;
  if (move == hash_move) {
    move_entry->score = 30000;
    return;
  }

  // if PV move scoring is allowed
  if (board->score_pv) {
    // make sure we are dealing with PV move
    if (board->pv_table[0][board->ply] == move) {
      // disable score PV flag
      board->score_pv = 0;

      // give PV move the highest score to search it first
      move_entry->score = 20000;
      return;
    }
  }

  // score capture move
  if (get_move_capture(move)) {
    // init target piece
    int target_piece = P;

    // pick up bitboard piece index ranges depending on side
    int start_piece, end_piece;

    // pick up side to move
    if (board->side == white) {
      start_piece = p;
      end_piece = k;
    } else {
      start_piece = P;
      end_piece = K;
    }

    // loop over bitboards opposite to the current side to move
    for (int bb_piece = start_piece; bb_piece <= end_piece; bb_piece++) {
      // if there's a piece on the target square
      if (get_bit(board->bitboards[bb_piece], get_move_target(move))) {
        // remove it from corresponding bitboard
        target_piece = bb_piece;
        break;
      }
    }

    // score move by MVV LVA lookup [source piece][target piece]
    move_entry->score = mvv_lva[get_move_piece(move)][target_piece] + 10000;
    return;
  }

  // score quiet move
  else {
    // score 1st killer move
    if (board->killer_moves[0][board->ply] == move) {
      move_entry->score = 9000;
    }

    // score 2nd killer move
    else if (board->killer_moves[1][board->ply] == move) {
      move_entry->score = 8000;
    }

    // score history move
    else {
      move_entry->score =
          board->history_moves[get_move_piece(move)][get_move_target(move)];
    }

    return;
  }

  move_entry->score = 0;
  return;
}

// sort moves in descending order
static inline void sort_moves(moves *move_list) {
  // loop over current move within a move list
  for (uint32_t current_move = 0; current_move < move_list->count;
       current_move++) {
    // loop over next move within a move list
    for (uint32_t next_move = current_move + 1; next_move < move_list->count;
         next_move++) {
      // compare current and next move scores
      if (move_list->entry[current_move].score <
          move_list->entry[next_move].score) {
        // swap scores
        int temp_score = move_list->entry[current_move].score;
        move_list->entry[current_move].score =
            move_list->entry[next_move].score;
        move_list->entry[next_move].score = temp_score;

        // swap moves
        int temp_move = move_list->entry[current_move].move;
        move_list->entry[current_move].move = move_list->entry[next_move].move;
        move_list->entry[next_move].move = temp_move;
      }
    }
  }
}

// position repetition detection
static inline int is_repetition(board_t *board) {
  // loop over repetition indices range
  for (uint32_t index = 0; index < board->repetition_index; index++)
    // if we found the hash key same with a current
    if (board->repetition_table[index] == board->hash_key)
      // we found a repetition
      return 1;

  // if no repetition found
  return 0;
}

// quiescence search
static inline int quiescence(engine_t *engine, board_t *board,
                             searchinfo_t *searchinfo, int alpha, int beta) {
  // every 4095 nodes
  if ((searchinfo->nodes & 4095) == 0)
    // "listen" to the GUI/user input
    communicate(searchinfo);

  // we are too deep, hence there's an overflow of arrays relying on max ply
  // constant
  if (board->ply > max_ply - 1)
    // evaluate position
    return evaluate(engine, board);

  // evaluate position
  int evaluation = evaluate(engine, board);

  // fail-hard beta cutoff
  if (evaluation >= beta) {
    // node (position) fails high
    return beta;
  }

  // found a better move
  if (evaluation > alpha) {
    // PV node (position)
    alpha = evaluation;
  }

  // create move list instance
  moves move_list[1];

  // generate moves
  generate_captures(board, move_list);

  for (uint32_t count = 0; count < move_list->count; count++) {
    score_move(board, &move_list->entry[count], 0);
  }

  sort_moves(move_list);

  // loop over moves within a movelist
  for (uint32_t count = 0; count < move_list->count; count++) {
    // preserve board state
    copy_board(board->bitboards, board->occupancies, board->side,
               board->enpassant, board->castle, board->fifty, board->hash_key);

    // increment ply
    board->ply++;

    // increment repetition index & store hash key
    board->repetition_index++;
    board->repetition_table[board->repetition_index] = board->hash_key;

    // make sure to make only legal moves
    if (make_move(engine, board, move_list->entry[count].move, only_captures) ==
        0) {
      // decrement ply
      board->ply--;

      // decrement repetition index
      board->repetition_index--;

      // skip to next move
      continue;
    }

    // score current move
    int score = -quiescence(engine, board, searchinfo, -beta, -alpha);

    // decrement ply
    board->ply--;

    // decrement repetition index
    board->repetition_index--;

    // take move back
    restore_board(board->bitboards, board->occupancies, board->side,
                  board->enpassant, board->castle, board->fifty,
                  board->hash_key);

    // reutrn 0 if time is up
    if (searchinfo->stopped == 1) {
      return 0;
    }

    // found a better move
    if (score > alpha) {
      // PV node (position)
      alpha = score;

      // fail-hard beta cutoff
      if (score >= beta) {
        // node (position) fails high
        return beta;
      }
    }
  }

  // node (position) fails low
  return alpha;
}

// negamax alpha beta search
static inline int negamax(engine_t *engine, board_t *board,
                          searchinfo_t *searchinfo, tt_t *hash_table, int alpha,
                          int beta, int depth) {
  // init PV length
  board->pv_length[board->ply] = board->ply;

  // variable to store current move's score (from the static evaluation
  // perspective)
  int score;

  int move = 0;

  // define hash flag
  int hash_flag = hash_flag_alpha;

  // if position repetition occurs
  if ((is_repetition(board) || board->fifty >= 100) && board->ply) {
    // return draw score
    return 0;
  }

  // we are too deep, hence there's an overflow of arrays relying on max ply
  // constant
  if (board->ply > max_ply - 1) {
    // evaluate position
    return evaluate(engine, board);
  }

  // a hack by Pedro Castro to figure out whether the current node is PV node or
  // not
  int pv_node = beta - alpha > 1;

  // read hash entry if we're not in a root ply and hash entry is available
  // and current node is not a PV node
  if (board->ply &&
      (score = read_hash_entry(board, hash_table, alpha, &move, beta, depth)) !=
          no_hash_entry &&
      pv_node == 0)
    // if the move has already been searched (hence has a value)
    // we just return the score for this move without searching it
    return score;

  // every 4095 nodes
  if ((searchinfo->nodes & 4095) == 0)
    // "listen" to the GUI/user input
    communicate(searchinfo);

  // recursion escapre condition
  if (depth == 0) {
    // run quiescence search
    return quiescence(engine, board, searchinfo, alpha, beta);
  }

  // increment nodes count
  searchinfo->nodes++;

  // is king in check
  int in_check = is_square_attacked(board,
                                    (board->side == white)
                                        ? __builtin_ctzll(board->bitboards[K])
                                        : __builtin_ctzll(board->bitboards[k]),
                                    board->side ^ 1);

  // increase search depth if the king has been exposed into a check
  if (in_check)
    depth++;

  // legal moves counter
  int legal_moves = 0;

  int static_eval = evaluate(engine, board);

  // evaluation pruning / static null move pruning
  if (depth < 3 && !pv_node && !in_check && abs(beta - 1) > -infinity + 100) {
    // get static evaluation score

    // define evaluation margin
    int eval_margin = 120 * depth;

    // evaluation margin substracted from static evaluation score fails high
    if (static_eval - eval_margin >= beta)
      // evaluation margin substracted from static evaluation score
      return static_eval - eval_margin;
  }

  // null move pruning
  if (depth >= 3 && in_check == 0 && board->ply) {
    // preserve board state
    copy_board(board->bitboards, board->occupancies, board->side,
               board->enpassant, board->castle, board->fifty, board->hash_key);

    // increment ply
    board->ply++;

    // increment repetition index & store hash key
    board->repetition_index++;
    board->repetition_table[board->repetition_index] = board->hash_key;

    // hash enpassant if available
    if (board->enpassant != no_sq)
      board->hash_key ^= engine->keys.enpassant_keys[board->enpassant];

    // reset enpassant capture square
    board->enpassant = no_sq;

    // switch the side, literally giving opponent an extra move to make
    board->side ^= 1;

    // hash the side
    board->hash_key ^= engine->keys.side_key;

    /* search moves with reduced depth to find beta cutoffs
       depth - 1 - R where R is a reduction limit */
    score = -negamax(engine, board, searchinfo, hash_table, -beta, -beta + 1,
                     depth - 1 - 2);

    // decrement ply
    board->ply--;

    // decrement repetition index
    board->repetition_index--;

    // restore board state
    restore_board(board->bitboards, board->occupancies, board->side,
                  board->enpassant, board->castle, board->fifty,
                  board->hash_key);

    // reutrn 0 if time is up
    if (searchinfo->stopped == 1) {
      return 0;
    }

    // fail-hard beta cutoff
    if (score >= beta)
      // node (position) fails high
      return beta;
  }

  // razoring
  if (!pv_node && !in_check && depth <= 3) {
    // get static eval and add first bonus
    score = static_eval + 125;

    // define new score
    int new_score;

    // static evaluation indicates a fail-low node
    if (score < beta) {
      // on depth 1
      if (depth == 1) {
        // get quiescence score
        new_score = quiescence(engine, board, searchinfo, alpha, beta);

        // return quiescence score if it's greater then static evaluation score
        return (new_score > score) ? new_score : score;
      }

      // add second bonus to static evaluation
      score += 175;

      // static evaluation indicates a fail-low node
      if (score < beta && depth <= 2) {
        // get quiescence score
        new_score = quiescence(engine, board, searchinfo, alpha, beta);

        // quiescence score indicates fail-low node
        if (new_score < beta)
          // return quiescence score if it's greater then static evaluation
          // score
          return (new_score > score) ? new_score : score;
      }
    }
  }

  // create move list instance
  moves move_list[1];

  // generate moves
  generate_moves(board, move_list);

  // if we are now following PV line
  if (board->follow_pv)
    // enable PV move scoring
    enable_pv_scoring(board, move_list);

  for (uint32_t count = 0; count < move_list->count; count++) {
    score_move(board, &move_list->entry[count], move);
  }

  sort_moves(move_list);

  // number of moves searched in a move list
  int moves_searched = 0;

  // loop over moves within a movelist
  for (uint32_t count = 0; count < move_list->count; count++) {

    // preserve board state
    copy_board(board->bitboards, board->occupancies, board->side,
               board->enpassant, board->castle, board->fifty, board->hash_key);

    int list_move = move_list->entry[count].move;

    // increment ply
    board->ply++;

    // increment repetition index & store hash key
    board->repetition_index++;
    board->repetition_table[board->repetition_index] = board->hash_key;

    // make sure to make only legal moves
    if (make_move(engine, board, list_move, all_moves) == 0) {
      // decrement ply
      board->ply--;

      // decrement repetition index
      board->repetition_index--;

      // skip to next move
      continue;
    }

    // increment legal moves
    legal_moves++;

    // full depth search
    if (moves_searched == 0) {
      // do normal alpha beta search
      score = -negamax(engine, board, searchinfo, hash_table, -beta, -alpha,
                       depth - 1);
    }

    // late move reduction (LMR)
    else {
      // condition to consider LMR
      if (moves_searched >= full_depth_moves && depth >= reduction_limit &&
          in_check == 0 && get_move_capture(list_move) == 0 &&
          get_move_promoted(list_move) == 0) {
        // search current move with reduced depth:
        score = -negamax(engine, board, searchinfo, hash_table, -alpha - 1,
                         -alpha, depth - 2);
      }

      // hack to ensure that full-depth search is done
      else {
        score = alpha + 1;
      }

      // principle variation search PVS
      if (score > alpha) {
        /* Once you've found a move with a score that is between alpha and beta,
           the rest of the moves are searched with the goal of proving that they
           are all bad. It's possible to do this a bit faster than a search that
           worries that one of the remaining moves might be good. */
        searchinfo->nodes++;
        score = -negamax(engine, board, searchinfo, hash_table, -alpha - 1,
                         -alpha, depth - 1);

        /* If the algorithm finds out that it was wrong, and that one of the
           subsequent moves was better than the first PV move, it has to search
           again, in the normal alpha-beta manner.  This happens sometimes, and
           it's a waste of time, but generally not often enough to counteract
           the savings gained from doing the "bad move proof" search referred to
           earlier. */
        if ((score > alpha) && (score < beta)) {
          /* re-search the move that has failed to be proved to be bad
             with normal alpha beta score bounds*/
          score = -negamax(engine, board, searchinfo, hash_table, -beta, -alpha,
                           depth - 1);
        }
      }
    }

    // decrement ply
    board->ply--;

    // decrement repetition index
    board->repetition_index--;

    // take move back
    restore_board(board->bitboards, board->occupancies, board->side,
                  board->enpassant, board->castle, board->fifty,
                  board->hash_key);

    // return infinity so we can deal with timeout in case we are doing
    // re-search
    if (searchinfo->stopped == 1) {
      return infinity;
    }

    // increment the counter of moves searched so far
    moves_searched++;

    // found a better move
    if (score > alpha) {
      // switch hash flag from storing score for fail-low node
      // to the one storing score for PV node
      hash_flag = hash_flag_exact;

      move = list_move;

      // on quiet moves
      if (get_move_capture(list_move) == 0)
        // store history moves
        board->history_moves[get_move_piece(list_move)]
                            [get_move_target(list_move)] += depth;

      // PV node (position)
      alpha = score;

      // write PV move
      board->pv_table[board->ply][board->ply] = list_move;

      // loop over the next ply
      for (int next_ply = board->ply + 1;
           next_ply < board->pv_length[board->ply + 1]; next_ply++)
        // copy move from deeper ply into a current ply's line
        board->pv_table[board->ply][next_ply] =
            board->pv_table[board->ply + 1][next_ply];

      // adjust PV length
      board->pv_length[board->ply] = board->pv_length[board->ply + 1];

      // fail-hard beta cutoff
      if (score >= beta) {
        // store hash entry with the score equal to beta
        write_hash_entry(board, hash_table, beta, depth, move, hash_flag_beta);

        // on quiet moves
        if (get_move_capture(list_move) == 0) {
          // store killer moves
          board->killer_moves[1][board->ply] =
              board->killer_moves[0][board->ply];
          board->killer_moves[0][board->ply] = list_move;
        }

        // node (position) fails high
        return beta;
      }
    }
  }

  // we don't have any legal moves to make in the current postion
  if (legal_moves == 0) {
    // king is in check
    if (in_check)
      // return mating score (assuming closest distance to mating position)
      return -mate_value + board->ply;

    // king is not in check
    else
      // return stalemate score
      return 0;
  }

  // store hash entry with the score equal to alpha
  write_hash_entry(board, hash_table, alpha, depth, move, hash_flag);

  // node (position) fails low
  return alpha;
}

// search position for the best move
void search_position(engine_t *engine, board_t *board, searchinfo_t *searchinfo,
                     tt_t *hash_table, int depth) {
  // search start time
  uint64_t start = get_time_ms();

  // define best score variable
  int score = 0;

  int pv_table_copy[max_ply][max_ply];
  int pv_length_copy[max_ply];

  uint8_t window_ok = 1;

  // reset nodes counter
  searchinfo->nodes = 0;

  // reset "time is up" flag
  searchinfo->stopped = 0;

  // reset follow PV flags
  board->follow_pv = 0;
  board->score_pv = 0;

  hash_table->current_age++;

  // clear helper data structures for search
  memset(board->killer_moves, 0, sizeof(board->killer_moves));
  memset(board->history_moves, 0, sizeof(board->history_moves));
  memset(board->pv_table, 0, sizeof(board->pv_table));
  memset(board->pv_length, 0, sizeof(board->pv_length));

  // define initial alpha beta bounds
  int alpha = -infinity;
  int beta = infinity;

  // iterative deepening
  for (int current_depth = 1; current_depth <= depth; current_depth++) {
    // if time is up
    if (searchinfo->stopped == 1) {
      // stop calculating and return best move so far
      break;
    }

    // enable follow PV flag
    board->follow_pv = 1;

    // We should not save PV move from unfinished depth for example if depth 12
    // finishes and goes to search depth 13 now but this triggers window cutoff
    // we dont want the info from depth 13 as its incomplete and in case depth
    // 14 search doesnt finish in time we will at least have an full PV line
    // from depth 12
    if (window_ok) {
      memcpy(pv_table_copy, board->pv_table, sizeof(board->pv_table));
      memcpy(pv_length_copy, board->pv_length, sizeof(board->pv_length));
    }

    // find best move within a given position
    score = negamax(engine, board, searchinfo, hash_table, alpha, beta,
                    current_depth);

    // Reset aspiration window OK flag back to 1
    window_ok = 1;

    // We hit an apspiration window cut-off before time ran out and we jumped to
    // another depth with wider search which we didnt finish
    if (score == infinity) {
      // Restore the saved best line
      memcpy(board->pv_table, pv_table_copy, sizeof(pv_table_copy));
      memcpy(board->pv_length, pv_length_copy, sizeof(pv_length_copy));
      // Break out of the loop without printing info about the unfinished depth
      break;
    }

    // we fell outside the window, so try again with a full-width window (and
    // the same depth)
    if ((score <= alpha) || (score >= beta)) {
      // Do a full window re-search
      alpha = -infinity;
      beta = infinity;
      window_ok = 0;
      current_depth--;
      continue;
    }

    // set up the window for the next iteration
    alpha = score - 50;
    beta = score + 50;

    // if PV is available
    if (board->pv_length[0]) {
      // print search info
      uint64_t time = get_time_ms() - start;
      uint64_t nps = (searchinfo->nodes / fmax(time, 1)) * 100;
      if (score > -mate_value && score < -mate_score) {
#ifdef WIN64
        printf("info depth %d score mate %d nodes %llu nps %llu time %llu pv ",
               current_depth, -(score + mate_value) / 2 - 1, searchinfo->nodes,
               nps, time);
#else
        printf("info depth %d score mate %d nodes %lu nps %ld time %lu pv ",
               current_depth, -(score + mate_value) / 2 - 1, searchinfo->nodes,
               nps, time);
#endif
      }

      else if (score > mate_score && score < mate_value) {
#ifdef WIN64
        printf("info depth %d score mate %d nodes %llu nps %llu time %llu pv ",
               current_depth, (mate_value - score) / 2 + 1, searchinfo->nodes,
               nps, time);
#else
        printf("info depth %d score mate %d nodes %lu nps %ld time %lu pv ",
               current_depth, (mate_value - score) / 2 + 1, searchinfo->nodes,
               nps, time);
#endif
      }

      else {
#ifdef WIN64
        printf("info depth %d score cp %d nodes %llu nps %llu time %llu pv ",
               current_depth, score, searchinfo->nodes, nps, time);
#else
        printf("info depth %d score cp %d nodes %lu nps %ld time %lu pv ",
               current_depth, score, searchinfo->nodes, nps, time);
#endif
      }

      // loop over the moves within a PV line
      for (int count = 0; count < board->pv_length[0]; count++) {
        // print PV move
        print_move(board->pv_table[0][count]);
        printf(" ");
      }

      // print new line
      printf("\n");
    }
  }

  // print best move
  printf("bestmove ");
  if (board->pv_table[0][0]) {
    print_move(board->pv_table[0][0]);
  } else {
    printf("(none)");
  }
  printf("\n");
}
