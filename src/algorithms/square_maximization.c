#include "square_maximization.h"

#include "../algohelpers.h"

enum
{
    CHECK_BONUS = 10000,
    CHECK_REPLY_MOVE_PENALTY = 100,
    CHECK_REPLY_SQUARE_PENALTY = 250,
    CHECKMATE_SCORE = VALUE_MATE - 1,
};

static int squareMaximizationEvaluate(const Board* b)
{
    return mobility(b, sideToMove(b));
}

static int replyDestinationCount(const MoveList* replies)
{
    Bitboard destinations = 0ULL;

    for (int i = 0; i < replies->count; i++)
        destinations |= squareBB(moveTo(replies->moves[i]));

    return popcount(destinations);
}

static int squareMaximizationMoveScore(const Board* b, Move m, int us)
{
    Board after = boardAfter(b, m);
    int   score = mobility(&after, us);

    if (boardInCheck(&after))
    {
        MoveList replies = legalMoves(&after);

        if (replies.count == 0)
            return CHECKMATE_SCORE;

        score += CHECK_BONUS;
        score -= replies.count * CHECK_REPLY_MOVE_PENALTY;
        score -= replyDestinationCount(&replies) * CHECK_REPLY_SQUARE_PENALTY;
    }

    return score;
}

static void squareMaximizationChooseMove(Board* b, const SearchLimits* limits, SearchResult* result)
{
    (void)limits;

    int      us = sideToMove(b);
    MoveList list = legalMoves(b);

    int bestScore = -1;
    for (int i = 0; i < list.count; i++)
    {
        int score = squareMaximizationMoveScore(b, list.moves[i], us);

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
