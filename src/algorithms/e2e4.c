#include "e2e4.h"

#include <stddef.h>

static bool e2e4ChooseMove(Board* b, const SearchLimits* limits, SearchResult* result)
{
    (void)b;
    (void)limits;

    result->bestMove = makeMove(E2, E4);
    result->nodes = 1;
    result->hasScore = false;
    result->score = 0;
    return true;
}

const Algorithm E2E4Algorithm = {
    "e2e4", "example algorithm that always asks for e2e4", NULL, NULL, NULL, e2e4ChooseMove,
};
