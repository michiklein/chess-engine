#!/usr/bin/env python3
"""Regression check: engine plays itself one full game; every move is
validated with python-chess. Fails loudly on any illegal move."""
import pathlib
import subprocess
import sys

import chess

ROOT = pathlib.Path(__file__).resolve().parent.parent

eng = subprocess.Popen([str(ROOT / "build" / "chess_engine")],
                       stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                       stderr=subprocess.DEVNULL, text=True)

def send(s): eng.stdin.write(s + "\n"); eng.stdin.flush()
def expect(prefix):
    while True:
        line = eng.stdout.readline()
        if not line: sys.exit("engine died")
        if line.startswith(prefix): return line.strip()

send("uci"); expect("uciok")
board = chess.Board()
moves = []
for ply in range(400):
    if board.is_game_over(claim_draw=True):
        print("game over:", board.result(claim_draw=True), "-",
              board.outcome(claim_draw=True).termination.name)
        break
    send("position startpos" + (" moves " + " ".join(moves) if moves else ""))
    send("go movetime 100")
    bm = expect("bestmove").split()[1]
    if bm == "0000":
        print("engine reports no move; python-chess game_over =", board.is_game_over())
        break
    mv = chess.Move.from_uci(bm)
    if mv not in board.legal_moves:
        sys.exit(f"ILLEGAL MOVE {bm} at ply {ply}, fen {board.fen()}")
    board.push(mv)
    moves.append(bm)
send("quit")
print(f"plies: {len(moves)} | all moves legal per python-chess")
