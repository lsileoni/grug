#include "attacks.h"
#include "bitboard.h"

Bitboard PawnAttacks[COLOUR_NB][SQUARE_NB];
Bitboard KnightAttacks[SQUARE_NB];
Bitboard KingAttacks[SQUARE_NB];

Magic BishopMagics[SQUARE_NB];
Magic RookMagics[SQUARE_NB];

static Bitboard RookTable[102400];
static Bitboard BishopTable[5248];

static const int BishopDeltas[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
static const int RookDeltas[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

static Bitboard slidingAttacks(int sq, Bitboard occ, const int deltas[4][2])
{
    Bitboard attacks = 0ULL;
    for (int i = 0; i < 4; i++)
    {
        int r = rankOf(sq) + deltas[i][0];
        int f = fileOf(sq) + deltas[i][1];
        while (r >= 0 && r < 8 && f >= 0 && f < 8)
        {
            int s = makeSquare(r, f);
            attacks |= 1ULL << s;
            if (occ & (1ULL << s))
                break;
            r += deltas[i][0];
            f += deltas[i][1];
        }
    }
    return attacks;
}

static uint64_t magicSeed = 0x246C5BE0F1A2D3C4ULL;
static uint64_t rng64(void)
{
    magicSeed ^= magicSeed >> 12;
    magicSeed ^= magicSeed << 25;
    magicSeed ^= magicSeed >> 27;
    return magicSeed * 0x2545F4914F6CDD1DULL;
}
static uint64_t sparseRand(void)
{
    // NOLINTNEXTLINE(misc-redundant-expression) distinct rng64() calls, not equal operands
    return rng64() & rng64() & rng64();
}

static void initMagicsFor(const int deltas[4][2], Magic* magics, Bitboard* table)
{
    Bitboard  occupancies[4096];
    Bitboard  references[4096];
    Bitboard* slice = table;

    for (int sq = 0; sq < SQUARE_NB; sq++)
    {
        Bitboard rank = 0xFFULL << (8 * rankOf(sq));
        Bitboard file = FILE_A_BB << fileOf(sq);
        Bitboard edges = ((RANK_1_BB | RANK_8_BB) & ~rank) | ((FILE_A_BB | FILE_H_BB) & ~file);

        Magic* m = &magics[sq];
        m->mask = slidingAttacks(sq, 0ULL, deltas) & ~edges;
        m->shift = 64 - popcount(m->mask);
        m->table = slice;

        int      size = 0;
        Bitboard b = 0ULL;
        do
        {
            occupancies[size] = b;
            references[size] = slidingAttacks(sq, b, deltas);
            size++;
            b = (b - m->mask) & m->mask;
        } while (b);

        slice += size;

        for (;;)
        {
            Bitboard magic;
            do
            {
                magic = sparseRand();
            } while (popcount((magic * m->mask) >> 56) < 6);

            for (int i = 0; i < size; i++)
                m->table[i] = 0ULL;

            bool ok = true;
            for (int i = 0; i < size; i++)
            {
                unsigned idx = (unsigned)((occupancies[i] * magic) >> m->shift);
                if (m->table[idx] == 0ULL)
                    m->table[idx] = references[i];
                else if (m->table[idx] != references[i])
                {
                    ok = false;
                    break;
                }
            }
            if (ok)
            {
                m->magic = magic;
                break;
            }
        }
    }
}

static Bitboard knightSpan(Bitboard b)
{
    Bitboard l1 = (b >> 1) & 0x7f7f7f7f7f7f7f7fULL;
    Bitboard l2 = (b >> 2) & 0x3f3f3f3f3f3f3f3fULL;
    Bitboard r1 = (b << 1) & 0xfefefefefefefefeULL;
    Bitboard r2 = (b << 2) & 0xfcfcfcfcfcfcfcfcULL;
    Bitboard h1 = l1 | r1;
    Bitboard h2 = l2 | r2;
    return (h1 << 16) | (h1 >> 16) | (h2 << 8) | (h2 >> 8);
}

static Bitboard kingSpan(Bitboard b)
{
    Bitboard lateral = ((b & ~FILE_H_BB) << 1) | ((b & ~FILE_A_BB) >> 1);
    Bitboard row = b | lateral;
    return lateral | (row << 8) | (row >> 8);
}

void initAttacks(void)
{
    for (int sq = 0; sq < SQUARE_NB; sq++)
    {
        Bitboard b = 1ULL << sq;
        KnightAttacks[sq] = knightSpan(b);
        KingAttacks[sq] = kingSpan(b);
        PawnAttacks[WHITE][sq] = ((b & ~FILE_A_BB) << 7) | ((b & ~FILE_H_BB) << 9);
        PawnAttacks[BLACK][sq] = ((b & ~FILE_H_BB) >> 7) | ((b & ~FILE_A_BB) >> 9);
    }
    initMagicsFor(BishopDeltas, BishopMagics, BishopTable);
    initMagicsFor(RookDeltas, RookMagics, RookTable);
}
