#include "movegen.h"
#include "attacks.h"

static inline int addPromotions(Move* moves, int idx, int from, int to)
{
    moves[idx++] = makePromotion(from, to, QUEEN);
    moves[idx++] = makePromotion(from, to, ROOK);
    moves[idx++] = makePromotion(from, to, BISHOP);
    moves[idx++] = makePromotion(from, to, KNIGHT);
    return idx;
}

static inline int serialize(Move* moves, int idx, int from, Bitboard targets)
{
    while (targets)
        moves[idx++] = makeMove(from, poplsb(&targets));
    return idx;
}

static int generate(const Board* b, Move* moves, bool quiets)
{
    int      idx = 0;
    int      us = b->turn, them = !us;
    int      up = (us == WHITE) ? 8 : -8;
    Bitboard occ = boardOccupancy(b);
    Bitboard usBB = b->colours[us];
    Bitboard themBB = b->colours[them];
    Bitboard themKing = b->pieces[KING] & themBB;
    Bitboard themNoKing = themBB & ~themKing;

    Bitboard pawns = b->pieces[PAWN] & usBB;
    while (pawns)
    {
        int  from = poplsb(&pawns);
        bool promoting = relativeRank(us, from) == RANK_7;

        int to1 = from + up;
        if (!(occ & (1ULL << to1)))
        {
            if (promoting)
            {
                idx = addPromotions(moves, idx, from, to1);
            }
            else if (quiets)
            {
                moves[idx++] = makeMove(from, to1);
                int to2 = to1 + up;
                if (relativeRank(us, from) == RANK_2 && !(occ & (1ULL << to2)))
                    moves[idx++] = makeMove(from, to2);
            }
        }

        Bitboard caps = PawnAttacks[us][from] & themNoKing;
        while (caps)
        {
            int to = poplsb(&caps);
            if (promoting)
                idx = addPromotions(moves, idx, from, to);
            else
                moves[idx++] = makeMove(from, to);
        }

        if (b->epSquare != SQ_NONE && (PawnAttacks[us][from] & (1ULL << b->epSquare)))
            moves[idx++] = makeMoveFlag(from, b->epSquare, EN_PASSANT);
    }

    Bitboard targets = quiets ? ~(usBB | themKing) : themNoKing;

    Bitboard it = b->pieces[KNIGHT] & usBB;
    while (it)
    {
        int s = poplsb(&it);
        idx = serialize(moves, idx, s, KnightAttacks[s] & targets);
    }

    it = b->pieces[BISHOP] & usBB;
    while (it)
    {
        int s = poplsb(&it);
        idx = serialize(moves, idx, s, bishopAttacks(s, occ) & targets);
    }

    it = b->pieces[ROOK] & usBB;
    while (it)
    {
        int s = poplsb(&it);
        idx = serialize(moves, idx, s, rookAttacks(s, occ) & targets);
    }

    it = b->pieces[QUEEN] & usBB;
    while (it)
    {
        int s = poplsb(&it);
        idx = serialize(moves, idx, s, queenAttacks(s, occ) & targets);
    }

    int ksq = kingSquare(b, us);
    idx = serialize(moves, idx, ksq, KingAttacks[ksq] & targets);

    if (quiets)
    {
        if (us == WHITE)
        {
            if ((b->castlingRights & CASTLE_WK) && !(occ & ((1ULL << F1) | (1ULL << G1))) &&
                !squareAttacked(b, E1, BLACK) && !squareAttacked(b, F1, BLACK) &&
                !squareAttacked(b, G1, BLACK))
                moves[idx++] = makeMoveFlag(E1, G1, CASTLING);
            if ((b->castlingRights & CASTLE_WQ) &&
                !(occ & ((1ULL << B1) | (1ULL << C1) | (1ULL << D1))) &&
                !squareAttacked(b, E1, BLACK) && !squareAttacked(b, D1, BLACK) &&
                !squareAttacked(b, C1, BLACK))
                moves[idx++] = makeMoveFlag(E1, C1, CASTLING);
        }
        else
        {
            if ((b->castlingRights & CASTLE_BK) && !(occ & ((1ULL << F8) | (1ULL << G8))) &&
                !squareAttacked(b, E8, WHITE) && !squareAttacked(b, F8, WHITE) &&
                !squareAttacked(b, G8, WHITE))
                moves[idx++] = makeMoveFlag(E8, G8, CASTLING);
            if ((b->castlingRights & CASTLE_BQ) &&
                !(occ & ((1ULL << B8) | (1ULL << C8) | (1ULL << D8))) &&
                !squareAttacked(b, E8, WHITE) && !squareAttacked(b, D8, WHITE) &&
                !squareAttacked(b, C8, WHITE))
                moves[idx++] = makeMoveFlag(E8, C8, CASTLING);
        }
    }

    return idx;
}

int generateAllMoves(const Board* b, Move* moves)
{
    return generate(b, moves, true);
}
int generateNoisyMoves(const Board* b, Move* moves)
{
    return generate(b, moves, false);
}

int generateQuietMoves(const Board* b, Move* moves)
{
    Move all[MAX_MOVES], noisy[MAX_MOVES];
    int  na = generate(b, all, true);
    int  nn = generate(b, noisy, false);
    int  idx = 0;
    for (int i = 0; i < na; i++)
    {
        bool isNoisy = false;
        for (int j = 0; j < nn; j++)
            if (all[i] == noisy[j])
            {
                isNoisy = true;
                break;
            }
        if (!isNoisy)
            moves[idx++] = all[i];
    }
    return idx;
}

bool moveIsLegal(Board* b, Move m)
{
    Undo u;
    applyMove(b, m, &u);
    int  mover = !b->turn;
    bool legal = !squareAttacked(b, kingSquare(b, mover), b->turn);
    revertMove(b, m, &u);
    return legal;
}
