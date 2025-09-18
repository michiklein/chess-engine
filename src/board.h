#ifndef BOARD_H
#define BOARD_H

#include "types.h"
#include <array>
#include <vector>
#include <string>

class Board {
private:
    std::array<Piece, 64> squares;
    Color sideToMove;
    bool canCastleKingSide[2];   // [WHITE][BLACK]
    bool canCastleQueenSide[2];  // [WHITE][BLACK]
    Square enPassantSquare;
    int halfMoveClock;
    int fullMoveNumber;

public:
    Board();
    
    // Initialize board to starting position
    void setupStartingPosition();
    
    // Board access
    const Piece& pieceAt(Square sq) const { return squares[sq]; }
    void setPiece(Square sq, const Piece& piece) { squares[sq] = piece; }
    void clearSquare(Square sq) { squares[sq] = Piece(); }
    
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
    
    // Game state history for unmake
    struct GameState {
        Piece capturedPiece;
        bool castlingRights[4]; // [WHITE_KING][WHITE_QUEEN][BLACK_KING][BLACK_QUEEN]
        Square enPassantSquare;
        int halfMoveClock;
    };
    std::vector<GameState> gameHistory;
    
    // Board evaluation helpers
    bool isSquareAttacked(Square sq, Color attacker) const;
    bool isInCheck(Color color) const;
    bool isCheckmate() const;
    bool isStalemate() const;
    
    // FEN notation
    std::string toFEN() const;
    bool fromFEN(const std::string& fen);
    
    // Display
    std::string toString() const;
    
private:
    Square findKing(Color color) const;
    void updateCastlingRights(const Move& move);
    void updateEnPassant(const Move& move);
};

#endif // BOARD_H