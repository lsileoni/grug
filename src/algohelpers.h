#ifndef GRUG_ALGOHELPERS_H
#define GRUG_ALGOHELPERS_H

#include "board.h"
#include "search.h"

// ---------------------------------------------------------------------------
// Squares & pieces
// ---------------------------------------------------------------------------

// The piece on a square, e.g. W_KNIGHT, or EMPTY. Returns colour and type
// together; use typeOn/colourOn when you only want one of them.
int pieceOn(const Board* b, int sq);

// The kind of piece on a square (PAWN..KING), or -1 if the square is empty.
// Handy for branching on what is standing somewhere ("is this a pawn?").
int typeOn(const Board* b, int sq);

// Which side owns the piece on a square (WHITE/BLACK), or -1 if empty. Tells
// friend from foe on a target or destination square.
int colourOn(const Board* b, int sq);

// Whether a square holds no piece. Use it to test that a destination, or a
// square a piece must pass over, is free.
bool isEmpty(const Board* b, int sq);

// Rough centipawn worth of a piece type (pawn 100 .. queen 900, king 0). The
// common currency for material maths: comparing trades, weighting targets,
// summing up a side's material.
int pieceValue(int type);

// ---------------------------------------------------------------------------
// Vision & attackers
// ---------------------------------------------------------------------------

// The squares the piece on `sq` currently attacks: a knight's jumps, a slider's
// rays up to the first blocker, a pawn's two capture diagonals (0 for an empty
// square). Use it to ask "what does this piece hit?"  list a knight's targets,
// see if a rook eyes the enemy king, measure how much a bishop controls.
Bitboard sees(const Board* b, int sq);

// Every piece of either colour that attacks `sq` right now. Use it to weigh up a
// contested square (who is fighting over it) before you commit a piece to it.
Bitboard attackersTo(const Board* b, int sq);

// The attackers of `sq` belonging to one side only. Use it to count a square's
// defenders (your colour) or the threats against it (the enemy).
Bitboard attackersOf(const Board* b, int sq, int colour);

// Whether `byColour` attacks `sq` at all  the cheap yes/no when you do not need
// the attackers themselves, e.g. "is this landing square covered by the enemy?".
bool isAttacked(const Board* b, int sq, int byColour);

// Whether the piece on `sq` is backed up by a friendly piece. Pair it with
// isAttacked to decide whether a piece is safe or loose. False for empty squares.
bool isDefended(const Board* b, int sq);

// ---------------------------------------------------------------------------
// Move consequences
// ---------------------------------------------------------------------------

// Whether `m` takes an enemy piece (en passant counts). Use it to separate
// captures from quiet moves  to score them differently, or to feed only the
// captures to see().
bool moveIsCapture(const Board* b, Move m);

// The type of piece `m` would capture (PAWN for en passant), or -1 if `m` is not
// a capture. Tells you the prize before you play; pairs naturally with pieceValue.
int moveCaptured(const Board* b, Move m);

// Whether playing `m` would leave the opponent in check. Use it to find or prefer
// forcing moves. It briefly makes and unmakes `m`, hence the writable `Board*`.
bool moveGivesCheck(Board* b, Move m);

// A quick material estimate of a capture: value taken minus value of the capturer
// (0 for a non-capture). A fast first cut only  it ignores recaptures, so use
// see() when you need to know a capture is actually safe.
int captureGain(const Board* b, Move m);

// Static exchange evaluation: plays out the full capture-and-recapture sequence on
// the move's destination square, each side using its cheapest attacker, and
// returns the net material in centipawns from the mover's point of view (positive
// = comes out ahead). The reliable way to ask "is this capture safe or winning?";
// it also flags quiet moves that step a piece onto a square the enemy would win.
int see(const Board* b, Move m);

// A question you want answered about a position: given a board and your `ctx`,
// return a number. Used with afterMove to evaluate the position a move leads to.
typedef int (*BoardQueryFn)(const Board* afterMove, void* ctx);

// Plays `m`, runs your `fn` on the position it produces, takes the move back, and
// returns whatever `fn` returned  one-ply lookahead without writing make/unmake
// yourself. Use it to score the resulting position: your mobility after the move,
// whether you left anything hanging, your material once the dust settles.
int afterMove(Board* b, Move m, BoardQueryFn fn, void* ctx);

// ---------------------------------------------------------------------------
// Threats & safety
// ---------------------------------------------------------------------------

// Whether the piece on `sq` is attacked by the enemy and has no defender of its
// own. Spots loose pieces  yours to rescue, or the opponent's to grab. False for
// an empty square.
bool isHanging(const Board* b, int sq);

// All of `colour`'s pieces that are hanging right now (see isHanging). Use it to
// total up loose material: penalise moves that leave your own pieces undefended,
// or find the enemy's free pieces.
Bitboard hangingPieces(const Board* b, int colour);

// ---------------------------------------------------------------------------
// Material & mobility
// ---------------------------------------------------------------------------

// How many pieces of one type a side has (e.g. White's knights). Useful for phase
// or endgame checks and piece-specific heuristics.
int materialCount(const Board* b, int colour, int type);

// Total centipawn worth of a side's pieces, kings aside. The material term of an
// evaluation.
int materialValue(const Board* b, int colour);

// A side's material minus the opponent's (positive = that side is up material).
// The usual "am I ahead?" number, measured from the colour you pass.
int materialBalance(const Board* b, int colour);

// The union of every square a side's pieces attack (pawns count their capture
// diagonals, not pushes). Use it for space and coverage ideas  squares you
// control, or whether you cover the enemy king's area.
Bitboard sideAttacks(const Board* b, int colour);

// How many squares a side attacks that are not blocked by its own pieces: a
// one-number "how active am I?". Use it as a mobility term in an eval, or as a
// whole heuristic on its own (see square_maximization).
int mobility(const Board* b, int colour);

// ---------------------------------------------------------------------------
// Move lists, legality, and iteration
// ---------------------------------------------------------------------------

// Clear a SearchResult to its empty state (no move, zero nodes, no score). Call
// it once at the top of chooseMove so you only set the fields you actually fill.
void searchResultInit(SearchResult* r);

// Fill `out` with just the legal moves and return how many  no pseudo-legal
// moves to filter yourself. The usual way to begin a root move loop. `out` must
// have room for MAX_MOVES.
int legalMoves(Board* b, Move* out);

// Try to play `m`: if it would leave your own king in check it changes nothing
// and returns false; otherwise it makes the move (you call revertMove afterwards)
// and returns true. Use it when you walk pseudo-legal moves yourself and want to
// skip the illegal ones.
bool applyIfLegal(Board* b, Move m, Undo* u);

// Remove and return the lowest square in a bitboard, or SQ_NONE once it is empty,
// so you can walk a bitboard's squares with a plain loop:
//     for (int sq; (sq = popNextSquare(&bb)) != SQ_NONE; ) { ... }
// Typical with the squares from sees(), hangingPieces(), or a side's pieces.
int popNextSquare(Bitboard* bb);

// The side whose turn it is.
int sideToMove(const Board* b);

// The other side  i.e. the side that just moved. This is usually what you want
// inside an afterMove query, where the turn has already flipped to the opponent.
int moverSide(const Board* b);

// ---------------------------------------------------------------------------
// Shortcut: score every move and keep the best
// ---------------------------------------------------------------------------

// Your scoring function: given the position after a candidate move and the side
// that made it (`mover`), return a score - bigger is better.
typedef int (*MoveEvalFn)(const Board* afterMove, int mover, void* ctx);

// Tries every legal move, scores the position each one produces with your `eval`,
// and fills `result` with the highest-scoring move - generation, make/unmake and
// bookkeeping done for you. A shortcut for algorithms whose entire logic is "score
// each move, keep the best"; if yours has any other shape, write the loop yourself
// with the functions above so the logic stays visible. Always returns true.
bool chooseHighestScoring(Board* b, SearchResult* result, MoveEvalFn eval, void* ctx);

#endif
