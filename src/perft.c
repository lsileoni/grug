#include <stdio.h>

#include "perft.h"
#include "movegen.h"
#include "util.h"

uint64_t perft(Board* b, int depth)
{
    if (depth == 0)
        return 1;

    Move     moves[MAX_MOVES];
    int      n = generateAllMoves(b, moves);
    uint64_t nodes = 0;

    for (int i = 0; i < n; i++)
    {
        Undo u;
        applyMove(b, moves[i], &u);
        int mover = !b->turn;
        if (!squareAttacked(b, kingSquare(b, mover), b->turn))
        {
            nodes += (depth == 1) ? 1 : perft(b, depth - 1);
        }
        revertMove(b, moves[i], &u);
    }
    return nodes;
}

void perftDivide(Board* b, int depth)
{
    if (depth < 1)
    {
        printf("%llu\n", (unsigned long long)perft(b, depth));
        return;
    }

    Move      moves[MAX_MOVES];
    int       n = generateAllMoves(b, moves);
    uint64_t  total = 0;
    long long start = timeNowMs();

    for (int i = 0; i < n; i++)
    {
        Undo u;
        applyMove(b, moves[i], &u);
        int mover = !b->turn;
        if (!squareAttacked(b, kingSquare(b, mover), b->turn))
        {
            uint64_t nodes = perft(b, depth - 1);
            total += nodes;
            char str[8];
            moveToString(moves[i], str);
            printf("%s: %llu\n", str, (unsigned long long)nodes);
        }
        revertMove(b, moves[i], &u);
    }

    long long elapsed = timeNowMs() - start;
    printf("\nNodes searched: %llu\n", (unsigned long long)total);
    printf("Time: %lld ms", elapsed);
    if (elapsed > 0)
        printf("  (%.2f Mnps)", (double)total / elapsed / 1000.0);
    printf("\n");
}
