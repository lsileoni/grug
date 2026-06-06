import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "worker"))

import cutechess  # noqa: E402


def _game(rnd, white, black, result):
    return (f'[Round "{rnd}"]\n[White "{white}"]\n[Black "{black}"]\n'
            f'[Result "{result}"]\n\n1. e4 e5 {result}\n\n')


def _write(tmp_path, *games):
    p = os.path.join(str(tmp_path), "games.pgn")
    with open(p, "w") as fh:
        fh.write("".join(games))
    return p


def test_pair_a_sweeps(tmp_path):
    pgn = _write(tmp_path,
                 _game(1, "A", "B", "1-0"),
                 _game(1, "B", "A", "0-1"))
    wdl, penta = cutechess.score_pgn(pgn)
    assert wdl == [2, 0, 0]
    assert penta == [0, 0, 0, 0, 1]


def test_pair_b_sweeps(tmp_path):
    pgn = _write(tmp_path,
                 _game(1, "A", "B", "0-1"),
                 _game(1, "B", "A", "1-0"))
    wdl, penta = cutechess.score_pgn(pgn)
    assert wdl == [0, 0, 2]
    assert penta == [1, 0, 0, 0, 0]


def test_pair_split_is_a_draw_bucket(tmp_path):
    pgn = _write(tmp_path,
                 _game(1, "A", "B", "1-0"),
                 _game(1, "B", "A", "1-0"))
    wdl, penta = cutechess.score_pgn(pgn)
    assert wdl == [1, 0, 1]
    assert penta == [0, 0, 1, 0, 0]


def test_both_draws(tmp_path):
    pgn = _write(tmp_path,
                 _game(1, "A", "B", "1/2-1/2"),
                 _game(1, "B", "A", "1/2-1/2"))
    wdl, penta = cutechess.score_pgn(pgn)
    assert wdl == [0, 2, 0]
    assert penta == [0, 0, 1, 0, 0]


def test_win_plus_draw_is_bucket_3(tmp_path):
    pgn = _write(tmp_path,
                 _game(1, "A", "B", "1-0"),
                 _game(1, "B", "A", "1/2-1/2"))
    wdl, penta = cutechess.score_pgn(pgn)
    assert wdl == [1, 1, 0]
    assert penta == [0, 0, 0, 1, 0]


def test_pairs_grouped_by_round_not_file_order(tmp_path):
    pgn = _write(tmp_path,
                 _game(1, "A", "B", "1-0"),
                 _game(2, "A", "B", "0-1"),
                 _game(1, "B", "A", "0-1"),
                 _game(2, "B", "A", "1-0"))
    wdl, penta = cutechess.score_pgn(pgn)
    assert wdl == [2, 0, 2]
    assert penta == [1, 0, 0, 0, 1]


def test_incomplete_pair_skipped_from_pentanomial(tmp_path):
    pgn = _write(tmp_path,
                 _game(1, "A", "B", "1-0"),
                 _game(1, "B", "A", "*"))
    wdl, penta = cutechess.score_pgn(pgn)
    assert wdl == [1, 0, 0]
    assert penta == [0, 0, 0, 0, 0]


def test_missing_file_is_empty():
    wdl, penta = cutechess.score_pgn("/no/such/file.pgn")
    assert wdl == [0, 0, 0]
    assert penta == [0, 0, 0, 0, 0]
