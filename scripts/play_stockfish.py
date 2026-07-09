#!/usr/bin/env python3
"""Play a match between this chess engine and Stockfish over UCI.

Alternates colors each game, prints a running score, and appends every game
to a PGN file. Requires python-chess and a stockfish binary on PATH.

Examples:
    scripts/play_stockfish.py                     # 100 games, full-strength Stockfish
    scripts/play_stockfish.py --games 10 --elo 1500
    scripts/play_stockfish.py --skill 3 --movetime 50
"""
import argparse
import datetime
import pathlib
import sys

try:
    import chess
    import chess.engine
    import chess.pgn
except ImportError:
    sys.exit("python-chess is required: pip install chess")

ROOT = pathlib.Path(__file__).resolve().parent.parent
MAX_PLIES = 400  # safety cap; adjudicated as a draw


def main():
    ap = argparse.ArgumentParser(description="Match: chess_engine vs Stockfish")
    ap.add_argument("--games", type=int, default=100, help="number of games (default 100)")
    ap.add_argument("--movetime", type=int, default=100, help="ms per move for both engines (default 100)")
    ap.add_argument("--elo", type=int, help="limit Stockfish strength via UCI_Elo (min 1320)")
    ap.add_argument("--skill", type=int, help="Stockfish Skill Level 0-20")
    ap.add_argument("--stockfish", default="stockfish", help="path to stockfish binary")
    ap.add_argument("--engine", default=str(ROOT / "build" / "chess_engine"), help="path to our engine")
    ap.add_argument("--pgn", default=str(ROOT / "games" / "vs_stockfish.pgn"), help="output PGN file (appended)")
    args = ap.parse_args()

    engine_path = pathlib.Path(args.engine)
    if not engine_path.exists():
        sys.exit(f"engine not found at {engine_path} — run ./build.sh first")

    pgn_path = pathlib.Path(args.pgn)
    pgn_path.parent.mkdir(parents=True, exist_ok=True)

    sf_options = {}
    if args.elo is not None:
        sf_options = {"UCI_LimitStrength": True, "UCI_Elo": args.elo}
    if args.skill is not None:
        sf_options["Skill Level"] = args.skill
    sf_name = "Stockfish" + (f" (Elo {args.elo})" if args.elo else "") + \
              (f" (Skill {args.skill})" if args.skill is not None else "")

    ours = chess.engine.SimpleEngine.popen_uci(str(engine_path))
    sf = chess.engine.SimpleEngine.popen_uci(args.stockfish)
    if sf_options:
        sf.configure(sf_options)

    limit = chess.engine.Limit(time=args.movetime / 1000)
    wins = losses = draws = 0
    date = datetime.date.today().strftime("%Y.%m.%d")

    try:
        for g in range(1, args.games + 1):
            ours_is_white = (g % 2 == 1)
            board = chess.Board()
            termination = None

            while not board.is_game_over(claim_draw=True) and len(board.move_stack) < MAX_PLIES:
                engine = ours if (board.turn == chess.WHITE) == ours_is_white else sf
                try:
                    result = engine.play(board, limit, game=g)
                except chess.engine.EngineError as e:
                    termination = f"engine error: {e}"
                    break
                if result.move is None or result.move not in board.legal_moves:
                    who = "chess_engine" if engine is ours else "stockfish"
                    termination = f"forfeit: {who} returned invalid move {result.move}"
                    break
                board.push(result.move)

            if termination and termination.startswith("forfeit"):
                # invalid move loses for the side that produced it
                offender_white = ("chess_engine" in termination) == ours_is_white
                result_str = "0-1" if offender_white else "1-0"
            elif len(board.move_stack) >= MAX_PLIES and not board.is_game_over(claim_draw=True):
                result_str = "1/2-1/2"
                termination = f"adjudicated draw at {MAX_PLIES} plies"
            else:
                result_str = board.result(claim_draw=True)

            our_result = result_str if ours_is_white else {"1-0": "0-1", "0-1": "1-0"}.get(result_str, result_str)
            if our_result == "1-0":
                wins += 1
            elif our_result == "0-1":
                losses += 1
            else:
                draws += 1

            game = chess.pgn.Game.from_board(board)
            game.headers["Event"] = "chess_engine vs Stockfish match"
            game.headers["Site"] = "local"
            game.headers["Date"] = date
            game.headers["Round"] = str(g)
            game.headers["White"] = "chess_engine" if ours_is_white else sf_name
            game.headers["Black"] = sf_name if ours_is_white else "chess_engine"
            game.headers["Result"] = result_str
            game.headers["TimeControl"] = f"{args.movetime}ms/move"
            if termination:
                game.headers["Termination"] = termination
            with open(pgn_path, "a") as f:
                print(game, file=f)
                print(file=f)

            side = "white" if ours_is_white else "black"
            print(f"game {g:3d}/{args.games} ({side}): {result_str:7s}  "
                  f"record W-L-D: {wins}-{losses}-{draws}", flush=True)
    finally:
        ours.quit()
        sf.quit()

    score = wins + draws / 2
    print(f"\nFINAL RECORD vs {sf_name}: {wins} wins, {losses} losses, {draws} draws "
          f"({score}/{args.games})")
    print(f"games saved to {pgn_path}")


if __name__ == "__main__":
    main()
