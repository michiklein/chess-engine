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
    
    // Game state history for unmake (also pushed for null moves)
    struct GameState {
        bool castlingRights[4]; // [WHITE_KING][WHITE_QUEEN][BLACK_KING][BLACK_QUEEN]
        Square enPassantSquare;
        int halfMoveClock;
        int fullMoveNumber;
        Color sideToMove;
        std::array<Bitboard, 12> pieceBitboards;
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
    
    // Make/unmake moves
    void makeMove(const Move& move);
    void unmakeMove(const Move& move);
    void makeNullMove();
    void unmakeNullMove();
    
    // Board evaluation helpers
    bool isSquareAttacked(Square sq, Color attacker) const;
    bool isInCheck(Color color) const;
    bool isCheckmate() const;
    bool isStalemate() const;
    Square findKing(Color color) const;

    // Draw detection
    bool isRepetition() const;      // current position occurred earlier in history
    bool isDrawByFiftyMoves() const { return halfMoveClock >= 100; }
    bool isInsufficientMaterial() const;  // K vs K, KN vs K, KB vs K
    
    // Bitboard access
    Bitboard getPieceBitboard(PieceType type, Color color) const;
    int countAttackedSquares(Color color) const;
    Bitboard getWhitePieces() const { return whitePieces; }
    Bitboard getBlackPieces() const { return blackPieces; }
    Bitboard getAllPieces() const { return allPieces; }
    
    // FEN notation
    std::string toFEN() const;
    bool fromFEN(const std::string& fen);
    
private:
    // Helper functions
    int getPieceIndex(PieceType type, Color color) const;
    void pushState();
    void popState();
    void normalizeEnPassant();
    void updateCombinedBitboards();
    void updateCastlingRights(const Move& move);
    void updateEnPassant(const Move& move);
    
    // Attack generation centralized in MoveGenerator
    // Use MoveGenerator::getPawnAttacks/getKnightAttacks/getBishopAttacks/
    // getRookAttacks/getQueenAttacks/getKingAttacks instead
};

#endif // BOARD_H
