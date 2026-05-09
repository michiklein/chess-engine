# Chess Engine

A chess engine written in C++17.

## Build

```bash
./build.sh
```

## Play

```bash
./build/chess_engine    # UCI engine for chess GUIs
```

Moves are entered in algebraic notation: `e4`, `Nf3`, `Bxc5`, `O-O`, or coordinate style: `e2e4`.

## Project Structure

```
src/
├── types.h              # core types, bitboard utilities
├── board.cpp/h          # board state, make/unmake move
├── movegen.cpp/h        # move generation
├── search.cpp/h         # alpha-beta search, evaluation
├── opening_book.cpp/h   # ECO opening book
├── uci.cpp/h            # UCI protocol
```
