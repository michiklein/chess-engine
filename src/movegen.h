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

    // All pieces of both colors attacking sq, given an occupancy (which may
    // differ from the board's, e.g. during static exchange evaluation)
    static Bitboard attackersTo(const Board& board, Square sq, Bitboard occupied);

    // Bitboard attack generation (shared by Board and move generation)
    static Bitboard getPawnAttacks(Square sq, Color color);
    static Bitboard getKnightAttacks(Square sq);
    static Bitboard getBishopAttacks(Square sq, Bitboard occupied);
    static Bitboard getRookAttacks(Square sq, Bitboard occupied);
    static Bitboard getQueenAttacks(Square sq, Bitboard occupied);
    static Bitboard getKingAttacks(Square sq);

private:
    // Generate moves for specific piece types using bitboards
    static void generatePawnMoves(const Board& board, Square sq, std::vector<Move>& moves);
    static void generateKnightMoves(const Board& board, Square sq, std::vector<Move>& moves);
    static void generateBishopMoves(const Board& board, Square sq, std::vector<Move>& moves);
    static void generateRookMoves(const Board& board, Square sq, std::vector<Move>& moves);
    static void generateQueenMoves(const Board& board, Square sq, std::vector<Move>& moves);
    static void generateKingMoves(const Board& board, Square sq, std::vector<Move>& moves);
    
    static void generateCastlingMoves(const Board& board, std::vector<Move>& moves);
    
};

#endif // MOVEGEN_H
