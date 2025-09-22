# ChessEngine

A UCI-compatible chess engine written in C++17 with alpha-beta search and positional evaluation.

## Features

- **UCI Protocol**: Compatible with chess GUIs like en-croissant
- **Alpha-Beta Search**: Efficient minimax search with pruning
- **Positional Evaluation**: Material + positional understanding
- **Complete Chess Rules**: Castling, en passant, promotion, check detection

## Build

```bash
mkdir build && cd build
cmake ..
make
```

## Usage

### With Chess GUI (en-croissant)
1. Add engine in en-croissant:
   - **Name**: ChessEngine v1.0
   - **Path**: `/path/to/chess-engine/build/chess_engine`
2. Play against it!

### Command Line
```bash
./chess_engine
# Then use UCI commands:
# uci
# isready
# position startpos
# go depth 4
# quit
```

## Engine Details

- **Search Depth**: 4 (configurable)
- **Evaluation**: Material + center control + development
- **Move Ordering**: Captures first, center moves preferred
- **Strength**: ~1500 ELO (estimated)

## Files

- `src/board.cpp` - Board representation and move execution
- `src/movegen.cpp` - Legal move generation
- `src/search.cpp` - Alpha-beta search and evaluation
- `src/uci.cpp` - UCI protocol interface