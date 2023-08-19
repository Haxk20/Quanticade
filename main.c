#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
  WHITE_PAWN = 'P',
  WHITE_KNIGHT = 'N',
  WHITE_BISHOP = 'B',
  WHITE_ROOK = 'R',
  WHITE_QUEEN = 'Q',
  WHITE_KING = 'K',
  BLACK_PAWN = 'p',
  BLACK_KNIGHT = 'n',
  BLACK_BISHOP = 'b',
  BLACK_ROOK = 'r',
  BLACK_QUEEN = 'q',
  BLACK_KING = 'k'
};
enum {
  WHITE_KING_CASTLE = 1,
  WHITE_QUEEN_CASTLE = 2,
  BLACK_KING_CASTLE = 4,
  BLACK_QUEEN_CASTLE = 8
};
enum { WHITE_TURN = 'w', BLACK_TURN = 'b' };

typedef struct board {
  uint8_t castling_rights;
  uint8_t turn;
  uint8_t half_moves;
  uint8_t number_of_moves;
  uint8_t en_passant_index;
  uint8_t squares[99];
} Board;

Board *board;

void load_fen(Board *board, char *fen) {
  int index = 70;
  while (*fen != ' ') {
    if (*fen > '0' && *fen < '9') {
      index += *fen - '0';
      if ((index % 10) == 8) {
        index--;
      }
    } else if (*fen == '/') {
      index -= 17;
    } else {
      board->squares[index] = *fen;
      char *fen_next = fen + 1;
      if (*fen_next != '/') {
        index++;
      }
    }
    fen++;
  }
  fen++;
  // We have loaded the position. Now we manually load the attributes of the
  // position such as active color, castling rights, en passant, half moves and
  // full moves
  board->turn = *fen;
  fen = fen + 2;
  if (*fen == '-') {
    fen = fen + 2;
    // No castling rights
  }
  board->castling_rights = 0;
  while (*fen != ' ') {
    switch (*fen) {
    case 'K':
      board->castling_rights += WHITE_KING_CASTLE;
      break;
    case 'Q':
      board->castling_rights += WHITE_QUEEN_CASTLE;
      break;
    case 'k':
      board->castling_rights += BLACK_KING_CASTLE;
      break;
    case 'q':
      board->castling_rights += BLACK_QUEEN_CASTLE;
      break;
    }
    fen++;
  }
  fen++;
  // We have castling rights loaded. Now onto en passant
  if (*fen != '-') {
    board->en_passant_index = ((*fen - 'a' + 1) * 10);
    fen++;
    board->en_passant_index += *fen - '0';
  } else {
    board->en_passant_index = 0;
  }
  fen = fen + 2;
  sscanf(fen, "%hhd %hhd", &board->half_moves, &board->number_of_moves);
  printf("Turn: %c, Castling Rights: %d, En passant index: %d, Half Moves: %d, "
         "Moves: %d\n",
         board->turn, board->castling_rights, board->en_passant_index,
         board->half_moves, board->number_of_moves);
}

int main(void) {
  board = (Board *)malloc(sizeof(Board));
  for (int i = 0; i <= 77; i++) {
    board->squares[i] = '0';
  }
  char fen[] =
      "r3k2r/p1pp1pb1/bn2Qnp1/2qPN3/1p2P3/2N5/PPPBBPPP/R3K2R b KQkq e3 0 1";
  load_fen(board, fen);
  for (int i = 7; i >= 0; i--) {
    for (int j = 0; j < 8; j++) {
      printf("%c", board->squares[(i * 10) + j]);
    }
    printf("\n");
  }
}