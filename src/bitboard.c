#include "bitboard.h"

Bitboard SquareBB[SQUARE_NB];
Bitboard FileBB[FILE_NB];
Bitboard RankBB[RANK_NB];
int      SquareDistance[SQUARE_NB][SQUARE_NB];

static int absInt(int x)
{
    return x < 0 ? -x : x;
}

void initBitboards(void)
{
    for (int sq = 0; sq < SQUARE_NB; sq++)
        SquareBB[sq] = 1ULL << sq;

    for (int f = 0; f < FILE_NB; f++)
        FileBB[f] = FILE_A_BB << f;

    for (int r = 0; r < RANK_NB; r++)
        RankBB[r] = RANK_1_BB << (8 * r);

    for (int a = 0; a < SQUARE_NB; a++)
        for (int b = 0; b < SQUARE_NB; b++)
        {
            int rd = absInt(rankOf(a) - rankOf(b));
            int fd = absInt(fileOf(a) - fileOf(b));
            SquareDistance[a][b] = rd > fd ? rd : fd;
        }
}
