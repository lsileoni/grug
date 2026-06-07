// Scratch SEE sanity check for algohelpers. Not part of the build; compile with:
//   gcc -std=c11 -Isrc src/bitboard.c src/attacks.c src/zobrist.c src/board.c \
//       src/movegen.c src/algohelpers.c tools/see_sanity.c -o /tmp/see_sanity
#include <stdio.h>

#include "bitboard.h"
#include "attacks.h"
#include "zobrist.h"
#include "board.h"
#include "move.h"
#include "algohelpers.h"

int main(void)
{
    initBitboards();
    initAttacks();
    initZobrist();

    struct
    {
        const char* fen;
        int         from;
        int         to;
        int         expect;
        const char* name;
    } tests[] = {
        {"4k3/8/8/3p4/8/8/3R4/4K3 w - - 0 1", D2, D5, 100, "Rxd5 free pawn"},
        {"4k3/8/2p5/3p4/8/4N3/8/4K3 w - - 0 1", E3, D5, -220, "Nxd5 (pawn-defended)"},
        {"4k3/8/2p5/3p4/4P3/8/8/4K3 w - - 0 1", E4, D5, 0, "exd5 pawn-for-pawn"},
        {"4k3/8/8/8/3p4/R7/8/4K3 w - - 0 1", A3, E3, -500, "Re3 hangs rook to pawn"},
    };

    int fails = 0;
    for (int i = 0; i < (int)(sizeof tests / sizeof tests[0]); i++)
    {
        Board b;
        boardSetFen(&b, tests[i].fen);
        Move m = makeMove(tests[i].from, tests[i].to);
        int  s = see(&b, m);
        bool ok = s == tests[i].expect;
        fails += !ok;
        printf("%-24s see=%-5d expect=%-5d %s\n", tests[i].name, s, tests[i].expect,
               ok ? "OK" : "FAIL");
    }
    printf("%s\n", fails ? "SOME TESTS FAILED" : "ALL OK");
    return fails;
}
