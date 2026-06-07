#include "threat_aware.h"

#include <stddef.h>

#include "../algohelpers.h"

// threat_aware: Scores every legal move and plays the
// highest, where a move's score is:
//
//     + the material a capture safely wins                  (see)
//     + a small bonus for giving check                      (moveGivesCheck)
//     - the value of our own pieces it leaves hanging       (hangingPieces)
//
// It looks exactly one move ahead, so it grabs free material and obvious checks
// and avoids blunders into undefended squares, but has no plan beyond that.

#define CHECK_BONUS 40

// Total value of the mover's pieces left hanging in a position (attacked by the
// enemy with no defender).
static int moverHangingValue(const Board* b, void* ctx)
{
    (void)ctx;
    int      mover = moverSide(b);
    Bitboard hanging = hangingPieces(b, mover);
    int      total = 0;
    int      sq;
    while ((sq = popNextSquare(&hanging)) != SQ_NONE)
        total += pieceValue(typeOn(b, sq));
    return total;
}

static bool threatAwareChooseMove(Board* b, const SearchLimits* limits, SearchResult* result)
{
    (void)limits;
    searchResultInit(result);

    Move moves[MAX_MOVES];
    int  n = legalMoves(b, moves);

    Move bestMove = NO_MOVE;
    int  bestScore = 0;

    for (int i = 0; i < n; i++)
    {
        Move m = moves[i];
        result->nodes++;

        int score = 0;

        // Count a capture only if it actually wins material, so we are not lured
        // into taking a defended piece with a more valuable one.
        if (moveIsCapture(b, m))
        {
            int exchange = see(b, m);
            if (exchange > 0)
                score += exchange;
        }

        // Lean toward forcing moves.
        if (moveGivesCheck(b, m))
            score += CHECK_BONUS;

        // Discourage moves that leave our own pieces undefended.
        score -= afterMove(b, m, moverHangingValue, NULL);

        if (bestMove == NO_MOVE || score > bestScore)
        {
            bestScore = score;
            bestMove = m;
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

const Algorithm ThreatAwareAlgorithm = {
    "threat_aware", "win material by SEE, give checks, avoid hanging pieces",
    NULL,           NULL,
    NULL,           threatAwareChooseMove,
};
