#ifndef MOVEGEN_H
#define MOVEGEN_H

#include "types.h"
#include "board.h"
#include <vector>

class MoveGenerator {
public:
    // Generate all legal moves for the current position
    static std::vector<Move> generateLegalMoves(const Board& board);
    
    // Generate all pseudo-legal moves (may leave king in check)
    static std::vector<Move> generatePseudoLegalMoves(const Board& board);
    
    // Check if a move is legal
    static bool isLegalMove(const Board& board, const Move& move);

private:
    // Generate moves for specific piece types
    static void generatePawnMoves(const Board& board, Square sq, std::vector<Move>& moves);
    static void generateKnightMoves(const Board& board, Square sq, std::vector<Move>& moves);
    static void generateBishopMoves(const Board& board, Square sq, std::vector<Move>& moves);
    static void generateRookMoves(const Board& board, Square sq, std::vector<Move>& moves);
    static void generateQueenMoves(const Board& board, Square sq, std::vector<Move>& moves);
    static void generateKingMoves(const Board& board, Square sq, std::vector<Move>& moves);
    
    // Helper functions
    static void generateCastlingMoves(const Board& board, std::vector<Move>& moves);
    static bool isSquareOnBoard(int file, int rank);
    static bool canMoveTo(const Board& board, Square from, Square to, Color movingColor);
};

#endif // MOVEGEN_H