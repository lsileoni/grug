-- Grug Bench schema (SQLite).
-- Source of truth for results is the `batches` table; each test row caches a
-- rolled-up aggregate (recomputed whenever a batch lands) for fast UI reads.

PRAGMA journal_mode = WAL;          -- let a worker submit while the UI reads
PRAGMA foreign_keys = ON;

-- Testable engine sources (seeded from config on startup).
CREATE TABLE IF NOT EXISTS engines (
    name    TEXT PRIMARY KEY,
    source  TEXT NOT NULL,          -- git url or local repo path
    build   TEXT NOT NULL,          -- build command, run in a checkout of <sha>
    binary  TEXT NOT NULL           -- produced binary, relative to the checkout
);

-- One row per test.
CREATE TABLE IF NOT EXISTS tests (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    type        TEXT NOT NULL,                      -- 'sprt' | 'gauntlet' | 'selftest'
    engine      TEXT NOT NULL REFERENCES engines(name),
    note        TEXT DEFAULT '',                    -- free-text label

    -- engine-under-test (the "dev"/"build" side) and the comparison side
    base_ref    TEXT, base_sha TEXT,                -- baseline (sprt); unused for gauntlet
    dev_ref     TEXT, dev_sha  TEXT,                -- patched build (sprt) / build (gauntlet)
    dev_options TEXT DEFAULT '{}',                  -- JSON UCI options for engine A
    base_options TEXT DEFAULT '{}',                 -- JSON UCI options for engine B builds
    reference   TEXT,                               -- reference engine name (gauntlet)
    ref_options TEXT DEFAULT '{}',                  -- JSON UCI options for the reference

    tc          TEXT NOT NULL DEFAULT '8+0.08',     -- cutechess time control
    book        TEXT NOT NULL DEFAULT 'starter.epd',
    elo0        REAL DEFAULT 0,                      -- SPRT hypotheses (logistic Elo)
    elo1        REAL DEFAULT 5,
    alpha       REAL DEFAULT 0.05,
    beta        REAL DEFAULT 0.05,
    max_pairs   INTEGER DEFAULT 0,                  -- cap (0 = unbounded for sprt; required for gauntlet)
    priority    INTEGER DEFAULT 0,

    status      TEXT NOT NULL DEFAULT 'pending',    -- pending|active|finished|stopped|failed
    result      TEXT DEFAULT '',                    -- 'accepted'|'rejected' (sprt verdict)

    -- cached rollup (recomputed from done batches)
    games   INTEGER DEFAULT 0,
    w INTEGER DEFAULT 0, d INTEGER DEFAULT 0, l INTEGER DEFAULT 0,
    p0 INTEGER DEFAULT 0, p1 INTEGER DEFAULT 0, p2 INTEGER DEFAULT 0,
    p3 INTEGER DEFAULT 0, p4 INTEGER DEFAULT 0,
    llr     REAL DEFAULT 0,
    elo     REAL DEFAULT 0, elo_lo REAL DEFAULT 0, elo_hi REAL DEFAULT 0, los REAL DEFAULT 0,

    created_at  TEXT DEFAULT (datetime('now')),
    updated_at  TEXT DEFAULT (datetime('now'))
);

-- A unit of work handed to a worker.
CREATE TABLE IF NOT EXISTS batches (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    test_id      INTEGER NOT NULL REFERENCES tests(id) ON DELETE CASCADE,
    worker_id    INTEGER,
    pairs        INTEGER NOT NULL,                  -- game-pairs requested
    status       TEXT NOT NULL DEFAULT 'assigned',  -- assigned|done|failed
    results      TEXT DEFAULT '{}',                 -- JSON {wdl, pentanomial, crashes, elapsed, pgn}
    games        INTEGER,                           -- cached count of games in results.pgn (for paginating the games view)
    assigned_at  TEXT DEFAULT (datetime('now')),
    completed_at TEXT
);

CREATE TABLE IF NOT EXISTS workers (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    name       TEXT UNIQUE,
    hostname   TEXT,
    cores      INTEGER DEFAULT 0,
    has_cutechess INTEGER DEFAULT 0,
    has_stockfish INTEGER DEFAULT 0,
    games_done INTEGER DEFAULT 0,
    last_seen  TEXT DEFAULT (datetime('now'))
);

CREATE INDEX IF NOT EXISTS idx_tests_status   ON tests(status, priority DESC, id);
CREATE INDEX IF NOT EXISTS idx_batches_test   ON batches(test_id, status);
CREATE INDEX IF NOT EXISTS idx_batches_status ON batches(status, assigned_at);
