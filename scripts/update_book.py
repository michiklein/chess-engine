#!/usr/bin/env python3
"""Rebuild src/eco.pgn from the lichess-org/chess-openings database (CC0),
weighted by real master-game counts from the Lichess Masters Opening
Explorer, then regenerate the embedded book (src/eco_book.cpp).

The named-opening encyclopedia provides the *lines* (comprehensive, curated);
the Masters Explorer provides the *weights* (how often each move is actually
played in master games since --since). Joke openings stay present but get
negligible weight, so weighted-random selection effectively never picks them.

Usage:
    scripts/update_book.py                 # full run (network: TSVs + explorer)
    scripts/update_book.py --dry-run       # just count the positions to fetch
    scripts/update_book.py --since 2018

Requires python-chess. Explorer responses are cached (resumable).
"""
import argparse
import json
import pathlib
import subprocess
import sys
import time
import urllib.parse
import urllib.request

import chess

ROOT = pathlib.Path(__file__).resolve().parent.parent
BASE = "https://raw.githubusercontent.com/lichess-org/chess-openings/master"
EXPLORER = "https://explorer.lichess.ovh/masters"
UA = "kleinibot-book-builder/1.0 (github.com/michiklein/chess-engine)"


def read_token():
    for line in (ROOT / ".env").read_text().splitlines():
        if line.startswith("LICHESS_BOT_TOKEN="):
            return line.split("=", 1)[1].strip()
    return ""


def download_lines():
    """[(eco, name, [san, ...]), ...] from the chess-openings TSVs."""
    lines = []
    for volume in "abcde":
        req = urllib.request.Request(f"{BASE}/{volume}.tsv", headers={"User-Agent": UA})
        with urllib.request.urlopen(req) as resp:
            tsv = resp.read().decode()
        for row in tsv.splitlines()[1:]:
            eco, name, pgn = row.split("\t")[:3]
            sans = [t for t in pgn.split() if not t.endswith(".")]
            lines.append((eco, name, sans))
    return lines


def collect_positions(lines):
    """{epd: set(uci)} for every position the book plays a move from."""
    positions = {}
    for _, _, sans in lines:
        board = chess.Board()
        for san in sans:
            try:
                move = board.parse_san(san)
            except ValueError:
                break
            positions.setdefault(board.epd(), set()).add(move.uci())
            board.push(move)
    return positions


def fetch_weights(positions, since, cache_path, token):
    cache = {}
    if cache_path.exists():
        cache = json.loads(cache_path.read_text())
    todo = [epd for epd in positions if epd not in cache]
    print(f"{len(positions)} positions, {len(todo)} to fetch "
          f"({len(cache)} cached)", flush=True)

    headers = {"User-Agent": UA}
    if token:
        headers["Authorization"] = f"Bearer {token}"

    for i, epd in enumerate(todo):
        params = urllib.parse.urlencode(
            {"fen": epd, "since": since, "moves": 50, "topGames": 0})
        req = urllib.request.Request(f"{EXPLORER}?{params}", headers=headers)
        for attempt in range(5):
            try:
                with urllib.request.urlopen(req, timeout=30) as r:
                    data = json.loads(r.read().decode())
                cache[epd] = {m["uci"]: m["white"] + m["draws"] + m["black"]
                              for m in data.get("moves", [])}
                break
            except urllib.error.HTTPError as e:
                if e.code == 429:
                    print("rate limited, sleeping 65s...", flush=True)
                    time.sleep(65)
                else:
                    print(f"HTTP {e.code} for a position, skipping", flush=True)
                    cache[epd] = {}
                    break
            except Exception as e:
                print(f"error ({e}), retrying...", flush=True)
                time.sleep(5)
        else:
            cache[epd] = {}

        if (i + 1) % 25 == 0:
            cache_path.write_text(json.dumps(cache))
            print(f"  {i + 1}/{len(todo)} fetched", flush=True)
        time.sleep(2.0)  # be polite to the explorer (429s observed even at 1 req/s)

    cache_path.write_text(json.dumps(cache))
    return cache


def emit_book(lines, weights, out_path):
    out = []
    for eco, name, sans in lines:
        board = chess.Board()
        tokens = []
        for ply, san in enumerate(sans):
            try:
                move = board.parse_san(san)
            except ValueError:
                break
            # real master-game count for this move here; floor 1 keeps the
            # line reachable in principle while making it ~never chosen
            w = max(weights.get(board.epd(), {}).get(move.uci(), 0), 1)
            if ply % 2 == 0:
                tokens.append(f"{ply // 2 + 1}.")
            tokens.append(san)
            tokens.append(f"${w}")
            board.push(move)
        if not tokens:
            continue
        out += [f'[Site "{eco}"]', f'[White "{name}"]', "", " ".join(tokens), ""]
    out_path.write_text("\n".join(out) + "\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--since", type=int, default=2018,
                    help="only count master games from this year on")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--cache", default="/tmp/masters_weights_cache.json")
    args = ap.parse_args()

    lines = download_lines()
    positions = collect_positions(lines)
    total_moves = sum(len(v) for v in positions.values())
    print(f"{len(lines)} opening lines, {len(positions)} unique positions, "
          f"{total_moves} book moves", flush=True)
    if args.dry_run:
        return

    weights = fetch_weights(positions, args.since,
                            pathlib.Path(args.cache), read_token())
    emit_book(lines, weights, ROOT / "src" / "eco.pgn")
    print("wrote src/eco.pgn with master-game weights", flush=True)

    subprocess.run(["python3", str(ROOT / "scripts" / "embed_book.py")], check=True)


if __name__ == "__main__":
    main()
