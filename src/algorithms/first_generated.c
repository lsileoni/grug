#include "first_generated.h"

#include <stddef.h>

#include "../movegen.h"

static bool firstGeneratedChooseMove(Board* b, const SearchLimits* limits, SearchResult* result)
{
    (void)limits;

    Move moves[MAX_MOVES];
    int  n = generateAllMoves(b, moves);

    result->bestMove = n > 0 ? moves[0] : NO_MOVE;
    result->nodes = n > 0 ? 1 : 0;
    result->hasScore = false;
    result->score = 0;
    return true;
}

const Algorithm FirstGeneratedAlgorithm = {
    "first_generated",
    "example algorithm that returns the first generated pseudo-legal move",
    NULL,
    NULL,
    NULL,
    firstGeneratedChooseMove,
};
