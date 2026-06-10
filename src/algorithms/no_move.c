#include "no_move.h"

#include "../algohelpers.h"

static void noMoveChooseMove(Board* b, const SearchLimits* limits, SearchResult* result)
{
    (void)b;
    (void)limits;
    (void)result;
}

const Algorithm NoMoveAlgorithm = {
    .name = "no_move",
    .description = "example algorithm that returns no move and lets the engine fall back",
    .chooseMove = noMoveChooseMove,
};
