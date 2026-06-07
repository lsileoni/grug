#include "square_maximization.h"

#include <stddef.h>

#include "../algohelpers.h"

// Mobility of the side that just moved, measured on the position a move leads to.
// Handed to afterMove() as the per-move score: more reachable squares is better.
static int moverMobility(const Board* b, void* ctx)
{
    (void)ctx;
    return mobility(b, moverSide(b));
}

static bool squareMaximizationEvaluate(const Board* b, int* score)
{
    *score = mobility(b, sideToMove(b));
    return true;
}

static bool squareMaximizationChooseMove(Board* b, const SearchLimits* limits, SearchResult* result)
{
    (void)limits;
    searchResultInit(result);

    // Score every legal move by the mobility it gives us, and keep the highest.
    Move moves[MAX_MOVES];
    int  n = legalMoves(b, moves);

    Move bestMove = NO_MOVE;
    int  bestScore = -1;

    for (int i = 0; i < n; i++)
    {
        result->nodes++;
        int score = afterMove(b, moves[i], moverMobility, NULL);
        if (score > bestScore)
        {
            bestScore = score;
            bestMove = moves[i];
        }
    }

    result->bestMove = bestMove;
    if (bestMove != NO_MOVE)
    {
        result->hasScore = true;
        result->score = bestScore;
    }
    return true;
}

const Algorithm SquareMaximizationAlgorithm = {
    "square_maximization",
    "chooses the move maximizing the mover's reachable (mobility) squares",
    NULL,
    NULL,
    squareMaximizationEvaluate,
    squareMaximizationChooseMove,
};
