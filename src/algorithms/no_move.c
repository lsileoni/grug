#include "no_move.h"

#include <stddef.h>

static bool noMoveChooseMove(Board* b, const SearchLimits* limits, SearchResult* result)
{
    (void)b;
    (void)limits;

    result->bestMove = NO_MOVE;
    result->nodes = 0;
    result->hasScore = false;
    result->score = 0;
    return true;
}

const Algorithm NoMoveAlgorithm = {
    "no_move", "example algorithm that deliberately returns no move and lets search fallback",
    NULL,      NULL,
    NULL,      noMoveChooseMove,
};
