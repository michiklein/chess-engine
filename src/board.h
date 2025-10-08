#ifndef BOARD_H
#define BOARD_H

#include "types.h"
#include <array>
#include <vector>
#include <string>

class Board {
private:
    // Bitboards for each piece type and color
    std::array<Bitboard, 12> pieceBitboards; // [WHITE_PAWN, WHITE_KNIGHT, WHITE_BISHOP, WHITE_ROOK, WHITE_QUEEN, WHITE_KING, BLACK_PAWN, ...]
    
    // Combined bitboards for faster operations
    Bitboard whitePieces;
    Bitboard blackPieces;
    Bitboard allPieces;
    
    // Game state
    Color sideToMove;
    bool canCastleKingSide[2];   // [WHITE][BLACK]
    bool canCastleQueenSide[2];  // [WHITE][BLACK]
    Square enPassantSquare;
    int halfMoveClock;
    int fullMoveNumber;
    
    // Game state history for unmake
    struct GameState {
        Piece capturedPiece;
        bool castlingRights[4]; // [WHITE_KING][WHITE_QUEEN][BLACK_KING][BLACK_QUEEN]
        Square enPassantSquare;
        int halfMoveClock;
        Bitboard whitePieces;
        Bitboard blackPieces;
        Bitboard allPieces;
    };
    std::vector<GameState> gameHistory;

public:
    Board();
    
    // Initialize board to starting position
    void setupStartingPosition();
    
    // Board access
    Piece pieceAt(Square sq) const;
    void setPiece(Square sq, const Piece& piece);
    void clearSquare(Square sq);
    
    // Game state
    Color getSideToMove() const { return sideToMove; }
    void setSideToMove(Color color) { sideToMove = color; }
    void switchSideToMove() { sideToMove = ~sideToMove; }
    
    // Castling rights
    bool canCastle(Color color, bool kingSide) const;
    void setCastlingRights(Color color, bool kingSide, bool canCastle);
    
    // En passant
    Square getEnPassantSquare() const { return enPassantSquare; }
    void setEnPassantSquare(Square sq) { enPassantSquare = sq; }
    
    // Move counters
    int getHalfMoveClock() const { return halfMoveClock; }
    int getFullMoveNumber() const { return fullMoveNumber; }
    void setHalfMoveClock(int count) { halfMoveClock = count; }
    void setFullMoveNumber(int count) { fullMoveNumber = count; }
    
    // Make/unmake moves
    void makeMove(const Move& move);
    void unmakeMove(const Move& move);
    
    // Board evaluation helpers
    bool isSquareAttacked(Square sq, Color attacker) const;
    bool isInCheck(Color color) const;
    bool isCheckmate() const;
    bool isStalemate() const;
    Square findKing(Color color) const;
    
    // Bitboard access
    Bitboard getPieceBitboard(PieceType type, Color color) const;
    Bitboard getWhitePieces() const { return whitePieces; }
    Bitboard getBlackPieces() const { return blackPieces; }
    Bitboard getAllPieces() const { return allPieces; }
    
    // FEN notation
    std::string toFEN() const;
    bool fromFEN(const std::string& fen);
    
    // Display
    std::string toString() const;
    
private:
    // Helper functions
    int getPieceIndex(PieceType type, Color color) const;
    void updateCombinedBitboards();
    void updateCastlingRights(const Move& move);
    void updateEnPassant(const Move& move);
    
    // Bitboard attack generation
    Bitboard getPawnAttacks(Square sq, Color color) const;
    Bitboard getKnightAttacks(Square sq) const;
    Bitboard getBishopAttacks(Square sq, Bitboard occupied) const;
    Bitboard getRookAttacks(Square sq, Bitboard occupied) const;
    Bitboard getQueenAttacks(Square sq, Bitboard occupied) const;
    Bitboard getKingAttacks(Square sq) const;
};

#endif // BOARD_H
