#define _CRT_SECURE_NO_WARNINGS 1 // NOLINT(bugprone-reserved-identifier) MSVC macro

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "board.h"
#include "attacks.h"
#include "zobrist.h"

static const char PieceChars[PIECE_NB + 1] = "PNBRQKpnbrqk";

static int CastleMask[SQUARE_NB];

static void initCastleMaskOnce(void)
{
    static bool done = false;
    if (done)
        return;
    done = true;
    for (int sq = 0; sq < SQUARE_NB; sq++)
        CastleMask[sq] = 0xF;
    CastleMask[E1] &= ~(CASTLE_WK | CASTLE_WQ);
    CastleMask[H1] &= ~CASTLE_WK;
    CastleMask[A1] &= ~CASTLE_WQ;
    CastleMask[E8] &= ~(CASTLE_BK | CASTLE_BQ);
    CastleMask[H8] &= ~CASTLE_BK;
    CastleMask[A8] &= ~CASTLE_BQ;
}

static inline void addPiece(Board* b, int sq, int piece)
{
    Bitboard bb = 1ULL << sq;
    b->pieces[pieceType(piece)] |= bb;
    b->colours[pieceColour(piece)] |= bb;
    b->squares[sq] = piece;
    b->hash ^= ZobristPieces[piece][sq];
}
static inline void removePiece(Board* b, int sq)
{
    int      piece = b->squares[sq];
    Bitboard bb = 1ULL << sq;
    b->pieces[pieceType(piece)] ^= bb;
    b->colours[pieceColour(piece)] ^= bb;
    b->squares[sq] = EMPTY;
    b->hash ^= ZobristPieces[piece][sq];
}
static inline void movePiece(Board* b, int from, int to)
{
    int      piece = b->squares[from];
    Bitboard mask = (1ULL << from) | (1ULL << to);
    b->pieces[pieceType(piece)] ^= mask;
    b->colours[pieceColour(piece)] ^= mask;
    b->squares[from] = EMPTY;
    b->squares[to] = piece;
    b->hash ^= ZobristPieces[piece][from] ^ ZobristPieces[piece][to];
}

void boardClear(Board* b)
{
    initCastleMaskOnce();
    memset(b, 0, sizeof(*b));
    for (int sq = 0; sq < SQUARE_NB; sq++)
        b->squares[sq] = EMPTY;
    b->epSquare = SQ_NONE;
    b->fullmoveNumber = 1;
}

uint64_t computeHash(const Board* b)
{
    uint64_t h = 0;
    for (int sq = 0; sq < SQUARE_NB; sq++)
        if (b->squares[sq] != EMPTY)
            h ^= ZobristPieces[b->squares[sq]][sq];
    if (b->epSquare != SQ_NONE)
        h ^= ZobristEnPassant[fileOf(b->epSquare)];
    h ^= ZobristCastling[b->castlingRights];
    if (b->turn == BLACK)
        h ^= ZobristSide;
    return h;
}

void boardSetFen(Board* b, const char* fen)
{
    boardClear(b);
    const char* p = fen;

    int rank = 7, file = 0;
    for (; *p && *p != ' '; p++)
    {
        char c = *p;
        if (c == '/')
        {
            rank--;
            file = 0;
        }
        else if (c >= '1' && c <= '8')
            file += c - '0';
        else
        {
            const char* q = strchr(PieceChars, c);
            if (q)
            {
                addPiece(b, makeSquare(rank, file), (int)(q - PieceChars));
                file++;
            }
        }
    }
    while (*p == ' ')
        p++;

    b->turn = (*p == 'b') ? BLACK : WHITE;
    while (*p && *p != ' ')
        p++;
    while (*p == ' ')
        p++;

    b->castlingRights = 0;
    if (*p == '-')
        p++;
    else
        for (; *p && *p != ' '; p++)
        {
            switch (*p)
            {
                case 'K':
                    b->castlingRights |= CASTLE_WK;
                    break;
                case 'Q':
                    b->castlingRights |= CASTLE_WQ;
                    break;
                case 'k':
                    b->castlingRights |= CASTLE_BK;
                    break;
                case 'q':
                    b->castlingRights |= CASTLE_BQ;
                    break;
            }
        }
    while (*p == ' ')
        p++;

    if (*p == '-')
    {
        b->epSquare = SQ_NONE;
        p++;
    }
    else
    {
        b->epSquare = makeSquare(p[1] - '1', p[0] - 'a');
        p += 2;
    }
    while (*p == ' ')
        p++;

    if (*p)
    {
        b->halfmoveClock = atoi(p);
        while (*p && *p != ' ')
            p++;
        while (*p == ' ')
            p++;
    }
    if (*p)
        b->fullmoveNumber = atoi(p);

    b->hash = computeHash(b);
    b->ply = 0;
    b->historyCount = 0;
    b->history[b->historyCount++] = b->hash;
}

void boardToFen(const Board* b, char* out)
{
    char* o = out;
    for (int rank = 7; rank >= 0; rank--)
    {
        int empty = 0;
        for (int file = 0; file < 8; file++)
        {
            int piece = b->squares[makeSquare(rank, file)];
            if (piece == EMPTY)
            {
                empty++;
                continue;
            }
            if (empty)
            {
                *o++ = '0' + empty;
                empty = 0;
            }
            *o++ = PieceChars[piece];
        }
        if (empty)
            *o++ = '0' + empty;
        if (rank)
            *o++ = '/';
    }
    *o++ = ' ';
    *o++ = b->turn == WHITE ? 'w' : 'b';
    *o++ = ' ';
    if (!b->castlingRights)
        *o++ = '-';
    else
    {
        if (b->castlingRights & CASTLE_WK)
            *o++ = 'K';
        if (b->castlingRights & CASTLE_WQ)
            *o++ = 'Q';
        if (b->castlingRights & CASTLE_BK)
            *o++ = 'k';
        if (b->castlingRights & CASTLE_BQ)
            *o++ = 'q';
    }
    *o++ = ' ';
    if (b->epSquare == SQ_NONE)
        *o++ = '-';
    else
    {
        *o++ = 'a' + fileOf(b->epSquare);
        *o++ = '1' + rankOf(b->epSquare);
    }
    sprintf(o, " %d %d", b->halfmoveClock, b->fullmoveNumber);
}

void boardPrint(const Board* b)
{
    char fen[128];
    boardToFen(b, fen);
    for (int rank = 7; rank >= 0; rank--)
    {
        printf(" %d ", rank + 1);
        for (int file = 0; file < 8; file++)
        {
            int piece = b->squares[makeSquare(rank, file)];
            printf(" %c", piece == EMPTY ? '.' : PieceChars[piece]);
        }
        printf("\n");
    }
    printf("    a b c d e f g h\n\n");
    printf("Fen: %s\n", fen);
    printf("Key: %016llX\n", (unsigned long long)b->hash);
}

bool squareAttacked(const Board* b, int sq, int bySide)
{
    Bitboard occ = boardOccupancy(b);
    Bitboard byColour = b->colours[bySide];

    if (PawnAttacks[!bySide][sq] & b->pieces[PAWN] & byColour)
        return true;
    if (KnightAttacks[sq] & b->pieces[KNIGHT] & byColour)
        return true;
    if (KingAttacks[sq] & b->pieces[KING] & byColour)
        return true;
    if (bishopAttacks(sq, occ) & (b->pieces[BISHOP] | b->pieces[QUEEN]) & byColour)
        return true;
    if (rookAttacks(sq, occ) & (b->pieces[ROOK] | b->pieces[QUEEN]) & byColour)
        return true;
    return false;
}

bool boardInCheck(const Board* b)
{
    return squareAttacked(b, kingSquare(b, b->turn), !b->turn);
}

void applyMove(Board* b, Move m, Undo* u)
{
    int from = moveFrom(m), to = moveTo(m), type = moveType(m);
    int us = b->turn, them = !us;
    int piece = b->squares[from];
    int captured = b->squares[to];

    u->captured = captured;
    u->epSquare = b->epSquare;
    u->castlingRights = b->castlingRights;
    u->halfmoveClock = b->halfmoveClock;
    u->hash = b->hash;

    if (b->epSquare != SQ_NONE)
        b->hash ^= ZobristEnPassant[fileOf(b->epSquare)];
    b->hash ^= ZobristCastling[b->castlingRights];

    if (type == EN_PASSANT)
    {
        int capSq = to + (us == WHITE ? -8 : 8);
        removePiece(b, capSq);
        movePiece(b, from, to);
    }
    else if (type == CASTLING)
    {
        movePiece(b, from, to);
        switch (to)
        {
            case G1:
                movePiece(b, H1, F1);
                break;
            case C1:
                movePiece(b, A1, D1);
                break;
            case G8:
                movePiece(b, H8, F8);
                break;
            case C8:
                movePiece(b, A8, D8);
                break;
        }
    }
    else
    {
        if (captured != EMPTY)
            removePiece(b, to);
        movePiece(b, from, to);
        if (type == PROMOTION)
        {
            removePiece(b, to);
            addPiece(b, to, makePiece(us, movePromoPiece(m)));
        }
    }

    b->halfmoveClock = (pieceType(piece) == PAWN || captured != EMPTY || type == EN_PASSANT)
                           ? 0
                           : b->halfmoveClock + 1;

    b->epSquare = SQ_NONE;
    if (pieceType(piece) == PAWN && ((from ^ to) == 16))
    {
        b->epSquare = (from + to) / 2;
        b->hash ^= ZobristEnPassant[fileOf(b->epSquare)];
    }

    b->castlingRights &= CastleMask[from] & CastleMask[to];
    b->hash ^= ZobristCastling[b->castlingRights];

    b->turn = them;
    b->hash ^= ZobristSide;
    if (us == BLACK)
        b->fullmoveNumber++;

    b->ply++;
    b->history[b->historyCount++] = b->hash;
}

void revertMove(Board* b, Move m, Undo* u)
{
    int from = moveFrom(m), to = moveTo(m), type = moveType(m);
    int us = !b->turn, them = b->turn;

    if (type == PROMOTION)
    {
        removePiece(b, to);
        addPiece(b, from, makePiece(us, PAWN));
    }
    else if (type == CASTLING)
    {
        movePiece(b, to, from);
        switch (to)
        {
            case G1:
                movePiece(b, F1, H1);
                break;
            case C1:
                movePiece(b, D1, A1);
                break;
            case G8:
                movePiece(b, F8, H8);
                break;
            case C8:
                movePiece(b, D8, A8);
                break;
        }
    }
    else
    {
        movePiece(b, to, from);
    }

    if (type == EN_PASSANT)
    {
        int capSq = to + (us == WHITE ? -8 : 8);
        addPiece(b, capSq, makePiece(them, PAWN));
    }
    else if (type != CASTLING && u->captured != EMPTY)
    {
        addPiece(b, to, u->captured);
    }

    b->turn = us;
    b->castlingRights = u->castlingRights;
    b->epSquare = u->epSquare;
    b->halfmoveClock = u->halfmoveClock;
    b->hash = u->hash;
    b->ply--;
    b->historyCount--;
    if (us == BLACK)
        b->fullmoveNumber--;
}

void applyNullMove(Board* b, Undo* u)
{
    u->captured = EMPTY;
    u->epSquare = b->epSquare;
    u->castlingRights = b->castlingRights;
    u->halfmoveClock = b->halfmoveClock;
    u->hash = b->hash;

    if (b->epSquare != SQ_NONE)
        b->hash ^= ZobristEnPassant[fileOf(b->epSquare)];
    b->epSquare = SQ_NONE;
    b->turn ^= 1;
    b->hash ^= ZobristSide;
    b->halfmoveClock++;
    b->ply++;
    b->history[b->historyCount++] = b->hash;
}

void revertNullMove(Board* b, Undo* u)
{
    b->turn ^= 1;
    b->epSquare = u->epSquare;
    b->castlingRights = u->castlingRights;
    b->halfmoveClock = u->halfmoveClock;
    b->hash = u->hash;
    b->ply--;
    b->historyCount--;
}

bool boardIsRepetition(const Board* b)
{
    int count = b->halfmoveClock;
    for (int i = b->historyCount - 3; i >= 0 && count >= 2; i -= 2, count -= 2)
        if (b->history[i] == b->hash)
            return true;
    return false;
}

bool boardIsDraw(const Board* b)
{
    if (b->halfmoveClock >= 100)
        return true;
    if (boardIsRepetition(b))
        return true;

    if (b->pieces[PAWN] | b->pieces[ROOK] | b->pieces[QUEEN])
        return false;
    int minors = popcount(b->pieces[KNIGHT] | b->pieces[BISHOP]);
    return minors <= 1;
}
