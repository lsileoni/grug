#ifndef GRUG_ZOBRIST_H
#define GRUG_ZOBRIST_H

#include "types.h"

extern uint64_t ZobristPieces[PIECE_NB][SQUARE_NB];
extern uint64_t ZobristCastling[CASTLE_NB];
extern uint64_t ZobristEnPassant[FILE_NB];
extern uint64_t ZobristSide;

void initZobrist(void);

#endif
