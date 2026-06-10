#include "e2e4.h"

#include "../algohelpers.h"

static void e2e4ChooseMove(Board* b, const SearchLimits* limits, SearchResult* result)
{
    (void)b;
    (void)limits;
    result->bestMove = makeMove(E2, E4);
    result->nodes = 1;
}

const Algorithm E2E4Algorithm = {
    .name = "e2e4",
    .description = "example algorithm that always asks for e2e4",
    .chooseMove = e2e4ChooseMove,
};
