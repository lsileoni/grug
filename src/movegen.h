#ifndef GRUG_MOVEGEN_H
#define GRUG_MOVEGEN_H

#include "board.h"

int generateAllMoves(const Board* b, Move* moves);
int generateNoisyMoves(const Board* b, Move* moves);
int generateQuietMoves(const Board* b, Move* moves);

bool moveIsLegal(Board* b, Move m);

#endif
