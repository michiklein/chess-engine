#ifndef TYPES_H
#define TYPES_H

#include <cstdint>

// Bitboard type
using Bitboard = uint64_t;

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
constexpr Square A2 = 8, B2 = 9, C2 = 10, D2 = 11, E2 = 12, F2 = 13, G2 = 14, H2 = 15;
constexpr Square A3 = 16, B3 = 17, C3 = 18, D3 = 19, E3 = 20, F3 = 21, G3 = 22, H3 = 23;
constexpr Square A4 = 24, B4 = 25, C4 = 26, D4 = 27, E4 = 28, F4 = 29, G4 = 30, H4 = 31;
constexpr Square A5 = 32, B5 = 33, C5 = 34, D5 = 35, E5 = 36, F5 = 37, G5 = 38, H5 = 39;
constexpr Square A6 = 40, B6 = 41, C6 = 42, D6 = 43, E6 = 44, F6 = 45, G6 = 46, H6 = 47;
constexpr Square A7 = 48, B7 = 49, C7 = 50, D7 = 51, E7 = 52, F7 = 53, G7 = 54, H7 = 55;
constexpr Square A8 = 56, B8 = 57, C8 = 58, D8 = 59, E8 = 60, F8 = 61, G8 = 62, H8 = 63;

// Other color
constexpr Color operator~(Color c) { return Color(static_cast<uint8_t>(c) ^ static_cast<uint8_t>(Color::BLACK)); }

// Bitboard utility functions
constexpr Bitboard setBit(Bitboard bb, Square sq) { return bb | (1ULL << sq); }
constexpr Bitboard clearBit(Bitboard bb, Square sq) { return bb & ~(1ULL << sq); }
constexpr bool getBit(Bitboard bb, Square sq) { return (bb >> sq) & 1; }
constexpr int popCount(Bitboard bb) { return __builtin_popcountll(bb); }
constexpr Square firstSquare(Bitboard bb) { return __builtin_ctzll(bb); }

// Bitboard constants
constexpr Bitboard EMPTY_BOARD = 0ULL;
constexpr Bitboard FULL_BOARD = 0xFFFFFFFFFFFFFFFFULL;

// File and rank masks
constexpr Bitboard FILE_A = 0x0101010101010101ULL;
constexpr Bitboard FILE_H = 0x8080808080808080ULL;
constexpr Bitboard RANK_1 = 0x00000000000000FFULL;
constexpr Bitboard RANK_8 = 0xFF00000000000000ULL;

// Center squares
constexpr Bitboard CENTER_SQUARES = setBit(setBit(setBit(setBit(EMPTY_BOARD, makeSquare(3, 3)), makeSquare(3, 4)), makeSquare(4, 3)), makeSquare(4, 4));

#endif // TYPES_H