#ifndef GRUG_BITBOARD_H
#define GRUG_BITBOARD_H

#include "types.h"

#define FILE_A_BB 0x0101010101010101ULL
#define FILE_B_BB (FILE_A_BB << 1)
#define FILE_H_BB 0x8080808080808080ULL
#define RANK_1_BB 0x00000000000000FFULL
#define RANK_8_BB 0xFF00000000000000ULL

extern Bitboard SquareBB[SQUARE_NB];
extern Bitboard FileBB[FILE_NB];
extern Bitboard RankBB[RANK_NB];
extern int      SquareDistance[SQUARE_NB][SQUARE_NB];

void initBitboards(void);

static inline Bitboard squareBB(int sq)
{
    return 1ULL << sq;
}

#if defined(__GNUC__)
static inline int popcount(Bitboard b)
{
    return __builtin_popcountll(b);
}
static inline int getlsb(Bitboard b)
{
    return __builtin_ctzll(b);
}
static inline int getmsb(Bitboard b)
{
    return 63 ^ __builtin_clzll(b);
}
#elif defined(_MSC_VER)
#include <intrin.h>
static inline int popcount(Bitboard b)
{
    return (int)__popcnt64(b);
}
static inline int getlsb(Bitboard b)
{
    unsigned long i;
    _BitScanForward64(&i, b);
    return (int)i;
}
static inline int getmsb(Bitboard b)
{
    unsigned long i;
    _BitScanReverse64(&i, b);
    return (int)i;
}
#else
static inline int popcount(Bitboard b)
{
    int c = 0;
    while (b)
    {
        b &= b - 1;
        c++;
    }
    return c;
}
static inline int getlsb(Bitboard b)
{
    int i = 0;
    while (!((b >> i) & 1))
        i++;
    return i;
}
static inline int getmsb(Bitboard b)
{
    int i = 63;
    while (!((b >> i) & 1))
        i--;
    return i;
}
#endif

static inline int poplsb(Bitboard* b)
{
    int sq = getlsb(*b);
    *b &= *b - 1;
    return sq;
}

static inline bool several(Bitboard b)
{
    return b & (b - 1);
}

static inline Bitboard shiftNorth(Bitboard b)
{
    return b << 8;
}
static inline Bitboard shiftSouth(Bitboard b)
{
    return b >> 8;
}
static inline Bitboard shiftEast(Bitboard b)
{
    return (b & ~FILE_H_BB) << 1;
}
static inline Bitboard shiftWest(Bitboard b)
{
    return (b & ~FILE_A_BB) >> 1;
}

#endif
