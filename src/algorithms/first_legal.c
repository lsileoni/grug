#include "first_legal.h"

#include "../algohelpers.h"

static void firstLegalChooseMove(Board* b, const SearchLimits* limits, SearchResult* result)
{
    (void)limits;

    MoveList list = legalMoves(b);
    if (list.count > 0)
        result->bestMove = list.moves[0];
    result->nodes = 1;
}

const Algorithm FirstLegalAlgorithm = {
    .name = "first_legal",
    .description = "plays the first legal move in generation order",
    .chooseMove = firstLegalChooseMove,
};
