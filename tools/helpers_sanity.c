// Scratch sanity check for the position-state and pawn-structure helpers.
// Not part of the build; compile with:
//   gcc -std=c11 -Isrc src/bitboard.c src/attacks.c src/zobrist.c src/board.c
//       src/movegen.c src/algohelpers.c tools/helpers_sanity.c -o /tmp/helpers_sanity
#include <stdio.h>

#include "bitboard.h"
#include "attacks.h"
#include "zobrist.h"
#include "board.h"
#include "algohelpers.h"

static int fails = 0;

static void check(const char* name, bool got, bool expect)
{
    if (got != expect)
        fails++;
    printf("%-40s got=%d expect=%d %s\n", name, got, expect, got == expect ? "OK" : "FAIL");
}

int main(void)
{
    initBitboards();
    initAttacks();
    initZobrist();

    Board b;

    // Fool's mate: white to move, checkmated.
    boardSetFen(&b, "rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3");
    check("fools mate: isCheckmate", isCheckmate(&b), true);
    check("fools mate: isStalemate", isStalemate(&b), false);

    // Classic stalemate: black to move, no moves, not in check.
    boardSetFen(&b, "7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");
    check("stalemate: isStalemate", isStalemate(&b), true);
    check("stalemate: isCheckmate", isCheckmate(&b), false);

    // Startpos: neither.
    boardSetFen(&b, STARTPOS_FEN);
    check("startpos: isCheckmate", isCheckmate(&b), false);
    check("startpos: isStalemate", isStalemate(&b), false);

    // Pawns: white c2+c3 (doubled, isolated), h2 (isolated, passed);
    // black c5 (isolated, blocked by white c-pawns so not passed).
    boardSetFen(&b, "4k3/8/8/2p5/8/2P5/2P4P/4K3 w - - 0 1");
    check("c3: doubled", isDoubledPawn(&b, C3), true);
    check("c2: doubled", isDoubledPawn(&b, C2), true);
    check("h2: doubled", isDoubledPawn(&b, H2), false);
    check("c3: isolated", isIsolatedPawn(&b, C3), true);
    check("h2: isolated", isIsolatedPawn(&b, H2), true);
    check("c3: passed (blocked by c5)", isPassedPawn(&b, C3), false);
    check("h2: passed", isPassedPawn(&b, H2), true);
    check("c5 (black): passed (blocked)", isPassedPawn(&b, C5), false);
    check("c5 (black): doubled", isDoubledPawn(&b, C5), false);
    check("c5 (black): isolated", isIsolatedPawn(&b, C5), true);
    check(
        "empty square: all false",
        isPassedPawn(&b, E4) || isIsolatedPawn(&b, E4) || isDoubledPawn(&b, E4), false
    );
    check("king square: all false", isPassedPawn(&b, E1) || isDoubledPawn(&b, E1), false);

    // Adjacent enemy pawn ahead blocks passage: white b4 vs black c5.
    boardSetFen(&b, "4k3/8/8/2p5/1P6/8/8/4K3 w - - 0 1");
    check("b4 vs c5: not passed", isPassedPawn(&b, B4), false);
    // ...but a pawn already past it is passed: white b5 vs black c5.
    boardSetFen(&b, "4k3/8/8/1Pp5/8/8/8/4K3 w - - 0 1");
    check("b5 beside c5: passed", isPassedPawn(&b, B5), true);

    printf("%s\n", fails ? "SOME TESTS FAILED" : "ALL OK");
    return fails;
}
