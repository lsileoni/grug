#include "first_generated.h"

#include <stddef.h>

#include "../algohelpers.h"
#include "../movegen.h"

static bool firstGeneratedChooseMove(Board* b, const SearchLimits* limits, SearchResult* result)
{
    (void)limits;
    searchResultInit(result);

    // Intentionally pseudo-legal: this picks the first GENERATED move with no
    // legality check (note generateAllMoves, not legalMoves), to contrast with
    // first_legal. The search wrapper will reject it if it turns out illegal.
    Move moves[MAX_MOVES];
    int  n = generateAllMoves(b, moves);
    if (n > 0)
    {
        result->bestMove = moves[0];
        result->nodes = 1;
    }
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
