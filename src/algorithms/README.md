# Developing Algorithms in grug

Algorithms live in this directory and are selected through the UCI `Algorithm`
option. Each algorithm is a small module that exports one `const Algorithm`
value. The core search wrapper calls that struct, validates the returned move,
prints UCI output, and falls back to the first legal move if the algorithm gives
an illegal move.

Use the existing files as examples:

- `basic_search.c`: real depth-limited search with evaluation, move ordering,
  time/node limits, and UCI `info` output.
- `first_legal.c`: minimal legal-move picker.
- `square_maximization.c`: one-ply heuristic that picks the move maximizing the
  mover's mobility, written with the `algohelpers` vocabulary (`legalMoves`,
  `afterMove`, `mobility`) while keeping its loop visible.
- `threat_aware.c`: one-ply heuristic that grabs winning captures (by `see`), gives
  checks, and avoids hanging its own pieces. The worked example for the
  `algohelpers` vocabulary below.
- `first_generated.c`: intentionally pseudo-legal example.
- `e2e4.c`: fixed-move example.
- `no_move.c`: deliberately returns no move so the wrapper can fall back.

## Easier path: the algohelpers vocabulary

Most of the fiddly parts of writing an algorithm  filtering legal moves, the
apply/revert dance, and turning bitboards into answers  are available as a small
"chess vocabulary" in `src/algohelpers.h`. Include it and ask your question
directly; your algorithm keeps its own loop and decision logic.

```c
#include "../algohelpers.h"
```

The helpers are plain functions, grouped by the question they answer:

**Squares & pieces**

- `pieceOn(b, sq)`, `typeOn(b, sq)`, `colourOn(b, sq)`, `isEmpty(b, sq)`
- `pieceValue(type)`  centipawns (PAWN=100 … QUEEN=900, KING=0)

**Vision & attackers**

- `sees(b, sq)`  the squares the piece on `sq` attacks (knight jumps, slider rays
  through occupancy, pawn diagonals). "What does the knight on d4 see?"
- `attackersTo(b, sq)` / `attackersOf(b, sq, colour)`  pieces attacking a square
- `isAttacked(b, sq, byColour)`, `isDefended(b, sq)`

**Move consequences**

- `moveIsCapture(b, m)`, `moveCaptured(b, m)`
- `moveGivesCheck(b, m)`  does playing `m` check the opponent?
- `captureGain(b, m)`  naive victim − attacker value
- `see(b, m)`  full static exchange evaluation: the real material outcome of a
  capture (or of a quiet move onto a contested square). Positive wins material.
- `afterMove(b, m, fn, ctx)`  run any query on the position that *results* from a
  move, without writing the apply/revert yourself.

**Threats & safety**

- `isHanging(b, sq)`, `hangingPieces(b, colour)`

**Material & mobility**

- `materialValue(b, colour)`, `materialBalance(b, colour)`, `materialCount(b, colour, type)`
- `sideAttacks(b, colour)`, `mobility(b, colour)`

**Convenience**

- `searchResultInit(result)`  initialize every `SearchResult` field
- `legalMoves(b, out)`  generate only the legal moves (no pseudo-legal filtering)
- `applyIfLegal(b, m, &u)`  apply a move only if it leaves your king safe
- `popNextSquare(&bb)`  walk a bitboard without a hand-written `poplsb` loop
- `sideToMove(b)`, `moverSide(b)` (`!turn`, handy inside an `afterMove` query)

Putting a few of these together, here is the heart of a threat-aware one-ply
heuristic  the loop *is* the algorithm, and each line is a chess question:

```c
Move moves[MAX_MOVES];
int  n = legalMoves(b, moves);

Move bestMove  = NO_MOVE;
int  bestScore = 0;

for (int i = 0; i < n; i++)
{
    Move m     = moves[i];
    int  score = 0;

    if (moveIsCapture(b, m) && see(b, m) > 0)        // a capture that wins material
        score += see(b, m);
    if (moveGivesCheck(b, m))                         // a forcing move
        score += 40;
    score -= afterMove(b, m, moverHangingValue, NULL); // don't leave pieces loose

    if (bestMove == NO_MOVE || score > bestScore)
    {
        bestScore = score;
        bestMove  = m;
    }
}
```

Here `moverHangingValue` is a one-line `BoardQueryFn` that sums the value of the
mover's hanging pieces (`hangingPieces` + `pieceValue`). See `threat_aware.c` for
the complete, compilable version.

### A guiding idea: vocabulary, not framework

These helpers deliberately give you *questions*, not control flow. Your generate /
loop / compare / decide stays in your algorithm file, where its meaning lives.

There is exactly one optional convenience that owns the loop,
`chooseHighestScoring(b, result, eval, ctx)`, for the narrow case where your
algorithm genuinely *is* "score every legal move and keep the best". Reach for it
only when that shape is the whole story; otherwise write the loop yourself with the
queries above.

## File Layout

For a new algorithm named `my_search`, add:

- `src/algorithms/my_search.h`
- `src/algorithms/my_search.c`

The header should only expose the exported algorithm object:

```c
#ifndef GRUG_ALGORITHMS_MY_SEARCH_H
#define GRUG_ALGORITHMS_MY_SEARCH_H

#include "../algorithm.h"

extern const Algorithm MySearchAlgorithm;

#endif
```

The source file should keep implementation details `static` and export exactly
one `const Algorithm`:

```c
#include "my_search.h"

#include <stddef.h>

#include "../movegen.h"

static bool mySearchChooseMove(Board *b, const SearchLimits *limits, SearchResult *result) {
    (void)limits;

    result->bestMove = NO_MOVE;
    result->nodes = 0;
    result->hasScore = false;
    result->score = 0;

    Move moves[MAX_MOVES];
    int n = generateAllMoves(b, moves);
    for (int i = 0; i < n; i++) {
        result->nodes++;
        if (moveIsLegal(b, moves[i])) {
            result->bestMove = moves[i];
            return true;
        }
    }

    return true;
}

const Algorithm MySearchAlgorithm = {
    "my_search",
    "short description shown by developers and logs",
    NULL,
    NULL,
    NULL,
    mySearchChooseMove,
};
```

Then register it in `src/algorithm.c`:

```c
#include "algorithms/my_search.h"

static const Algorithm *Algorithms[] = {
    &BasicSearchAlgorithm,
    &MySearchAlgorithm,
    ...
};
```

The build already includes `src/algorithms/*.c`, so no build-system edit is
needed for normal new algorithm files.

## The Algorithm Struct

`Algorithm` is declared in `src/algorithm.h`:

```c
typedef struct {
    const char *name;
    const char *description;

    void (*init)(void);
    void (*newGame)(void);
    bool (*evaluate)(const Board *b, int *score);
    bool (*chooseMove)(Board *b, const SearchLimits *limits, SearchResult *result);
} Algorithm;
```

Fill the fields as follows:

- `name`: stable UCI name. Use lowercase snake case, for example
  `"basic_search"`. This is what users pass with
  `setoption name Algorithm value basic_search`.
- `description`: short human-readable description. Keep it one line.
- `init`: optional startup hook. Use it for lookup tables, persistent state, or
  allocation. Set `NULL` if unused.
- `newGame`: optional game-reset hook called on `ucinewgame`. Use it to clear
  game-local state such as history heuristics. Set `NULL` if unused.
- `evaluate`: optional static evaluation hook used by the `eval` command. It
  must write `*score` in centipawns from the side-to-move perspective and return
  `true` when available. Set `NULL` if the algorithm has no public eval.
- `chooseMove`: required for playing. It receives the current board, search
  limits, and a result struct to fill. Return `true` if the algorithm ran,
  even if there is no legal move. Return `false` only for an internal failure.

Keep helper functions `static` unless they are intentionally shared. This keeps
algorithm modules independent and prevents accidental symbol collisions.

## Filling SearchResult

`SearchResult` is declared in `src/search.h`:

```c
typedef struct {
    Move     bestMove;
    uint64_t nodes;
    bool     hasScore;
    int      score;
} SearchResult;
```

Always initialize every field at the top of `chooseMove`:

```c
result->bestMove = NO_MOVE;
result->nodes = 0;
result->hasScore = false;
result->score = 0;
```

Then update the fields as your search progresses:

- `bestMove`: the best legal move found, or `NO_MOVE` if none exists.
- `nodes`: number of nodes or generated candidate moves searched. This is
  printed as UCI `info nodes`.
- `hasScore`: set `true` only when `score` is meaningful.
- `score`: centipawns from the side-to-move perspective. Positive means good for
  the player to move. Use mate scores near `VALUE_MATE`/`-VALUE_MATE` when
  reporting forced mate.

The wrapper in `searchPosition()` validates `bestMove`. If it is illegal, grug
prints an `info string` and falls back to the first legal move. Do not rely on
that for real algorithms; it is a last-resort safety net.

If your algorithm has no legal move, return `true` with `bestMove = NO_MOVE`.
The wrapper will print `bestmove 0000`.

## Reading SearchLimits

`SearchLimits` comes from the UCI `go` command:

```c
typedef struct {
    long long wtime, btime;
    long long winc, binc;
    int       movestogo;
    long long movetime;
    int       depth;
    long long nodes;
    bool      infinite;
} SearchLimits;
```

Important fields:

- `depth`: fixed depth from `go depth N`.
- `nodes`: node cap from `go nodes N`.
- `movetime`: exact move time in milliseconds from `go movetime N`.
- `wtime`, `btime`: remaining clock time in milliseconds.
- `winc`, `binc`: increment in milliseconds.
- `movestogo`: expected moves until the next time control.
- `infinite`: true for `go infinite`.

`limits` can be `NULL`, so guard it before reading. If no relevant limit is set,
pick a conservative default. `basic_search.c` is the current reference for
combining fixed depth, movetime, clock time, increment, node limits, and an
overhead buffer.

## Board and Move Tools

The core engine exposes useful tools through headers in `src/`.

### Board

`Board` is declared in `src/board.h`. Useful fields and helpers:

- `b->turn`: side to move, `WHITE` or `BLACK`.
- `b->squares[sq]`: piece on a square, or `EMPTY`.
- `b->pieces[type]`: bitboard of all pieces of that type.
- `b->colours[colour]`: bitboard of all pieces for one side.
- `b->ply`: current ply, useful for mate-distance scores.
- `boardOccupancy(b)`: all occupied squares.
- `boardPieces(b, colour, type)`: pieces of one colour and type.
- `kingSquare(b, colour)`: king square for a colour.
- `boardInCheck(b)`: whether side to move is in check.
- `boardIsDraw(b)`: repetition or fifty-move draw state.
- `squareAttacked(b, sq, bySide)`: attack test.

Do not edit board fields by hand during search. Use `applyMove()` and
`revertMove()` with an `Undo` object:

```c
Undo u;
applyMove(b, move, &u);
...
revertMove(b, move, &u);
```

If you call `applyMove()` on pseudo-legal moves, you must reject moves that leave
the moving side's king in check. `basic_search.c` does this with:

```c
applyMove(b, m, u);
int mover = !b->turn;
if (squareAttacked(b, kingSquare(b, mover), b->turn)) {
    revertMove(b, m, u);
    return false;
}
```

### Moves

`Move` is a compact `uint16_t` declared in `src/types.h` and manipulated through
`src/move.h`:

- `NO_MOVE`: no move.
- `makeMove(from, to)`: normal move.
- `makeMoveFlag(from, to, CASTLING)` or another move type.
- `makePromotion(from, to, QUEEN)`: promotion move.
- `moveFrom(m)`, `moveTo(m)`, `moveType(m)`, `movePromoPiece(m)`.
- `moveToString(m, buf)`: UCI text such as `e2e4` or `e7e8q`.

Use the generated move list when possible instead of manufacturing moves by
hand. Generated moves include the correct flags for castling, en passant, and
promotion.

### Move Generation

`src/movegen.h` provides:

- `generateAllMoves(b, moves)`: pseudo-legal moves.
- `generateNoisyMoves(b, moves)`: captures and tactical moves.
- `generateQuietMoves(b, moves)`: quiet moves.
- `moveIsLegal(b, move)`: legality check for a move.

Allocate move arrays as `Move moves[MAX_MOVES];`. `MAX_MOVES` is declared in
`src/types.h`.

`generateAllMoves()` returns pseudo-legal moves. For a real search, either call
`moveIsLegal()` before accepting a root move, or apply/revert and test whether
the moving king is attacked. Never assume every generated move is legal.

### Bitboards and Attacks

`src/bitboard.h` and `src/attacks.h` are available for fast evaluation and move
ordering:

- `squareBB(sq)`, `popcount(bb)`, `poplsb(&bb)`.
- `shiftNorth/South/East/West(bb)`.
- `PawnAttacks[colour][sq]`, `KnightAttacks[sq]`, `KingAttacks[sq]`.
- `bishopAttacks(sq, occ)`, `rookAttacks(sq, occ)`,
  `queenAttacks(sq, occ)`, `pieceAttacks(type, sq, occ)`.
- `SquareDistance[from][to]`.

Only call `getlsb()`, `getmsb()`, or `poplsb()` on nonzero bitboards.

### Types and Constants

`src/types.h` defines the common constants:

- Colours: `WHITE`, `BLACK`, `COLOUR_NB`.
- Piece types: `PAWN`, `KNIGHT`, `BISHOP`, `ROOK`, `QUEEN`, `KING`.
- Pieces: `W_PAWN` through `B_KING`, plus `EMPTY`.
- Squares: `A1` through `H8`, plus `SQ_NONE`.
- Values: `VALUE_DRAW`, `VALUE_MATE`, `VALUE_INF`, `VALUE_NONE`,
  `VALUE_MATE_IN_MAX`.
- Helpers: `fileOf`, `rankOf`, `makeSquare`, `pieceType`, `pieceColour`,
  `makePiece`, `relativeSquare`, `relativeRank`, `isOk`.

Square numbering starts at `A1 = 0` and increases by file, then rank. Mirroring
from black's perspective is commonly done with `sq ^ 56` or `relativeSquare()`.

## Pattern: Scoring Each Root Move (one-ply heuristics)

Many useful algorithms sit between `first_legal.c` (take the first legal move)
and `basic_search.c` (full alpha-beta). A common middle ground is to score every
legal move by the position it produces and keep the best one. `square_maximization.c`
is the reference for this pattern.

> With the `algohelpers` vocabulary this is just `legalMoves` + `afterMove` (and
> `chooseHighestScoring` if scoring every move really is the whole algorithm). The
> longer form below explains what those helpers do under the hood  read it to
> understand the two things that are easy to get wrong.

The shape is always the same: generate moves, apply each legal one, score the
resulting position, revert, and track the maximum.

```c
Move moves[MAX_MOVES];
int  n = generateAllMoves(b, moves);

Move bestMove  = NO_MOVE;
int  bestScore = -1;

for (int i = 0; i < n; i++)
{
    Undo u;
    if (!applyIfLegal(b, moves[i], &u))   // skip pseudo-legal moves
        continue;

    int score = scorePosition(b);         // your heuristic
    revertMove(b, moves[i], &u);          // always revert before the next move

    if (score > bestScore)
    {
        bestScore = score;
        bestMove  = moves[i];
    }
}
```

Two things are easy to get wrong:

### Reject illegal moves yourself

`generateAllMoves()` is pseudo-legal, so wrap apply/revert in a helper that
rejects moves leaving your own king in check. This is the same helper used by
`basic_search.c`:

```c
static bool applyIfLegal(Board* b, Move m, Undo* u)
{
    applyMove(b, m, u);
    int mover = !b->turn;                 // applyMove already flipped the turn
    if (squareAttacked(b, kingSquare(b, mover), b->turn))
    {
        revertMove(b, m, u);
        return false;
    }
    return true;
}
```

### The side to move flips after `applyMove`

After `applyMove()`, `b->turn` is the *opponent*. The side that just moved is
`!b->turn`. When your heuristic is about the mover (mobility, material, king
safety, etc.), pass `!b->turn`, not `b->turn`:

```c
int score = mobilityCount(b, !b->turn);   // the side that just moved
```

### Iterating one side's pieces with bitboards

Heuristics that look at attack coverage iterate a side's pieces by type and
union their attack sets. Combine `b->pieces[type]` with `b->colours[colour]`
(or `boardPieces(b, colour, type)`) and walk the bits with `poplsb`:

```c
Bitboard occ     = boardOccupancy(b);
Bitboard us      = b->colours[colour];
Bitboard covered = 0ULL;

Bitboard pawns = b->pieces[PAWN] & us;
while (pawns)
    covered |= PawnAttacks[colour][poplsb(&pawns)];   // pawns: diagonal attacks only

for (int type = KNIGHT; type <= KING; type++)
{
    Bitboard bb = b->pieces[type] & us;
    while (bb)
        covered |= pieceAttacks(type, poplsb(&bb), occ);
}

int reachable = popcount(covered & ~us);  // exclude squares blocked by own pieces
```

Note that attack sets give pawn captures, not pushes, so a mobility heuristic
built this way ignores pawn advances. That is fine for an attack/coverage
metric; decide deliberately whether your heuristic wants pushes too.

## UCI Output

`searchPosition()` prints final `bestmove` and summary score/node lines. Your
algorithm may also print iterative `info` lines while searching:

```c
printf("info depth %d score cp %d nodes %llu time %lld\n",
       depth,
       score,
       (unsigned long long)nodes,
       elapsedMs);
fflush(stdout);
```

Keep output UCI-compatible. Use `info string ...` for free-form diagnostics.
Flush after progress lines if the engine may run for noticeable time.

## Development Checklist

1. Add `my_search.h` and `my_search.c`.
2. Export `const Algorithm MySearchAlgorithm`.
3. Fill every `Algorithm` field, using `NULL` for unsupported optional hooks.
4. Initialize every `SearchResult` field in `chooseMove`.
5. Respect `SearchLimits` that matter to the algorithm.
6. Generate moves into `Move moves[MAX_MOVES]`.
7. Accept only legal moves at the root.
8. Use `applyMove()`/`revertMove()` for search tree traversal.
9. Register the algorithm in `src/algorithm.c`.
10. Build and test through UCI, perft, bench, and play tools.

## Local Tools and Commands

Build native:

```sh
make native
```

Build with CMake:

```sh
cmake -S . -B build
cmake --build build
```

Run a perft smoke test:

```sh
./build/grug perft 5
```

Run the built-in bench positions:

```sh
./build/grug bench 4
```

Try the algorithm through UCI:

```text
uci
setoption name Algorithm value my_search
isready
position startpos
go depth 3
quit
```

Play against the engine with the helper script:

```sh
python3 tools/play.py --engine ./build/grug --side white --depth 3
```

The `bench/` directory contains a larger cutechess/worker setup for engine
testing against references. Use it after the algorithm is stable enough that
simple UCI and bench runs pass.

## Common Mistakes

- Returning a pseudo-legal move as `bestMove`. The wrapper catches this, but the
  algorithm will lose control of its chosen move.
- Forgetting to call `revertMove()` on every search path.
- Reading `limits->...` without checking that `limits` is not `NULL`.
- Reporting scores from White's perspective when the interface expects
  side-to-move perspective.
- Calling bitboard LSB/MSB helpers on zero.
- Using handmade promotion, castling, or en-passant moves instead of generated
  moves with the correct flags.
- Letting persistent state survive `ucinewgame` when it is game-specific.
