#!/usr/bin/env python3
import argparse

import chess
import chess.engine


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--engine", default="./grug", help="path to the UCI engine")
    ap.add_argument("--side", choices=["white", "black"], default="white",
                    help="which colour you play")
    ap.add_argument("--movetime", type=float, default=2.0,
                    help="engine seconds per move")
    ap.add_argument("--depth", type=int, default=None,
                    help="fixed search depth (overrides --movetime)")
    ap.add_argument("--fen", default=None, help="start from a FEN")
    args = ap.parse_args()

    limit = (chess.engine.Limit(depth=args.depth) if args.depth
             else chess.engine.Limit(time=args.movetime))
    human = chess.WHITE if args.side == "white" else chess.BLACK

    board = chess.Board(args.fen) if args.fen else chess.Board()
    engine = chess.engine.SimpleEngine.popen_uci(args.engine)
    print(f"Engine: {engine.id.get('name', args.engine)}   "
          f"You: {args.side}   Limit: {limit}\n")

    try:
        while not board.is_game_over():
            print(board.unicode(borders=True, empty_square="."))
            print(f"\nFEN: {board.fen()}")
            if board.turn == human:
                move = read_human_move(board)
                if move is None:
                    break
            else:
                result = engine.play(board, limit, info=chess.engine.INFO_SCORE)
                move = result.move
                score = result.info.get("score")
                print(f"\nEngine plays: {board.san(move)}  ({move.uci()})"
                      f"   score: {score}\n")
            board.push(move)

        print("\n" + board.unicode(borders=True, empty_square="."))
        print("\nResult:", board.result(), "-", outcome_text(board))
    finally:
        engine.quit()


def read_human_move(board):
    while True:
        try:
            raw = input("Your move (SAN or UCI, 'q' to quit): ").strip()
        except EOFError:
            return None
        if raw in ("q", "quit", "exit"):
            return None
        for parse in (board.parse_san, board.parse_uci):
            try:
                move = parse(raw)
                if move in board.legal_moves:
                    return move
            except ValueError:
                pass
        print("  illegal / unparseable - try e.g. 'Nf3' or 'g1f3'")


def outcome_text(board):
    o = board.outcome(claim_draw=True)
    if o is None:
        return "aborted"
    if o.winner is None:
        return f"draw ({o.termination.name.lower()})"
    return f"{'white' if o.winner else 'black'} wins ({o.termination.name.lower()})"


if __name__ == "__main__":
    main()
