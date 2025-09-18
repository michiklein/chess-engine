#ifndef TYPES_H
#define TYPES_H

#include <cstdint>

// Basic chess types
enum class Color : uint8_t {
    WHITE = 0,
    BLACK = 1,
    NONE = 2
};

enum class PieceType : uint8_t {
    PAWN = 0,
    KNIGHT = 1,
    BISHOP = 2,
    ROOK = 3,
    QUEEN = 4,
    KING = 5,
    NONE = 6
};

struct Piece {
    PieceType type;
    Color color;
    
    Piece() : type(PieceType::NONE), color(Color::NONE) {}
    Piece(PieceType t, Color c) : type(t), color(c) {}
    
    bool isEmpty() const { return type == PieceType::NONE; }
};

// Square representation (0-63, a1=0, h8=63)
using Square = uint8_t;

// Move representation
struct Move {
    Square from;
    Square to;
    PieceType promotion;
    bool isCapture;
    bool isCastle;
    bool isEnPassant;
    
    Move() : from(0), to(0), promotion(PieceType::NONE), 
             isCapture(false), isCastle(false), isEnPassant(false) {}
    
    Move(Square f, Square t) : from(f), to(t), promotion(PieceType::NONE),
                               isCapture(false), isCastle(false), isEnPassant(false) {}
};

// Utility functions
constexpr int fileOf(Square sq) { return sq & 7; }
constexpr int rankOf(Square sq) { return sq >> 3; }
constexpr Square makeSquare(int file, int rank) { return rank * 8 + file; }

// Square constants
constexpr Square A1 = 0, B1 = 1, C1 = 2, D1 = 3, E1 = 4, F1 = 5, G1 = 6, H1 = 7;
constexpr Square A8 = 56, B8 = 57, C8 = 58, D8 = 59, E8 = 60, F8 = 61, G8 = 62, H8 = 63;

// Other color
constexpr Color operator~(Color c) { return Color(static_cast<uint8_t>(c) ^ static_cast<uint8_t>(Color::BLACK)); }

#endif // TYPES_H