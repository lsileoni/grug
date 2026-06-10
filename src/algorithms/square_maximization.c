#include "square_maximization.h"

#include "../algohelpers.h"

static int squareMaximizationEvaluate(const Board* b)
{
    return mobility(b, sideToMove(b));
}

static void squareMaximizationChooseMove(Board* b, const SearchLimits* limits, SearchResult* result)
{
    (void)limits;

    int      us = sideToMove(b);
    MoveList list = legalMoves(b);

    int bestScore = -1;
    for (int i = 0; i < list.count; i++)
    {
        Board after = boardAfter(b, list.moves[i]);
        int   score = mobility(&after, us);

        result->nodes++;
        if (score > bestScore)
        {
            bestScore = score;
            result->bestMove = list.moves[i];
            result->score = score;
        }
    }
}

const Algorithm SquareMaximizationAlgorithm = {
    .name = "square_maximization",
    .description = "chooses the move maximizing the mover's reachable (mobility) squares",
    .evaluate = squareMaximizationEvaluate,
    .chooseMove = squareMaximizationChooseMove,
};
