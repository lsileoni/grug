#ifndef GRUG_ALGOHELPERS_H
#define GRUG_ALGOHELPERS_H

#include "board.h"
#include "search.h"

// Chess questions for algorithms. Moves come out already legal; looking ahead
// returns a copy, so there is nothing to revert.

// ---------------------------------------------------------------------------
// Moves
// ---------------------------------------------------------------------------

typedef struct
{
    Move moves[MAX_MOVES];
    int  count;
} MoveList;

// All legal moves, with correct castling/promotion/en-passant flags.
MoveList legalMoves(const Board* b);

// ---------------------------------------------------------------------------
// Looking ahead
// ---------------------------------------------------------------------------

// The position after `m`, as a copy; `b` is untouched. In the copy it is the
// opponent's turn.
Board boardAfter(const Board* b, Move m);

// Whether `m` checks the opponent.
bool moveGivesCheck(const Board* b, Move m);

// ---------------------------------------------------------------------------
// Position state (see also boardInCheck and boardIsDraw in board.h)
// ---------------------------------------------------------------------------

// Whether the side to move is checkmated.
bool isCheckmate(const Board* b);

// Whether the side to move has no legal move but is not in check.
bool isStalemate(const Board* b);

// ---------------------------------------------------------------------------
// Move consequences
// ---------------------------------------------------------------------------

// Whether `m` captures (en passant counts).
bool moveIsCapture(const Board* b, Move m);

// The piece type `m` captures (PAWN for en passant), or -1 for a non-capture.
int moveCaptured(const Board* b, Move m);

// Victim value minus attacker value; 0 for a non-capture. Ignores recaptures -
// use see() for the real outcome.
int captureGain(const Board* b, Move m);

// Static exchange evaluation: net centipawns for the mover after the full
// capture sequence on the destination square. Positive wins material.
int see(const Board* b, Move m);

// ---------------------------------------------------------------------------
// Squares & pieces
// ---------------------------------------------------------------------------

// The piece on `sq` (e.g. W_KNIGHT), or EMPTY.
int pieceOn(const Board* b, int sq);

// The piece type on `sq` (PAWN..KING), or -1 if empty.
int typeOn(const Board* b, int sq);

// The owner of the piece on `sq` (WHITE/BLACK), or -1 if empty.
int colourOn(const Board* b, int sq);

// Whether `sq` holds no piece.
bool isEmpty(const Board* b, int sq);

// Centipawn value of a piece type: pawn 100 .. queen 900, king 0.
int pieceValue(int type);

// ---------------------------------------------------------------------------
// Vision & attackers
// ---------------------------------------------------------------------------

// The squares the piece on `sq` attacks; 0 for an empty square.
Bitboard sees(const Board* b, int sq);

// All pieces of both colours attacking `sq`.
Bitboard attackersTo(const Board* b, int sq);

// `colour`'s pieces attacking `sq`.
Bitboard attackersOf(const Board* b, int sq, int colour);

// Whether `byColour` attacks `sq`.
bool isAttacked(const Board* b, int sq, int byColour);

// Whether the piece on `sq` has a friendly defender. False for empty squares.
bool isDefended(const Board* b, int sq);

// ---------------------------------------------------------------------------
// Threats & safety
// ---------------------------------------------------------------------------

// Whether the piece on `sq` is attacked and undefended. False for empty squares.
bool isHanging(const Board* b, int sq);

// `colour`'s hanging pieces.
Bitboard hangingPieces(const Board* b, int colour);

// Total centipawn value of `colour`'s hanging pieces.
int hangingValue(const Board* b, int colour);

// ---------------------------------------------------------------------------
// Pawn structure (all false when `sq` does not hold a pawn)
// ---------------------------------------------------------------------------

// No enemy pawn ahead on this or an adjacent file.
bool isPassedPawn(const Board* b, int sq);

// No friendly pawn on an adjacent file.
bool isIsolatedPawn(const Board* b, int sq);

// Two or more friendly pawns on this file.
bool isDoubledPawn(const Board* b, int sq);

// ---------------------------------------------------------------------------
// Material & mobility
// ---------------------------------------------------------------------------

// How many of `colour`'s pieces are of `type`.
int materialCount(const Board* b, int colour, int type);

// Total centipawn value of `colour`'s pieces, king excluded.
int materialValue(const Board* b, int colour);

// `colour`'s material minus the opponent's.
int materialBalance(const Board* b, int colour);

// Every square `colour` attacks (pawns: capture diagonals, not pushes).
Bitboard sideAttacks(const Board* b, int colour);

// How many squares `colour` attacks that its own pieces do not occupy.
int mobility(const Board* b, int colour);

// ---------------------------------------------------------------------------
// Time
// ---------------------------------------------------------------------------

// Milliseconds to spend on this move per the UCI limits, safety margin
// included; 0 when unconstrained. Pair with timeNowMs() from util.h.
long long timeBudgetMs(const Board* b, const SearchLimits* limits);

// ---------------------------------------------------------------------------
// Iteration & sides
// ---------------------------------------------------------------------------

// Remove and return the lowest square of a bitboard, or SQ_NONE when empty:
//     for (int sq; (sq = popNextSquare(&bb)) != SQ_NONE; ) ...
int popNextSquare(Bitboard* bb);

// The side whose turn it is.
int sideToMove(const Board* b);

// ---------------------------------------------------------------------------
// The fast path (deep searches only; everything above is simpler)
// ---------------------------------------------------------------------------

// Apply `m` in place if it leaves the own king safe; otherwise change nothing
// and return false. Every applied move must be revertMove()d. See basic_search.c.
bool applyIfLegal(Board* b, Move m, Undo* u);

#endif
