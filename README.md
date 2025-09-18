# ChessEngine

A complete chess engine framework implemented in C++17, featuring UCI (Universal Chess Interface) support, minimax search with alpha-beta pruning, and a comprehensive chess rule implementation.

## Features

### Core Components
- **Board Representation**: Efficient 8x8 array-based board with piece-centric design
- **Move Generation**: Complete legal move generation for all piece types
- **Search Algorithm**: Minimax with alpha-beta pruning
- **Evaluation Function**: Material and positional evaluation
- **UCI Interface**: Standard chess engine protocol support
- **Game State Management**: Full support for castling, en passant, and move history

### Supported Chess Rules
- All standard chess piece movements (pawn, knight, bishop, rook, queen, king)
- Castling (both kingside and queenside)
- En passant captures
- Pawn promotion
- Check and checkmate detection
- Move validation and legal move generation

## Building

### Requirements
- CMake 3.16 or higher
- C++17 compatible compiler (GCC, Clang, MSVC)

### Build Instructions

```bash
# Clone the repository
git clone <repository-url>
cd chess-engine

# Create build directory
mkdir build
cd build

# Configure and build
cmake ..
make

# Run the engine
./chess_engine
```

## Usage

### UCI Interface
The engine supports the Universal Chess Interface (UCI) protocol, making it compatible with most chess GUIs.

Basic UCI commands:
```
uci                    # Initialize UCI mode
isready               # Check if engine is ready
ucinewgame           # Start a new game
position startpos     # Set starting position
position startpos moves e2e4 e7e5  # Set position with moves
go depth 5           # Search to depth 5
quit                 # Exit the engine
```

### Programmatic Usage
```cpp
#include "board.h"
#include "movegen.h"
#include "search.h"

// Create and initialize board
Board board;
board.setupStartingPosition();

// Generate legal moves
auto moves = MoveGenerator::generateLegalMoves(board);

// Search for best move
SearchEngine search;
SearchResult result = search.search(board, 5);
```

## Architecture

### Class Structure

#### Core Classes
- **`Board`**: Manages game state, piece positions, and move execution
- **`MoveGenerator`**: Generates legal and pseudo-legal moves
- **`SearchEngine`**: Implements minimax search with alpha-beta pruning
- **`UCIEngine`**: Handles UCI protocol communication

#### Data Types
- **`Piece`**: Represents chess pieces with type and color
- **`Move`**: Represents chess moves with special move flags
- **`Square`**: 8-bit square representation (0-63, a1=0, h8=63)

### Search Algorithm
The engine uses minimax search with alpha-beta pruning:
- Configurable search depth
- Material and positional evaluation
- Move ordering (can be extended)
- Transposition table support (ready for implementation)

### Evaluation Function
Current evaluation considers:
- Material values (P=100, N=300, B=300, R=500, Q=900, K=10000)
- Basic positional factors
- Pawn advancement bonus
- Piece centralization
- King safety (simplified)

## Extending the Engine

### Adding Features
1. **Transposition Tables**: Implement hash tables for position caching
2. **Move Ordering**: Add killer moves, history heuristics
3. **Quiescence Search**: Extend search for tactical sequences
4. **Opening Book**: Add opening move databases
5. **Endgame Tablebases**: Support for perfect endgame play

### Performance Improvements
1. **Bitboards**: Replace array-based representation
2. **Magic Bitboards**: Faster sliding piece attack generation
3. **Parallel Search**: Multi-threaded search implementation
4. **SIMD Evaluation**: Vectorized evaluation functions

## Testing

The framework includes basic testing capabilities. To run tests:

```cpp
// Example test
Board board;
auto moves = MoveGenerator::generateLegalMoves(board);
assert(moves.size() == 20); // Starting position has 20 legal moves
```

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests for new functionality
5. Submit a pull request

## License

This project is open source. See LICENSE file for details.

## Acknowledgments

- Chess programming community for algorithms and techniques
- UCI protocol specification
- Classical chess engine design patterns
