#include "basic_search.h"

#include <stdio.h>
#include <stddef.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

#include "../algohelpers.h"
#include "../movegen.h"

#define DEFAULT_DEPTH       3
#define MAX_SEARCH_DEPTH    8
#define DEFAULT_INFINITE_MS 1000
#define MOVE_OVERHEAD_MS    100

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

static bool basicSearchEvaluate(const Board* b, int* score)
{
    *score = staticEval(b);
    return true;
}

static long long nowMs(void)
{
#ifdef _WIN32
    static LARGE_INTEGER frequency;
    static bool          initialized = false;
    LARGE_INTEGER        counter;
    if (!initialized)
    {
        QueryPerformanceFrequency(&frequency);
        initialized = true;
    }
    QueryPerformanceCounter(&counter);
    return (long long)(counter.QuadPart * 1000 / frequency.QuadPart);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}

static long long elapsedMs(const SearchContext* ctx)
{
    return nowMs() - ctx->startMs;
}

static bool outOfTime(const SearchContext* ctx)
{
    return ctx->timeLimitMs > 0 && elapsedMs(ctx) >= ctx->timeLimitMs;
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
    for (int i = 0; i < count - 1; i++)
    {
        int best = i;
        int bestScore = moveScore(b, moves[i]);
        for (int j = i + 1; j < count; j++)
        {
            int score = moveScore(b, moves[j]);
            if (score > bestScore)
            {
                best = j;
                bestScore = score;
            }
        }
        if (best != i)
        {
            Move tmp = moves[i];
            moves[i] = moves[best];
            moves[best] = tmp;
        }
    }
}

static int negamax(Board* b, int depth, int alpha, int beta, SearchContext* ctx)
{
    ctx->nodes++;
    if (ctx->nodeLimit && ctx->nodes >= ctx->nodeLimit)
        ctx->stopped = true;
    if ((ctx->nodes & 255ULL) == 0 && outOfTime(ctx))
        ctx->stopped = true;
    if (ctx->stopped)
        return staticEval(b);

    if (boardIsDraw(b))
        return VALUE_DRAW;
    bool inCheck = boardInCheck(b);
    if (depth <= 0 && !inCheck)
        return staticEval(b);

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
        int score = -negamax(b, depth - 1, -beta, -alpha, ctx);
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
        return inCheck ? -VALUE_MATE + b->ply : VALUE_DRAW;

    return best;
}

static int searchDepthFromLimits(const SearchLimits* limits)
{
    int depth = DEFAULT_DEPTH;
    if (limits && limits->depth > 0)
        depth = limits->depth;
    if (depth > MAX_SEARCH_DEPTH)
        depth = MAX_SEARCH_DEPTH;
    return depth;
}

static long long searchTimeFromLimits(const Board* b, const SearchLimits* limits)
{
    if (!limits)
        return 0;
    if (limits->movetime > 0)
        return limits->movetime > MOVE_OVERHEAD_MS ? limits->movetime - MOVE_OVERHEAD_MS : 1;

    long long remaining = b->turn == WHITE ? limits->wtime : limits->btime;
    long long increment = b->turn == WHITE ? limits->winc : limits->binc;
    if (remaining > 0)
    {
        int       movesToGo = limits->movestogo > 0 ? limits->movestogo : 30;
        long long budget = remaining / movesToGo + increment / 2;
        long long maxBudget = remaining > MOVE_OVERHEAD_MS ? remaining - MOVE_OVERHEAD_MS : 1;
        if (budget > maxBudget)
            budget = maxBudget;
        if (budget < 1)
            budget = 1;
        return budget;
    }

    if (limits->infinite)
        return DEFAULT_INFINITE_MS;
    return 0;
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
        int score = -negamax(b, depth - 1, -beta, -alpha, ctx);
        revertMove(b, moves[i], &u);

        if (score > rootBestScore)
        {
            rootBestScore = score;
            rootBestMove = moves[i];
        }
        if (score > alpha)
            alpha = score;
        if (ctx->stopped)
            break;
    }

    *bestMove = rootBestMove;
    *bestScore = foundLegal ? rootBestScore : (boardInCheck(b) ? -VALUE_MATE + b->ply : VALUE_DRAW);
    return foundLegal;
}

static bool basicSearchChooseMove(Board* b, const SearchLimits* limits, SearchResult* result)
{
    result->bestMove = NO_MOVE;
    result->nodes = 0;
    result->hasScore = false;
    result->score = 0;

    long long     timeLimitMs = searchTimeFromLimits(b, limits);
    int           targetDepth = (timeLimitMs > 0 && (!limits || limits->depth <= 0))
                                    ? MAX_SEARCH_DEPTH
                                    : searchDepthFromLimits(limits);
    SearchContext ctx = {
        0, limits && limits->nodes > 0 ? (uint64_t)limits->nodes : 0, nowMs(), timeLimitMs, false,
    };

    Move bestMove = NO_MOVE;
    int  bestScore = VALUE_DRAW;
    bool foundLegal = false;

    for (int depth = 1; depth <= targetDepth; depth++)
    {
        Move iterationMove = NO_MOVE;
        int  iterationScore = VALUE_DRAW;
        bool iterationFound = searchRoot(b, depth, &ctx, &iterationMove, &iterationScore);

        if (iterationFound && !ctx.stopped)
        {
            foundLegal = true;
            bestMove = iterationMove;
            bestScore = iterationScore;
            printf(
                "info depth %d score cp %d nodes %llu time %lld\n", depth, bestScore,
                (unsigned long long)ctx.nodes, elapsedMs(&ctx)
            );
            fflush(stdout);
        }
        else if (!foundLegal && iterationFound)
        {
            foundLegal = true;
            bestMove = iterationMove;
            bestScore = iterationScore;
        }
        else if (!iterationFound)
        {
            bestScore = iterationScore;
            break;
        }

        if (ctx.stopped || outOfTime(&ctx))
            break;
    }

    result->nodes = ctx.nodes;
    result->bestMove = bestMove;
    result->hasScore = foundLegal || bestMove == NO_MOVE;
    result->score = foundLegal ? bestScore : (boardInCheck(b) ? -VALUE_MATE + b->ply : VALUE_DRAW);
    return true;
}

const Algorithm BasicSearchAlgorithm = {
    "basic_search",
    "depth-limited material search with alpha-beta pruning",
    NULL,
    NULL,
    basicSearchEvaluate,
    basicSearchChooseMove,
};
