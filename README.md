# Chess Engine

A high-performance chess engine written in C++17 with bitboard representation, alpha-beta search, and advanced positional evaluation.

## 🚀 Quick Start

### Build Everything
```bash
./build.sh
```

That's it! This builds both the UCI engine and terminal game.

### Play Chess
```bash
# Interactive terminal game
./build/terminal_game

# UCI engine (for chess GUIs)
./build/chess_engine
```

## 🎮 How to Play

### Terminal Game
1. Run `./build/terminal_game`
2. Choose your color (white or black)
3. Enter moves in algebraic notation:
   - `e4` (pawn to e4)
   - `Nf3` (knight to f3)
   - `Bxc5` (bishop captures on c5)
   - `O-O` (kingside castling)
   - `Qdd5` (queen from d-file to d5)

### UCI Engine (Chess GUIs)
1. Add engine in your chess GUI:
   - **Name**: ChessEngine v2.0
   - **Path**: `/path/to/chess-engine/build/chess_engine`
2. Play against it!

## ⚡ Engine Features

### Performance
- **Bitboard Representation**: Ultra-fast move generation and attack detection
- **Search Depth**: 8 plies (4 moves ahead for each side)
- **Alpha-Beta Pruning**: Efficient search with cutoffs

### Evaluation
- **Material**: Pawn=100, Knight/Bishop=300, Rook=500, Queen=950, King=10000
- **Positional**: Center control, piece mobility, king safety
- **Tactical**: Attack bonuses, check/checkmate detection
- **Strategic**: Pawn structure, development, king attack patterns

### Chess Rules
- ✅ Complete move generation (all piece types)
- ✅ Castling (kingside and queenside)
- ✅ En passant captures
- ✅ Pawn promotion
- ✅ Check and checkmate detection
- ✅ Stalemate detection
- ✅ Algebraic notation support

## 🛠️ Manual Build (if needed)

```bash
mkdir build && cd build
cmake ..
make chess_engine    # UCI engine
make terminal_game   # Interactive game
```

## 📁 Project Structure

```
src/
├── board.cpp/h      # Array-based board (legacy)
├── bitboard.cpp/h   # Bitboard representation (new)
├── movegen.cpp/h    # Move generation
├── search.cpp/h     # Alpha-beta search & evaluation
├── uci.cpp/h        # UCI protocol interface
├── terminal_game.cpp # Interactive chess game
└── types.h          # Chess types and bitboard utilities
```

## 🎯 Engine Strength

The engine features:
- **Advanced Evaluation**: 7 different evaluation components
- **Tactical Vision**: King attack patterns and checkmate detection
- **Positional Understanding**: Center control, development, pawn structure
- **Fast Search**: Bitboard operations for maximum performance

Perfect for casual play and learning chess tactics!