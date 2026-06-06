#ifndef GRUG_MOVE_H
#define GRUG_MOVE_H

#include "types.h"

enum
{
    NORMAL = 0 << 12,
    PROMOTION = 1 << 12,
    EN_PASSANT = 2 << 12,
    CASTLING = 3 << 12,
    MOVETYPE_MASK = 3 << 12
};

#define NO_MOVE   ((Move)0)
#define NULL_MOVE ((Move)(H8 | (H8 << 6)))

static inline int moveFrom(Move m)
{
    return m & 0x3F;
}
static inline int moveTo(Move m)
{
    return (m >> 6) & 0x3F;
}
static inline int moveType(Move m)
{
    return m & MOVETYPE_MASK;
}
static inline int movePromoPiece(Move m)
{
    return KNIGHT + ((m >> 14) & 3);
}

static inline Move makeMove(int from, int to)
{
    return (Move)(from | (to << 6));
}
static inline Move makeMoveFlag(int from, int to, int t)
{
    return (Move)(from | (to << 6) | t);
}
static inline Move makePromotion(int from, int to, int pt)
{
    return (Move)(from | (to << 6) | PROMOTION | ((pt - KNIGHT) << 14));
}

static inline void moveToString(Move m, char* buf)
{
    int from = moveFrom(m), to = moveTo(m);
    buf[0] = 'a' + fileOf(from);
    buf[1] = '1' + rankOf(from);
    buf[2] = 'a' + fileOf(to);
    buf[3] = '1' + rankOf(to);
    if (moveType(m) == PROMOTION)
    {
        buf[4] = " nbrqk"[movePromoPiece(m)];
        buf[5] = '\0';
    }
    else
    {
        buf[4] = '\0';
    }
}

#endif
