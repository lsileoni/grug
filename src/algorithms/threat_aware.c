#include "threat_aware.h"

#include "../algohelpers.h"

#define CHECK_BONUS 40

static void threatAwareChooseMove(Board* b, const SearchLimits* limits, SearchResult* result)
{
    (void)limits;

    int      us = sideToMove(b);
    MoveList list = legalMoves(b);

    int bestScore = 0;
    for (int i = 0; i < list.count; i++)
    {
        Move m = list.moves[i];
        int  score = 0;

        if (moveIsCapture(b, m))
        {
            int exchange = see(b, m);
            if (exchange > 0)
                score += exchange;
        }

        if (moveGivesCheck(b, m))
            score += CHECK_BONUS;

        Board after = boardAfter(b, m);
        score -= hangingValue(&after, us);

        result->nodes++;
        if (result->bestMove == NO_MOVE || score > bestScore)
        {
            bestScore = score;
            result->bestMove = m;
            result->score = score;
        }
    }
}

const Algorithm ThreatAwareAlgorithm = {
    .name = "threat_aware",
    .description = "win material by SEE, give checks, avoid hanging pieces",
    .chooseMove = threatAwareChooseMove,
};
