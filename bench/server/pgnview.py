"""PGN-derivation helpers for the games view.

Pure functions over PGN text: split a batch's PGN into games, parse tags, and
replay a game into a list of board positions for the web viewer. They live in
their own module so both the request path (``app``) and the ingest path
(``scheduler``) can use them without importing each other.

Replaying a game with python-chess (``board.san``/``board.push`` per move) is
the expensive step, so callers cache the result - see ``app._batch_games`` for
the per-batch memoisation that, together with pagination, keeps the public
games page from re-parsing every game on every request.
"""
from __future__ import annotations

import re
from io import StringIO

import chess.pgn

_PGN_TAG_RE = re.compile(r'^\[(\w+)\s+"(.*)"\]\s*$')


def split_pgn_games(pgn: str) -> list[str]:
    games = []
    cur = []
    for line in pgn.splitlines():
        if line.startswith("[Event ") and cur:
            games.append("\n".join(cur).strip())
            cur = []
        cur.append(line)
    if cur:
        text = "\n".join(cur).strip()
        if text:
            games.append(text)
    return games


def count_games(pgn: str) -> int:
    """Number of games in a batch's PGN - cheap (no move replay)."""
    return len(split_pgn_games(pgn))


def parse_pgn_game(pgn: str) -> dict:
    tags = {}
    move_lines = []
    in_tags = True
    for line in pgn.splitlines():
        m = _PGN_TAG_RE.match(line)
        if in_tags and m:
            tags[m.group(1)] = m.group(2)
            continue
        if line.strip():
            in_tags = False
            move_lines.append(line)
    return {"tags": tags, "moves": "\n".join(move_lines).strip(), "pgn": pgn,
            "positions": positions_from_pgn(pgn)}


def positions_from_pgn(pgn: str) -> list[dict]:
    game = chess.pgn.read_game(StringIO(pgn))
    if not game:
        return []
    board = game.board()
    positions = [{"ply": 0, "move": "Start", "fen": board.fen()}]
    for node in game.mainline():
        san = board.san(node.move)
        board.push(node.move)
        positions.append({"ply": board.ply(), "move": san, "fen": board.fen()})
    return positions


def a_score(tags: dict) -> str:
    result = tags.get("Result", "*")
    white = tags.get("White", "")
    if result == "1/2-1/2":
        return "0.5"
    if result not in ("1-0", "0-1"):
        return ""
    white_won = result == "1-0"
    return "1" if (white == "A") == white_won else "0"


def game_record(batch_id: int, idx: int, pgn: str) -> dict:
    """A single game's row data for the games table (no global ``number``)."""
    parsed = parse_pgn_game(pgn)
    tags = parsed["tags"]
    return {
        "batch_id": batch_id,
        "batch_game": idx,
        "event": tags.get("Event", ""),
        "round": tags.get("Round", ""),
        "white": tags.get("White", ""),
        "black": tags.get("Black", ""),
        "result": tags.get("Result", "*"),
        "a_score": a_score(tags),
        **parsed,
    }
