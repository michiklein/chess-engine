# Chess Engine

A chess engine written in C++17.

## Build

```bash
./build.sh
```

The binary is fully self-contained: the ECO opening book is embedded at build
time (see `scripts/embed_book.py`). An `eco.pgn` placed next to the executable
overrides the built-in book.

### Windows

```bash
./scripts/build_windows.sh   # needs mingw-w64; outputs dist/windows/chess_engine.exe
```

## Play

```bash
./build/chess_engine    # UCI engine for chess GUIs
```

Moves are entered in algebraic notation: `e4`, `Nf3`, `Bxc5`, `O-O`, or coordinate style: `e2e4`.

## Test

```bash
cd build && ctest        # perft: validates move generation against known node counts
```

## Project Structure

```
src/
├── types.h              # core types, bitboard utilities
├── board.cpp/h          # board state, make/unmake move
├── movegen.cpp/h        # move generation
├── search.cpp/h         # alpha-beta search, evaluation
├── opening_book.cpp/h   # ECO opening book
├── uci.cpp/h            # UCI protocol
tests/
├── perft.cpp            # move generation correctness tests
```
