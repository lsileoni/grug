#ifndef GRUG_ATTACKS_H
#define GRUG_ATTACKS_H

#include "types.h"

extern Bitboard PawnAttacks[COLOUR_NB][SQUARE_NB];
extern Bitboard KnightAttacks[SQUARE_NB];
extern Bitboard KingAttacks[SQUARE_NB];

typedef struct
{
    Bitboard* table;
    Bitboard  mask;
    Bitboard  magic;
    int       shift;
} Magic;

extern Magic BishopMagics[SQUARE_NB];
extern Magic RookMagics[SQUARE_NB];

void initAttacks(void);

static inline Bitboard bishopAttacks(int sq, Bitboard occ)
{
    const Magic* m = &BishopMagics[sq];
    return m->table[((occ & m->mask) * m->magic) >> m->shift];
}
static inline Bitboard rookAttacks(int sq, Bitboard occ)
{
    const Magic* m = &RookMagics[sq];
    return m->table[((occ & m->mask) * m->magic) >> m->shift];
}
static inline Bitboard queenAttacks(int sq, Bitboard occ)
{
    return bishopAttacks(sq, occ) | rookAttacks(sq, occ);
}

static inline Bitboard pieceAttacks(int type, int sq, Bitboard occ)
{
    switch (type)
    {
        case KNIGHT:
            return KnightAttacks[sq];
        case BISHOP:
            return bishopAttacks(sq, occ);
        case ROOK:
            return rookAttacks(sq, occ);
        case QUEEN:
            return queenAttacks(sq, occ);
        case KING:
            return KingAttacks[sq];
        default:
            return 0ULL;
    }
}

#endif
