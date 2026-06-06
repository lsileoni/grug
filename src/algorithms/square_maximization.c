#include "square_maximization.h"

#include <stddef.h>

#include "../attacks.h"
#include "../bitboard.h"
#include "../movegen.h"

// Counts the "mobility" squares for one colour: the union of every square its
// pieces attack, excluding squares blocked by its own pieces. This leaves empty
// squares plus capturable enemy squares. Derived from attack sets, so pawns
// contribute only their diagonal attack squares (pushes are not counted).
static int mobilityCount(const Board* b, int colour)
{
    Bitboard occ = boardOccupancy(b);
    Bitboard us = b->colours[colour];
    Bitboard covered = 0ULL;

    Bitboard pawns = b->pieces[PAWN] & us;
    while (pawns)
        covered |= PawnAttacks[colour][poplsb(&pawns)];

    for (int type = KNIGHT; type <= KING; type++)
    {
        Bitboard bb = b->pieces[type] & us;
        while (bb)
            covered |= pieceAttacks(type, poplsb(&bb), occ);
    }

    return popcount(covered & ~us);
}

static bool applyIfLegal(Board* b, Move m, Undo* u)
{
    applyMove(b, m, u);
    int mover = !b->turn;
    if (squareAttacked(b, kingSquare(b, mover), b->turn))
    {
        revertMove(b, m, u);
        return false;
    }
    return true;
}

static bool squareMaximizationEvaluate(const Board* b, int* score)
{
    *score = mobilityCount(b, b->turn);
    return true;
}

static bool squareMaximizationChooseMove(Board* b, const SearchLimits* limits, SearchResult* result)
{
    (void)limits;

    result->bestMove = NO_MOVE;
    result->nodes = 0;
    result->hasScore = false;
    result->score = 0;

    Move moves[MAX_MOVES];
    int  n = generateAllMoves(b, moves);

    Move bestMove = NO_MOVE;
    int  bestScore = -1;

    for (int i = 0; i < n; i++)
    {
        Undo u;
        if (!applyIfLegal(b, moves[i], &u))
            continue;

        result->nodes++;
        int score = mobilityCount(b, !b->turn);
        revertMove(b, moves[i], &u);

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
