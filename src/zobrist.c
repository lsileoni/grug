#include "zobrist.h"

uint64_t ZobristPieces[PIECE_NB][SQUARE_NB];
uint64_t ZobristCastling[CASTLE_NB];
uint64_t ZobristEnPassant[FILE_NB];
uint64_t ZobristSide;

static uint64_t prngState = 0x9E3779B97F4A7C15ULL;

static uint64_t prng(void)
{
    uint64_t z = (prngState += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

void initZobrist(void)
{
    for (int p = 0; p < PIECE_NB; p++)
        for (int sq = 0; sq < SQUARE_NB; sq++)
            ZobristPieces[p][sq] = prng();

    for (int i = 0; i < CASTLE_NB; i++)
        ZobristCastling[i] = prng();

    for (int f = 0; f < FILE_NB; f++)
        ZobristEnPassant[f] = prng();

    ZobristSide = prng();
}
