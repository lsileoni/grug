#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "bitboard.h"
#include "attacks.h"
#include "zobrist.h"
#include "board.h"
#include "perft.h"
#include "search.h"
#include "uci.h"

static void initAll(void)
{
    initBitboards();
    initAttacks();
    initZobrist();
    searchInit();
}

static const char* benchPositions[] = {
    STARTPOS_FEN,
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
    "r1bqkbnr/pppp1ppp/2n5/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3",
    "8/8/8/2k5/2pP4/8/B7/4K3 b - d3 0 1",
};

static void runBench(int depth)
{
    SearchLimits lim;
    memset(&lim, 0, sizeof lim);
    lim.depth = depth > 0 ? depth : 8;

    for (size_t i = 0; i < sizeof benchPositions / sizeof benchPositions[0]; i++)
    {
        Board b;
        boardSetFen(&b, benchPositions[i]);
        printf("position %zu: %s\n", i + 1, benchPositions[i]);
        searchPosition(&b, &lim);
        printf("\n");
    }
}

int main(int argc, char** argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    initAll();

    if (argc > 1 && strcmp(argv[1], "bench") == 0)
    {
        runBench(argc > 2 ? atoi(argv[2]) : 8);
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "perft") == 0)
    {
        Board b;
        boardSetFen(&b, STARTPOS_FEN);
        perftDivide(&b, argc > 2 ? atoi(argv[2]) : 5);
        return 0;
    }

    uciLoop();
    return 0;
}
