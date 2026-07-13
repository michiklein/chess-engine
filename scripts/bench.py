#!/usr/bin/env python3
"""Benchmark: search a fixed set of positions at fixed depth, report nodes/nps.

Works with any UCI engine, so it can compare builds:
    scripts/bench.py                        # current build
    scripts/bench.py --engine /tmp/old --depth 8
"""
import argparse
import pathlib
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parent.parent

POSITIONS = [
    # middlegames (tactical and quiet) and endgames
    "r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
    "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
    "r2q1rk1/ppp2ppp/3p1n2/2bPp3/2B1P1b1/2NP1N2/PPP2PPP/R1BQ1RK1 w - - 4 8",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
    "8/8/1p2k3/p1p2p2/P1P2P2/1P2K3/8/8 w - - 0 1",
    "8/3k4/8/3K4/3P4/8/8/8 w - - 0 1",
    "6k1/5ppp/1q6/8/8/1Q6/5PPP/6K1 w - - 0 1",
    "2r3k1/pp3ppp/8/3p4/3P4/8/PP3PPP/2R3K1 w - - 0 1",
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--engine", default=str(ROOT / "build" / "chess_engine"))
    ap.add_argument("--depth", type=int, default=9)
    args = ap.parse_args()

    eng = subprocess.Popen([args.engine], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                           stderr=subprocess.DEVNULL, text=True)

    def send(s): eng.stdin.write(s + "\n"); eng.stdin.flush()

    def search(fen):
        """Returns nodes searched for one position (last info line's count)."""
        send(f"position fen {fen}")
        send(f"go depth {args.depth}")
        nodes = 0
        while True:
            line = eng.stdout.readline()
            if not line: sys.exit("engine died")
            parts = line.split()
            if line.startswith("info") and "nodes" in parts:
                nodes = int(parts[parts.index("nodes") + 1])
            if line.startswith("bestmove"):
                return nodes

    send("uci")
    while not eng.stdout.readline().startswith("uciok"):
        pass

    total_nodes = 0
    start = time.perf_counter()
    for fen in POSITIONS:
        total_nodes += search(fen)
    elapsed = time.perf_counter() - start
    send("quit")

    print(f"depth {args.depth}: {total_nodes} nodes in {elapsed:.2f}s "
          f"= {int(total_nodes / elapsed)} nps")


if __name__ == "__main__":
    main()
