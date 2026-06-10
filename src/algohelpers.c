#include "algohelpers.h"

#include "attacks.h"
#include "bitboard.h"
#include "movegen.h"

// SEE-internal values; the king is worth more than any exchange so the swap
// never profits from losing it.
static const int SeeValue[PIECE_TYPE_NB] = {100, 320, 330, 500, 900, 30000};

// ---------------------------------------------------------------------------
// Moves & looking ahead
// ---------------------------------------------------------------------------

MoveList legalMoves(const Board* b)
{
    Board scratch = *b;
    Move  pseudo[MAX_MOVES];
    int   n = generateAllMoves(&scratch, pseudo);

    MoveList list;
    list.count = 0;
    for (int i = 0; i < n; i++)
        if (moveIsLegal(&scratch, pseudo[i]))
            list.moves[list.count++] = pseudo[i];
    return list;
}

Board boardAfter(const Board* b, Move m)
{
    Board after = *b;
    Undo  u;
    applyMove(&after, m, &u);
    return after;
}

bool moveGivesCheck(const Board* b, Move m)
{
    Board after = boardAfter(b, m);
    return boardInCheck(&after);
}

bool applyIfLegal(Board* b, Move m, Undo* u)
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

// ---------------------------------------------------------------------------
// Position state
// ---------------------------------------------------------------------------

bool isCheckmate(const Board* b)
{
    return boardInCheck(b) && legalMoves(b).count == 0;
}

bool isStalemate(const Board* b)
{
    return !boardInCheck(b) && legalMoves(b).count == 0;
}

// ---------------------------------------------------------------------------
// Squares & pieces
// ---------------------------------------------------------------------------

int pieceOn(const Board* b, int sq)
{
    return b->squares[sq];
}

int typeOn(const Board* b, int sq)
{
    int p = b->squares[sq];
    return p == EMPTY ? -1 : pieceType(p);
}

int colourOn(const Board* b, int sq)
{
    int p = b->squares[sq];
    return p == EMPTY ? -1 : pieceColour(p);
}

bool isEmpty(const Board* b, int sq)
{
    return b->squares[sq] == EMPTY;
}

int pieceValue(int type)
{
    static const int values[PIECE_TYPE_NB] = {100, 320, 330, 500, 900, 0};
    if (type < PAWN || type > KING)
        return 0;
    return values[type];
}

// ---------------------------------------------------------------------------
// Vision & attackers
// ---------------------------------------------------------------------------

Bitboard sees(const Board* b, int sq)
{
    int p = b->squares[sq];
    if (p == EMPTY)
        return 0ULL;
    int type = pieceType(p);
    if (type == PAWN)
        return PawnAttacks[pieceColour(p)][sq];
    return pieceAttacks(type, sq, boardOccupancy(b));
}

// `occ` may omit pieces (the SEE swap removes them to expose x-ray attackers);
// intersect the result with `occ` to keep only pieces still present.
static Bitboard attackersToOcc(const Board* b, int sq, Bitboard occ)
{
    Bitboard attackers = 0ULL;
    attackers |= PawnAttacks[WHITE][sq] & b->pieces[PAWN] & b->colours[BLACK];
    attackers |= PawnAttacks[BLACK][sq] & b->pieces[PAWN] & b->colours[WHITE];
    attackers |= KnightAttacks[sq] & b->pieces[KNIGHT];
    attackers |= KingAttacks[sq] & b->pieces[KING];
    attackers |= bishopAttacks(sq, occ) & (b->pieces[BISHOP] | b->pieces[QUEEN]);
    attackers |= rookAttacks(sq, occ) & (b->pieces[ROOK] | b->pieces[QUEEN]);
    return attackers;
}

Bitboard attackersTo(const Board* b, int sq)
{
    return attackersToOcc(b, sq, boardOccupancy(b));
}

Bitboard attackersOf(const Board* b, int sq, int colour)
{
    return attackersTo(b, sq) & b->colours[colour];
}

bool isAttacked(const Board* b, int sq, int byColour)
{
    return attackersOf(b, sq, byColour) != 0ULL;
}

bool isDefended(const Board* b, int sq)
{
    int colour = colourOn(b, sq);
    if (colour < 0)
        return false;
    return (attackersTo(b, sq) & b->colours[colour]) != 0ULL;
}

// ---------------------------------------------------------------------------
// Move consequences
// ---------------------------------------------------------------------------

bool moveIsCapture(const Board* b, Move m)
{
    if (moveType(m) == EN_PASSANT)
        return true;
    return b->squares[moveTo(m)] != EMPTY;
}

int moveCaptured(const Board* b, Move m)
{
    if (moveType(m) == EN_PASSANT)
        return PAWN;
    int p = b->squares[moveTo(m)];
    return p == EMPTY ? -1 : pieceType(p);
}

int captureGain(const Board* b, Move m)
{
    int victim = moveCaptured(b, m);
    if (victim < 0)
        return 0;
    int attacker = pieceType(b->squares[moveFrom(m)]);
    return pieceValue(victim) - pieceValue(attacker);
}

// The cheapest piece in `set` as a single-square bitboard (0 if empty), its
// SeeValue written to `*valueOut`.
static Bitboard leastValuableAttacker(const Board* b, Bitboard set, int* valueOut)
{
    for (int type = PAWN; type <= KING; type++)
    {
        Bitboard pieces = set & b->pieces[type];
        if (pieces != 0ULL)
        {
            *valueOut = SeeValue[type];
            return squareBB(getlsb(pieces));
        }
    }
    return 0ULL;
}

int see(const Board* b, Move m)
{
    int from = moveFrom(m);
    int to = moveTo(m);
    int type = moveType(m);
    int us = pieceColour(b->squares[from]);

    Bitboard occ = boardOccupancy(b);

    int gain[32];
    int depth = 0;

    int onSquare = SeeValue[pieceType(b->squares[from])];
    if (type == EN_PASSANT)
    {
        gain[0] = SeeValue[PAWN];
        occ ^= squareBB(to + (us == WHITE ? -8 : 8));
    }
    else
    {
        int victim = b->squares[to];
        gain[0] = victim == EMPTY ? 0 : SeeValue[pieceType(victim)];
    }
    if (type == PROMOTION)
    {
        int promo = movePromoPiece(m);
        gain[0] += SeeValue[promo] - SeeValue[PAWN];
        onSquare = SeeValue[promo];
    }

    occ ^= squareBB(from);
    occ |= squareBB(to);

    int      side = us ^ 1;
    Bitboard attackers = attackersToOcc(b, to, occ) & occ;

    while (true)
    {
        int      attackerValue = 0;
        Bitboard next = leastValuableAttacker(b, attackers & b->colours[side], &attackerValue);
        if (next == 0ULL)
            break;
        if (depth + 1 >= (int)(sizeof gain / sizeof gain[0]))
            break;

        depth++;
        gain[depth] = onSquare - gain[depth - 1];

        // Neither continuing nor standing pat helps the side to move: prune.
        int bestSoFar = -gain[depth - 1] > gain[depth] ? -gain[depth - 1] : gain[depth];
        if (bestSoFar < 0)
            break;

        onSquare = attackerValue;
        occ ^= next;
        attackers = attackersToOcc(b, to, occ) & occ;
        side ^= 1;
    }

    // Resolve the exchange backwards; each side may stand pat.
    while (depth > 0)
    {
        gain[depth - 1] = -(-gain[depth - 1] > gain[depth] ? -gain[depth - 1] : gain[depth]);
        depth--;
    }
    return gain[0];
}

// ---------------------------------------------------------------------------
// Threats & safety
// ---------------------------------------------------------------------------

bool isHanging(const Board* b, int sq)
{
    int colour = colourOn(b, sq);
    if (colour < 0)
        return false;
    return isAttacked(b, sq, colour ^ 1) && !isDefended(b, sq);
}

Bitboard hangingPieces(const Board* b, int colour)
{
    Bitboard out = 0ULL;
    Bitboard bb = b->colours[colour];
    int      sq;
    while ((sq = popNextSquare(&bb)) != SQ_NONE)
        if (isAttacked(b, sq, colour ^ 1) && !isDefended(b, sq))
            out |= squareBB(sq);
    return out;
}

int hangingValue(const Board* b, int colour)
{
    Bitboard hanging = hangingPieces(b, colour);
    int      total = 0;
    int      sq;
    while ((sq = popNextSquare(&hanging)) != SQ_NONE)
        total += pieceValue(typeOn(b, sq));
    return total;
}

// ---------------------------------------------------------------------------
// Pawn structure
// ---------------------------------------------------------------------------

// Squares strictly ahead of `sq` on its own file, from `colour`'s viewpoint.
static Bitboard frontSpan(int colour, int sq)
{
    Bitboard front = colour == WHITE ? shiftNorth(squareBB(sq)) : shiftSouth(squareBB(sq));
    front |= colour == WHITE ? front << 8 : front >> 8;
    front |= colour == WHITE ? front << 16 : front >> 16;
    front |= colour == WHITE ? front << 32 : front >> 32;
    return front;
}

bool isPassedPawn(const Board* b, int sq)
{
    if (typeOn(b, sq) != PAWN)
        return false;
    int      colour = colourOn(b, sq);
    Bitboard front = frontSpan(colour, sq);
    Bitboard lane = front | shiftEast(front) | shiftWest(front);
    return !(boardPieces(b, colour ^ 1, PAWN) & lane);
}

bool isIsolatedPawn(const Board* b, int sq)
{
    if (typeOn(b, sq) != PAWN)
        return false;
    Bitboard file = FileBB[fileOf(sq)];
    Bitboard adjacent = shiftEast(file) | shiftWest(file);
    return !(boardPieces(b, colourOn(b, sq), PAWN) & adjacent);
}

bool isDoubledPawn(const Board* b, int sq)
{
    if (typeOn(b, sq) != PAWN)
        return false;
    return several(boardPieces(b, colourOn(b, sq), PAWN) & FileBB[fileOf(sq)]);
}

// ---------------------------------------------------------------------------
// Material & mobility
// ---------------------------------------------------------------------------

int materialCount(const Board* b, int colour, int type)
{
    return popcount(boardPieces(b, colour, type));
}

int materialValue(const Board* b, int colour)
{
    int value = 0;
    for (int type = PAWN; type <= QUEEN; type++)
        value += materialCount(b, colour, type) * pieceValue(type);
    return value;
}

int materialBalance(const Board* b, int colour)
{
    return materialValue(b, colour) - materialValue(b, colour ^ 1);
}

Bitboard sideAttacks(const Board* b, int colour)
{
    Bitboard occ = boardOccupancy(b);
    Bitboard us = b->colours[colour];
    Bitboard covered = 0ULL;
    int      sq;

    Bitboard pawns = b->pieces[PAWN] & us;
    while ((sq = popNextSquare(&pawns)) != SQ_NONE)
        covered |= PawnAttacks[colour][sq];

    for (int type = KNIGHT; type <= KING; type++)
    {
        Bitboard bb = b->pieces[type] & us;
        while ((sq = popNextSquare(&bb)) != SQ_NONE)
            covered |= pieceAttacks(type, sq, occ);
    }
    return covered;
}

int mobility(const Board* b, int colour)
{
    return popcount(sideAttacks(b, colour) & ~b->colours[colour]);
}

// ---------------------------------------------------------------------------
// Time
// ---------------------------------------------------------------------------

enum
{
    MOVE_OVERHEAD_MS = 100,  // margin for I/O latency so we never flag
    INFINITE_THINK_MS = 1000 // "go infinite" budget; grug has no stop handling
};

long long timeBudgetMs(const Board* b, const SearchLimits* limits)
{
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
        return INFINITE_THINK_MS;
    return 0;
}

// ---------------------------------------------------------------------------
// Iteration & sides
// ---------------------------------------------------------------------------

int popNextSquare(Bitboard* bb)
{
    if (*bb == 0ULL)
        return SQ_NONE;
    return poplsb(bb);
}

int sideToMove(const Board* b)
{
    return b->turn;
}
