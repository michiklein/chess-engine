#!/usr/bin/env python3
import subprocess, sys, chess

def selfplay(fen, max_plies=200, movetime=0.3):
    eng = subprocess.Popen(["./chess_engine"], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                           stderr=subprocess.DEVNULL, text=True,
                           cwd="/Users/michaelklein/Documents/chess-engine/build")
    def send(s): eng.stdin.write(s + "\n"); eng.stdin.flush()
    def expect(prefix):
        while True:
            line = eng.stdout.readline()
            if not line: sys.exit("engine died")
            if line.startswith(prefix): return line.strip()
    send("uci"); expect("uciok")
    board = chess.Board(fen); moves = []
    while not board.is_game_over(claim_draw=True) and len(moves) < max_plies:
        send(f"position fen {fen}" + (" moves " + " ".join(moves) if moves else ""))
        send(f"go movetime {int(movetime*1000)}")
        bm = expect("bestmove").split()[1]
        if bm == "0000": break
        mv = chess.Move.from_uci(bm)
        if mv not in board.legal_moves: sys.exit(f"ILLEGAL {bm}")
        board.push(mv); moves.append(bm)
    send("quit")
    return board.result(claim_draw=True), len(moves)

tests = [
    ("KR vs K (mop-up)",       "8/8/8/4k3/8/8/8/R3K3 w - - 0 1", "1-0"),
    ("KQ vs K",                "4k3/8/8/8/8/8/8/Q3K3 w - - 0 1", "1-0"),
    ("KP vs K won (far king)", "8/8/8/8/8/1k6/6PK/8 w - - 0 1",  "1-0"),
    ("KB vs K dead draw",      "8/8/8/4k3/8/8/8/B3K3 w - - 0 1", "1/2-1/2"),
]
for name, fen, want in tests:
    result, plies = selfplay(fen)
    status = "OK " if result == want else "FAIL"
    print(f"{status} {name}: {result} in {plies} plies (want {want})")
