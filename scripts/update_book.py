#!/usr/bin/env python3
"""Rebuild src/eco.pgn from the lichess-org/chess-openings database (CC0),
then regenerate the embedded book (src/eco_book.cpp).

Usage: scripts/update_book.py   (needs network for the TSV downloads)
"""
import pathlib
import subprocess
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parent.parent
BASE = "https://raw.githubusercontent.com/lichess-org/chess-openings/master"

lines_out = []
openings = 0
for volume in "abcde":
    with urllib.request.urlopen(f"{BASE}/{volume}.tsv") as resp:
        tsv = resp.read().decode()
    for row in tsv.splitlines()[1:]:  # skip header
        eco, name, pgn = row.split("\t")[:3]
        lines_out.append(f'[Site "{eco}"]')
        lines_out.append(f'[White "{name}"]')
        lines_out.append("")
        lines_out.append(pgn)
        lines_out.append("")
        openings += 1

(ROOT / "src" / "eco.pgn").write_text("\n".join(lines_out) + "\n")
print(f"wrote src/eco.pgn: {openings} opening lines")

subprocess.run(["python3", str(ROOT / "scripts" / "embed_book.py")], check=True)
