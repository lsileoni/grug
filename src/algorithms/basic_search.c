#include "basic_search.h"

#include <stdio.h>
#include <stddef.h>

#include "../algohelpers.h"
#include "../movegen.h"
#include "../util.h"

#define DEFAULT_DEPTH    3
#define MAX_SEARCH_DEPTH 8

typedef struct
{
    uint64_t  nodes;
    uint64_t  nodeLimit;
    long long startMs;
    long long timeLimitMs;
    bool      stopped;
} SearchContext;

static const int PieceValue[PIECE_TYPE_NB] = {100, 320, 330, 500, 900, 0};

// clang-format off
static const int PawnPst[SQUARE_NB] = {
      0,   0,   0,   0,   0,   0,   0,   0,
     50,  50,  50,  50,  50,  50,  50,  50,
     10,  10,  20,  30,  30,  20,  10,  10,
      5,   5,  10,  25,  25,  10,   5,   5,
      0,   0,   0,  20,  20,   0,   0,   0,
      5,  -5, -10,   0,   0, -10,  -5,   5,
      5,  10,  10, -20, -20,  10,  10,   5,
      0,   0,   0,   0,   0,   0,   0,   0,
};

static const int KnightPst[SQUARE_NB] = {
    -50, -40, -30, -30, -30, -30, -40, -50,
    -40, -20,   0,   5,   5,   0, -20, -40,
    -30,   5,  10,  15,  15,  10,   5, -30,
    -30,   0,  15,  20,  20,  15,   0, -30,
    -30,   5,  15,  20,  20,  15,   5, -30,
    -30,   0,  10,  15,  15,  10,   0, -30,
    -40, -20,   0,   0,   0,   0, -20, -40,
    -50, -40, -30, -30, -30, -30, -40, -50,
};

static const int BishopPst[SQUARE_NB] = {
    -20, -10, -10, -10, -10, -10, -10, -20,
    -10,   5,   0,   0,   0,   0,   5, -10,
    -10,  10,  10,  10,  10,  10,  10, -10,
    -10,   0,  10,  10,  10,  10,   0, -10,
    -10,   5,   5,  10,  10,   5,   5, -10,
    -10,   0,   5,  10,  10,   5,   0, -10,
    -10,   0,   0,   0,   0,   0,   0, -10,
    -20, -10, -10, -10, -10, -10, -10, -20,
};

static const int RookPst[SQUARE_NB] = {
      0,   0,   0,   5,   5,   0,   0,   0,
     -5,   0,   0,   0,   0,   0,   0,  -5,
     -5,   0,   0,   0,   0,   0,   0,  -5,
     -5,   0,   0,   0,   0,   0,   0,  -5,
     -5,   0,   0,   0,   0,   0,   0,  -5,
     -5,   0,   0,   0,   0,   0,   0,  -5,
      5,  10,  10,  10,  10,  10,  10,   5,
      0,   0,   0,   5,   5,   0,   0,   0,
};

static const int QueenPst[SQUARE_NB] = {
    -20, -10, -10,  -5,  -5, -10, -10, -20,
    -10,   0,   5,   0,   0,   0,   0, -10,
    -10,   5,   5,   5,   5,   5,   0, -10,
      0,   0,   5,   5,   5,   5,   0,  -5,
     -5,   0,   5,   5,   5,   5,   0,  -5,
    -10,   0,   5,   5,   5,   5,   0, -10,
    -10,   0,   0,   0,   0,   0,   0, -10,
    -20, -10, -10,  -5,  -5, -10, -10, -20,
};

static const int KingPst[SQUARE_NB] = {
     20,  30,  10,   0,   0,  10,  30,  20,
     20,  20,   0,   0,   0,   0,  20,  20,
    -10, -20, -20, -20, -20, -20, -20, -10,
    -20, -30, -30, -40, -40, -30, -30, -20,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
};
// clang-format on

static const int* pieceSquareTable(int type)
{
    switch (type)
    {
        case PAWN:
            return PawnPst;
        case KNIGHT:
            return KnightPst;
        case BISHOP:
            return BishopPst;
        case ROOK:
            return RookPst;
        case QUEEN:
            return QueenPst;
        case KING:
            return KingPst;
        default:
            return NULL;
    }
}

static int whiteStaticEval(const Board* b)
{
    int score = 0;

    for (int sq = A1; sq <= H8; sq++)
    {
        int piece = b->squares[sq];
        if (piece == EMPTY)
            continue;

        int        type = pieceType(piece);
        int        colour = pieceColour(piece);
        const int* pst = pieceSquareTable(type);
        int        pstSq = colour == WHITE ? sq : (sq ^ 56);
        int        value = PieceValue[type] + (pst ? pst[pstSq] : 0);

        score += colour == WHITE ? value : -value;
    }

    return score;
}

static int staticEval(const Board* b)
{
    int score = whiteStaticEval(b);
    return b->turn == WHITE ? score : -score;
}

static long long elapsedMs(const SearchContext* ctx)
{
    return timeNowMs() - ctx->startMs;
}

static bool outOfTime(const SearchContext* ctx)
{
    return ctx->timeLimitMs > 0 && elapsedMs(ctx) >= ctx->timeLimitMs;
}

static bool shouldStop(SearchContext* ctx)
{
    if (ctx->nodeLimit && ctx->nodes >= ctx->nodeLimit)
        ctx->stopped = true;
    if ((ctx->nodes & 255ULL) == 0 && outOfTime(ctx))
        ctx->stopped = true;
    return ctx->stopped;
}

static int moveScore(const Board* b, Move m)
{
    int score = 0;
    int attacker = b->squares[moveFrom(m)];
    int victim = b->squares[moveTo(m)];

    if (moveType(m) == EN_PASSANT)
        victim = makePiece(!b->turn, PAWN);
    if (victim != EMPTY)
        score += 10000 + PieceValue[pieceType(victim)] - PieceValue[pieceType(attacker)];
    if (moveType(m) == PROMOTION)
        score += 9000 + PieceValue[movePromoPiece(m)];
    if (moveType(m) == CASTLING)
        score += 50;

    return score;
}

static void orderMoves(const Board* b, Move* moves, int count)
{
    int scores[MAX_MOVES];
    for (int i = 0; i < count; i++)
        scores[i] = moveScore(b, moves[i]);

    for (int i = 1; i < count; i++)
    {
        Move m = moves[i];
        int  s = scores[i];
        int  j = i - 1;
        for (; j >= 0 && scores[j] < s; j--)
        {
            moves[j + 1] = moves[j];
            scores[j + 1] = scores[j];
        }
        moves[j + 1] = m;
        scores[j + 1] = s;
    }
}

static int quiesce(Board* b, int alpha, int beta, SearchContext* ctx)
{
    ctx->nodes++;
    int standPat = staticEval(b);
    if (shouldStop(ctx) || standPat >= beta)
        return standPat;
    if (standPat > alpha)
        alpha = standPat;

    Move moves[MAX_MOVES];
    int  n = generateNoisyMoves(b, moves);
    orderMoves(b, moves, n);

    int best = standPat;
    for (int i = 0; i < n; i++)
    {
        Undo u;
        if (!applyIfLegal(b, moves[i], &u))
            continue;

        int score = -quiesce(b, -beta, -alpha, ctx);
        revertMove(b, moves[i], &u);

        if (ctx->stopped)
            return score;
        if (score > best)
            best = score;
        if (score > alpha)
            alpha = score;
        if (alpha >= beta)
            break;
    }

    return best;
}

static int negamax(Board* b, int depth, int ply, int alpha, int beta, SearchContext* ctx)
{
    ctx->nodes++;
    if (shouldStop(ctx))
        return staticEval(b);

    if (boardIsDraw(b))
        return VALUE_DRAW;
    bool inCheck = boardInCheck(b);
    if ((depth <= 0 && !inCheck) || ply >= MAX_PLY)
        return quiesce(b, alpha, beta, ctx);

    Move moves[MAX_MOVES];
    int  n = generateAllMoves(b, moves);
    orderMoves(b, moves, n);

    bool foundLegal = false;
    int  best = -VALUE_INF;

    for (int i = 0; i < n; i++)
    {
        Undo u;
        if (!applyIfLegal(b, moves[i], &u))
            continue;

        foundLegal = true;
        int score = -negamax(b, depth - 1, ply + 1, -beta, -alpha, ctx);
        revertMove(b, moves[i], &u);

        if (ctx->stopped)
            return score;
        if (score > best)
            best = score;
        if (score > alpha)
            alpha = score;
        if (alpha >= beta)
            break;
    }

    if (!foundLegal)
        return inCheck ? -VALUE_MATE + ply : VALUE_DRAW;

    return best;
}

static int searchDepthFromLimits(const SearchLimits* limits)
{
    int depth = limits->depth > 0 ? limits->depth : DEFAULT_DEPTH;
    return depth > MAX_SEARCH_DEPTH ? MAX_SEARCH_DEPTH : depth;
}

static bool searchRoot(Board* b, int depth, SearchContext* ctx, Move* bestMove, int* bestScore)
{
    Move moves[MAX_MOVES];
    int  n = generateAllMoves(b, moves);
    orderMoves(b, moves, n);

    bool foundLegal = false;
    int  rootBestScore = -VALUE_INF;
    Move rootBestMove = NO_MOVE;
    int  alpha = -VALUE_INF;
    int  beta = VALUE_INF;

    for (int i = 0; i < n; i++)
    {
        Undo u;
        if (!applyIfLegal(b, moves[i], &u))
            continue;

        foundLegal = true;
        int score = -negamax(b, depth - 1, 1, -beta, -alpha, ctx);
        revertMove(b, moves[i], &u);

        // A stopped search returns an unfinished score; it must not pick a move.
        if (ctx->stopped)
            break;
        if (score > rootBestScore)
        {
            rootBestScore = score;
            rootBestMove = moves[i];
        }
        if (score > alpha)
            alpha = score;
    }

    *bestMove = rootBestMove;
    *bestScore = foundLegal ? rootBestScore : (boardInCheck(b) ? -VALUE_MATE : VALUE_DRAW);
    return foundLegal;
}

static void basicSearchChooseMove(Board* b, const SearchLimits* limits, SearchResult* result)
{
    long long timeLimitMs = timeBudgetMs(b, limits);
    int       targetDepth =
        (timeLimitMs > 0 && limits->depth <= 0) ? MAX_SEARCH_DEPTH : searchDepthFromLimits(limits);

    SearchContext ctx = {
        0, limits->nodes > 0 ? (uint64_t)limits->nodes : 0, timeNowMs(), timeLimitMs, false,
    };

    for (int depth = 1; depth <= targetDepth; depth++)
    {
        Move iterationMove = NO_MOVE;
        int  iterationScore = VALUE_DRAW;
        bool iterationFound = searchRoot(b, depth, &ctx, &iterationMove, &iterationScore);

        if (!iterationFound)
        {
            result->score = iterationScore;
            break;
        }

        if (!ctx.stopped)
        {
            result->bestMove = iterationMove;
            result->score = iterationScore;
            printf(
                "info depth %d score cp %d nodes %llu time %lld\n", depth, iterationScore,
                (unsigned long long)ctx.nodes, elapsedMs(&ctx)
            );
            fflush(stdout);
        }
        else if (result->bestMove == NO_MOVE && iterationMove != NO_MOVE)
        {
            result->bestMove = iterationMove;
            result->score = iterationScore;
        }

        if (ctx.stopped || outOfTime(&ctx))
            break;
    }

    result->nodes = ctx.nodes;
}

const Algorithm BasicSearchAlgorithm = {
    .name = "basic_search",
    .description = "alpha-beta search with quiescence and piece-square evaluation",
    .evaluate = staticEval,
    .chooseMove = basicSearchChooseMove,
};
