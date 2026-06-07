#include "algohelpers.h"

#include "attacks.h"
#include "bitboard.h"
#include "movegen.h"

// Piece values used internally by the static exchange evaluator. The king is
// given a value far larger than any realistic exchange so the swap resolution
// never "wins" material by giving up the king. Public material helpers use
// pieceValue() instead, where the king is worth 0.
static const int SeeValue[PIECE_TYPE_NB] = {100, 320, 330, 500, 900, 30000};

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

// Attackers of `sq` among all pieces, evaluating slider rays against a supplied
// occupancy. The occupancy may differ from the real board (the SEE swap removes
// captured pieces from it to reveal x-ray attackers), so callers that want only
// pieces still on the board should intersect the result with that occupancy.
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

bool moveGivesCheck(Board* b, Move m)
{
    Undo u;
    applyMove(b, m, &u);
    bool check = boardInCheck(b);
    revertMove(b, m, &u);
    return check;
}

int captureGain(const Board* b, Move m)
{
    int victim = moveCaptured(b, m);
    if (victim < 0)
        return 0;
    int attacker = pieceType(b->squares[moveFrom(m)]);
    return pieceValue(victim) - pieceValue(attacker);
}

// Least valuable attacker of `side` within `set` (already restricted to that
// side's attacking pieces). Returns the single-square bitboard of the chosen
// piece and writes its SeeValue to `*valueOut`, or 0 if `set` is empty.
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

    // Value captured by the initial move, and the value of the piece that ends up
    // standing on `to` (which the opponent may now recapture).
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

    // Make the initial move on the occupancy: the attacker leaves `from` and now
    // sits on `to`.
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

        // Stop once the side to move cannot improve on simply standing pat.
        int bestSoFar = -gain[depth - 1] > gain[depth] ? -gain[depth - 1] : gain[depth];
        if (bestSoFar < 0)
            break;

        onSquare = attackerValue; // the recapturing piece now stands on `to`
        occ ^= next;
        attackers = attackersToOcc(b, to, occ) & occ;
        side ^= 1;
    }

    // Resolve the gain stack with negamax-style minimaxing of the exchange.
    while (depth > 0)
    {
        gain[depth - 1] = -(-gain[depth - 1] > gain[depth] ? -gain[depth - 1] : gain[depth]);
        depth--;
    }
    return gain[0];
}

int afterMove(Board* b, Move m, BoardQueryFn fn, void* ctx)
{
    Undo u;
    applyMove(b, m, &u);
    int result = fn(b, ctx);
    revertMove(b, m, &u);
    return result;
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
// Convenience
// ---------------------------------------------------------------------------

void searchResultInit(SearchResult* r)
{
    r->bestMove = NO_MOVE;
    r->nodes = 0;
    r->hasScore = false;
    r->score = 0;
}

int legalMoves(Board* b, Move* out)
{
    Move pseudo[MAX_MOVES];
    int  n = generateAllMoves(b, pseudo);
    int  count = 0;
    for (int i = 0; i < n; i++)
        if (moveIsLegal(b, pseudo[i]))
            out[count++] = pseudo[i];
    return count;
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

int moverSide(const Board* b)
{
    return !b->turn;
}

// ---------------------------------------------------------------------------
// Move evaluator
// ---------------------------------------------------------------------------

bool chooseHighestScoring(Board* b, SearchResult* result, MoveEvalFn eval, void* ctx)
{
    searchResultInit(result);

    Move moves[MAX_MOVES];
    int  n = legalMoves(b, moves);

    Move best = NO_MOVE;
    int  bestScore = 0;

    for (int i = 0; i < n; i++)
    {
        Undo u;
        applyMove(b, moves[i], &u);
        int mover = !b->turn;
        int score = eval(b, mover, ctx);
        revertMove(b, moves[i], &u);

        result->nodes++;
        if (best == NO_MOVE || score > bestScore)
        {
            bestScore = score;
            best = moves[i];
        }
    }

    result->bestMove = best;
    if (best != NO_MOVE)
    {
        result->hasScore = true;
        result->score = bestScore;
    }
    return true;
}
