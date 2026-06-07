#include "no_move.h"

#include <stddef.h>

#include "../algohelpers.h"

static bool noMoveChooseMove(Board* b, const SearchLimits* limits, SearchResult* result)
{
    (void)b;
    (void)limits;

    searchResultInit(result);
    return true;
}

const Algorithm NoMoveAlgorithm = {
    "no_move", "example algorithm that deliberately returns no move and lets search fallback",
    NULL,      NULL,
    NULL,      noMoveChooseMove,
};
