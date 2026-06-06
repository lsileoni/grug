#ifndef GRUG_PERFT_H
#define GRUG_PERFT_H

#include "board.h"

uint64_t perft(Board* b, int depth);
void     perftDivide(Board* b, int depth);

#endif
