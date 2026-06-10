# Writing Algorithms

An algorithm is one C file that answers one question: **given this position,
which move?** Everything else - UCI, time parsing, output, legality safety
nets - is the engine's job.

## The whole interface

```c
#include "../algohelpers.h"

static void myChooseMove(Board* b, const SearchLimits* limits, SearchResult* result)
{
    MoveList list = legalMoves(b);
    if (list.count > 0)
        result->bestMove = list.moves[0];
}

const Algorithm MySearchAlgorithm = {
    .name = "my_search",
    .description = "one line shown in logs",
    .chooseMove = myChooseMove,
};
```

That is a complete, playable algorithm. The engine guarantees:

- **The board is yours.** `b` is a scratch copy of the position; modify it
  freely, the engine keeps its own.
- **`limits` is never NULL.** Unset fields are zero.
- **`result` arrives initialized** (`bestMove` NO_MOVE, `score` VALUE_NONE,
  `nodes` 0). Set only what you know.
- **Bad output cannot crash a game.** If `bestMove` is missing or illegal, the
  engine prints an `info string` and plays the first legal move.

`Algorithm` has two more optional hooks, set them only if you need them:

- `.init` - runs once at startup (build lookup tables, allocate state).
- `.newGame` - runs on `ucinewgame` (clear game-local state).
- `.evaluate` - returns centipawns for the side to move; powers the `eval`
  command. Useful while developing a heuristic: you can probe what your
  algorithm thinks of any position.

## Asking questions: the vocabulary

[`algohelpers.h`](../algohelpers.h) is a chess vocabulary: plain functions,
each answering one question about a position. Your algorithm is the loop that
combines them - generate, score, compare, decide - and the meaning stays in
your file.

**Moves** - `legalMoves(b)` returns every legal move as a `MoveList` you
iterate by value. Castling, promotions, and en passant arrive with the right
flags; king safety is already filtered.

```c
MoveList list = legalMoves(b);
for (int i = 0; i < list.count; i++)
{
    Move m = list.moves[i];
    ...
}
```

**Looking ahead** - `boardAfter(b, m)` returns the position a move leads to as
a fresh copy. Nothing to undo, and your board is untouched:

```c
int us = sideToMove(b);              // capture this before looking ahead
for each move m:
    Board after = boardAfter(b, m);
    int   score = mobility(&after, us) - hangingValue(&after, us);
```

In `after` it is the opponent's turn, so keep asking about `us`, the side you
captured at the top.

**Position state** - `boardInCheck(b)` and `boardIsDraw(b)` (from board.h),
plus `isCheckmate(b)` and `isStalemate(b)`. On a lookahead position these
answer the strongest questions a one-ply search can ask:

```c
Board after = boardAfter(b, m);
if (isCheckmate(&after))             // m delivers mate - play it
    ...
if (boardIsDraw(&after))             // m allows a repetition/fifty-move draw
    ...
int replies = legalMoves(&after).count;  // fewer replies = more forcing
```

**Move consequences** - `moveIsCapture(b, m)`, `moveCaptured(b, m)`,
`moveGivesCheck(b, m)`, `captureGain(b, m)` (naive victim - attacker), and
`see(b, m)` - full static exchange evaluation, the reliable "is this capture
actually winning?".

**Threats** - `isHanging(b, sq)`, `hangingPieces(b, colour)`,
`hangingValue(b, colour)`.

**Pawns** - `isPassedPawn(b, sq)`, `isIsolatedPawn(b, sq)`,
`isDoubledPawn(b, sq)`; weigh advancement with `relativeRank(colour, sq)`
(types.h).

**Vision** - `sees(b, sq)` (what does this piece hit?), `attackersTo(b, sq)`,
`attackersOf(b, sq, colour)`, `isAttacked(b, sq, byColour)`, `isDefended(b, sq)`.
These compose: king danger in one line is
`popcount(sideAttacks(b, !us) & sees(b, kingSquare(b, us)))` - enemy coverage
of your king's neighborhood.

**Material & mobility** - `pieceValue(type)`, `materialCount`, `materialValue`,
`materialBalance`, `sideAttacks`, `mobility`.

**Squares & pieces** - `pieceOn(b, sq)`, `typeOn(b, sq)`, `colourOn(b, sq)`,
`isEmpty(b, sq)`.

**Time** - `timeBudgetMs(b, limits)` turns the UCI clock fields into "spend
this many milliseconds", overhead margin included; 0 means unconstrained.
Pair with `timeNowMs()` from `util.h`.

**Iteration** - `popNextSquare(&bb)` walks a bitboard's squares:

```c
for (int sq; (sq = popNextSquare(&bb)) != SQ_NONE; )
    ...
```

Every function's header comment in `algohelpers.h` says what question it
answers and when to reach for it.

## A worked example

`threat_aware.c` in this directory is the reference for composing the
vocabulary. Its entire logic:

```c
int      us = sideToMove(b);
MoveList list = legalMoves(b);

int bestScore = 0;
for (int i = 0; i < list.count; i++)
{
    Move m = list.moves[i];
    int  score = 0;

    if (moveIsCapture(b, m))
    {
        int exchange = see(b, m);
        if (exchange > 0)
            score += exchange;          // captures that actually win material
    }

    if (moveGivesCheck(b, m))
        score += CHECK_BONUS;           // lean toward forcing moves

    Board after = boardAfter(b, m);
    score -= hangingValue(&after, us);  // don't leave our pieces loose

    result->nodes++;
    if (result->bestMove == NO_MOVE || score > bestScore)
    {
        bestScore = score;
        result->bestMove = m;
        result->score = score;
    }
}
```

Each line is a chess question; the loop is the algorithm. Start from this
shape, swap in your own questions.

## The other examples

- `first_legal.c` - the minimal algorithm (plays the first legal move).
- `square_maximization.c` - one-ply heuristic: maximize your own mobility.
- `threat_aware.c` - the worked example above.
- `basic_search.c` - real alpha-beta search with quiescence, move ordering,
  and time management. The reference for deep searches.
- `e2e4.c` / `no_move.c` - deliberately misbehaving examples that demonstrate
  the engine's fallback safety net.

## Going deep: the fast path

`boardAfter` and `legalMoves` copy the board, which is perfect for one-ply
heuristics (a few dozen copies per move) but adds up in a search visiting
hundreds of thousands of nodes. Deep searches use make/unmake on one board
instead:

```c
Move moves[MAX_MOVES];
int  n = generateAllMoves(b, moves);   // movegen.h: pseudo-legal, fast

for (int i = 0; i < n; i++)
{
    Undo u;
    if (!applyIfLegal(b, moves[i], &u))   // skips moves leaving the king in check
        continue;

    int score = -search(b, depth - 1, ...);
    revertMove(b, moves[i], &u);          // every applied move must be reverted
    ...
}
```

Two rules apply on this path only:

1. **Revert every move you apply**, on every code path.
2. **The turn flips after a move is applied**: inside the recursion `b->turn`
   is the opponent; the side that just moved is `!b->turn`.

`basic_search.c` is the complete pattern - iterative deepening, quiescence,
ordering, and a time/node-limit stop - and is meant to be copied from.

## Adding and registering

For an algorithm named `my_search`:

1. Create `src/algorithms/my_search.h`:

   ```c
   #ifndef GRUG_ALGORITHMS_MY_SEARCH_H
   #define GRUG_ALGORITHMS_MY_SEARCH_H

   #include "../algorithm.h"

   extern const Algorithm MySearchAlgorithm;

   #endif
   ```

2. Create `src/algorithms/my_search.c` exporting the `Algorithm` (see the top
   of this document). Keep helpers `static`.

3. Register it in `src/algorithm.c`: add the `#include` and one entry to the
   `Algorithms[]` array.

The build globs `src/algorithms/*.c`; no build-system edits needed.

## Reading SearchLimits

`SearchLimits` carries the UCI `go` arguments; unset fields are zero.

- `depth` - fixed depth from `go depth N`.
- `nodes` - node cap from `go nodes N`.
- `movetime`, `wtime`/`btime`, `winc`/`binc`, `movestogo` - the clock. You
  rarely read these directly: `timeBudgetMs(b, limits)` does the maths.
- `infinite` - true for `go infinite`.

Algorithms that ignore limits entirely (like the one-ply examples) are fine -
they simply answer fast.

## Trying it out

```sh
make native                       # build build/grug
./build/grug perft 5              # move generator sanity check
./build/grug bench 4              # built-in positions at depth 4
```

Through UCI:

```text
uci
setoption name Algorithm value my_search
position startpos
go depth 3
eval
quit
```

Play against it:

```sh
python3 tools/play.py --engine ./build/grug --side white --depth 3
```

For real strength measurement, `bench/` runs engine-vs-engine matches (SPRT)
between two git refs.

## Conventions

- **Scores are centipawns from the side to move's perspective.** Positive
  means good for whoever is choosing. For forced mates report
  `VALUE_MATE - pliesToMate` (negative when being mated); the engine prints
  these as `mate N`.
- **`getlsb`/`getmsb`/`poplsb` (bitboard.h) require a nonzero bitboard.**
  `popNextSquare` is the checked alternative.
- **Game-local state belongs behind `.newGame`.** If your algorithm remembers
  anything between moves, clear it there.
