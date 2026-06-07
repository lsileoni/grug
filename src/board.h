#ifndef GRUG_BOARD_H
#define GRUG_BOARD_H

#include "types.h"
#include "move.h"
#include "bitboard.h"

#define STARTPOS_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

typedef struct
{
    int      captured;
    int      epSquare;
    int      castlingRights;
    int      halfmoveClock;
    uint64_t hash;
} Undo;

typedef struct
{
    Bitboard pieces[PIECE_TYPE_NB];
    Bitboard colours[COLOUR_NB];
    int      squares[SQUARE_NB];

    int turn;
    int castlingRights;
    int epSquare;
    int halfmoveClock;
    int fullmoveNumber;

    uint64_t hash;
    int      ply;

    uint64_t history[8192];
    int      historyCount;
} Board;

void     boardClear(Board* b);
void     boardSetFen(Board* b, const char* fen);
void     boardToFen(const Board* b, char* out);
void     boardPrint(const Board* b);
uint64_t computeHash(const Board* b);

bool squareAttacked(const Board* b, int sq, int bySide);
bool boardInCheck(const Board* b);
bool boardIsRepetition(const Board* b);
bool boardIsDraw(const Board* b);

static inline Bitboard boardOccupancy(const Board* b)
{
    return b->colours[WHITE] | b->colours[BLACK];
}
static inline Bitboard boardPieces(const Board* b, int colour, int type)
{
    return b->pieces[type] & b->colours[colour];
}
static inline int kingSquare(const Board* b, int colour)
{
    return getlsb(b->pieces[KING] & b->colours[colour]);
}

void applyMove(Board* b, Move m, Undo* u);
void revertMove(Board* b, Move m, Undo* u);
void applyNullMove(Board* b, Undo* u);
void revertNullMove(Board* b, Undo* u);

#endif
