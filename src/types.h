#ifndef GRUG_TYPES_H
#define GRUG_TYPES_H

#include <stdint.h>
#include <stdbool.h>

typedef uint64_t Bitboard;
typedef uint16_t Move;

enum
{
    MAX_PLY = 128,
    MAX_MOVES = 256
};

enum
{
    WHITE,
    BLACK,
    COLOUR_NB
};

enum
{
    PAWN,
    KNIGHT,
    BISHOP,
    ROOK,
    QUEEN,
    KING,
    PIECE_TYPE_NB
};

enum
{
    W_PAWN,
    W_KNIGHT,
    W_BISHOP,
    W_ROOK,
    W_QUEEN,
    W_KING,
    B_PAWN,
    B_KNIGHT,
    B_BISHOP,
    B_ROOK,
    B_QUEEN,
    B_KING,
    PIECE_NB,
    EMPTY = PIECE_NB
};

enum
{
    A1,
    B1,
    C1,
    D1,
    E1,
    F1,
    G1,
    H1,
    A2,
    B2,
    C2,
    D2,
    E2,
    F2,
    G2,
    H2,
    A3,
    B3,
    C3,
    D3,
    E3,
    F3,
    G3,
    H3,
    A4,
    B4,
    C4,
    D4,
    E4,
    F4,
    G4,
    H4,
    A5,
    B5,
    C5,
    D5,
    E5,
    F5,
    G5,
    H5,
    A6,
    B6,
    C6,
    D6,
    E6,
    F6,
    G6,
    H6,
    A7,
    B7,
    C7,
    D7,
    E7,
    F7,
    G7,
    H7,
    A8,
    B8,
    C8,
    D8,
    E8,
    F8,
    G8,
    H8,
    SQUARE_NB,
    SQ_NONE = SQUARE_NB
};

enum
{
    FILE_A,
    FILE_B,
    FILE_C,
    FILE_D,
    FILE_E,
    FILE_F,
    FILE_G,
    FILE_H,
    FILE_NB
};
enum
{
    RANK_1,
    RANK_2,
    RANK_3,
    RANK_4,
    RANK_5,
    RANK_6,
    RANK_7,
    RANK_8,
    RANK_NB
};

enum
{
    CASTLE_WK = 1,
    CASTLE_WQ = 2,
    CASTLE_BK = 4,
    CASTLE_BQ = 8,
    CASTLE_NB = 16
};

enum
{
    VALUE_DRAW = 0,
    VALUE_MATE = 32000,
    VALUE_INF = 32001,
    VALUE_NONE = 32002,
    VALUE_MATE_IN_MAX = VALUE_MATE - MAX_PLY,
};

static inline int fileOf(int sq)
{
    return sq & 7;
}
static inline int rankOf(int sq)
{
    return sq >> 3;
}
static inline int makeSquare(int r, int f)
{
    return (r << 3) | f;
}

static inline int pieceType(int piece)
{
    return piece % PIECE_TYPE_NB;
}
static inline int pieceColour(int piece)
{
    return piece / PIECE_TYPE_NB;
}
static inline int makePiece(int c, int t)
{
    return c * PIECE_TYPE_NB + t;
}

static inline int relativeSquare(int colour, int sq)
{
    return sq ^ (colour * 56);
}
static inline int relativeRank(int colour, int sq)
{
    return rankOf(sq) ^ (colour * 7);
}

static inline bool isOk(int sq)
{
    return sq >= A1 && sq <= H8;
}

#endif
