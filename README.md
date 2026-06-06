# grug

A C11 UCI chess engine. The interesting part is that move-selection logic lives
in swappable **algorithms**, so you can write and benchmark different approaches
against each other.

## Build & run

```sh
make native              # builds build/grug
./build/grug perft 5     # smoke test
./build/grug bench       # built-in benchmark positions
```

Then talk to it over UCI:

```text
uci
setoption name Algorithm value basic_search
position startpos
go depth 3
```

Or play it with the helper script:

```sh
python3 tools/play.py --engine ./build/grug --side white --depth 3
```

If you don't want to install the toolchain locally, `make env` builds and runs
everything in a Docker container. See [GETTINGSTARTED.md](GETTINGSTARTED.md).

## Writing an algorithm

Algorithms live in [src/algorithms/](src/algorithms/). Each one is a small module
exporting a single `const Algorithm` struct, selected at runtime via the UCI
`Algorithm` option.

To add one:

1. Create `src/algorithms/my_search.{c,h}`.
2. Export a `const Algorithm` (copy an existing file as a template).
3. Register it in [src/algorithm.c](src/algorithm.c).

The build globs `src/algorithms/*.c`, so no Makefile edits are needed.

Start by reading existing examples - `first_legal.c` (minimal), `square_maximization.c`
(one-ply heuristic), and `basic_search.c` (real alpha-beta search). The full guide,
including the engine APIs you can call, is in
[src/algorithms/README.md](src/algorithms/README.md).

## Benchmarking

`bench/` is a Flask + SQLite server with a worker pool that plays two git refs
against each other (SPRT or fixed games) using fastchess/cutechess, so you can
tell whether a change actually made the engine stronger.

```sh
cd bench
./benchctl start both --config config.yaml    # start server + worker
./benchctl status --config config.yaml
./benchctl stop --config config.yaml
```

The UI is at <http://127.0.0.1:8000> (or `:8001` when running via `make env`).
Create a test from the web UI (`/new`) or the API. Setup, config, and the
production deployment story are all in [GETTINGSTARTED.md](GETTINGSTARTED.md).

## Useful commands

| Command | What it does |
| --- | --- |
| `make native` | Build `build/grug` |
| `make test` | Run a perft sanity check |
| `make bench` | Run built-in bench positions |
| `make format` | Format source with clang-format |
| `make style-check` | Check formatting without editing |
| `make clean` | Remove build artifacts |
| `make env` | Build & start the Docker dev container |
