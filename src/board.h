#ifndef BOARD_H
#define BOARD_H

#include "types.h"
#include <array>
#include <cstdint>
#include <vector>
#include <string>

class Board {
private:
    // Bitboards for each piece type and color
    // [WHITE_PAWN..WHITE_KING, BLACK_PAWN..BLACK_KING]
    std::array<Bitboard, 12> pieceBitboards;
    // Mailbox mirror of the bitboards for O(1) pieceAt
    std::array<Piece, 64> squares;

    // Combined bitboards, kept incrementally up to date
    Bitboard whitePieces;
    Bitboard blackPieces;
    Bitboard allPieces;

    // Game state
    Color sideToMove;
    bool canCastleKingSide[2];   // [WHITE][BLACK]
    bool canCastleQueenSide[2];  // [WHITE][BLACK]
    Square enPassantSquare;      // 64 = none
    int halfMoveClock;
    int fullMoveNumber;
    uint64_t hash;               // zobrist key, kept incrementally up to date

    // Minimal undo record per move (also pushed for null moves)
    struct Undo {
        Piece captured;
        Square capturedSquare;   // 64 = no capture
        bool castleK[2], castleQ[2];
        Square enPassant;
        int halfMoveClock;
        uint64_t hash;
    };
    std::vector<Undo> undoStack;

public:
    Board();

    void setupStartingPosition();

    // Board access
    Piece pieceAt(Square sq) const { return squares[sq]; }

    // Game state
    Color getSideToMove() const { return sideToMove; }
    uint64_t getHash() const { return hash; }

    // Castling rights
    bool canCastle(Color color, bool kingSide) const {
        int i = static_cast<int>(color);
        return kingSide ? canCastleKingSide[i] : canCastleQueenSide[i];
    }

    // En passant
    Square getEnPassantSquare() const { return enPassantSquare; }

    // Make/unmake moves
    void makeMove(const Move& move);
    void unmakeMove(const Move& move);
    void makeNullMove();
    void unmakeNullMove();

    // Attacks and checks
    bool isSquareAttacked(Square sq, Color attacker) const;
    bool isInCheck(Color color) const;
    Square findKing(Color color) const;

    // Draw detection
    bool isRepetition() const;      // current position occurred earlier in history
    bool isDrawByFiftyMoves() const { return halfMoveClock >= 100; }
    bool isInsufficientMaterial() const;  // K vs K, KN vs K, KB vs K

    // Bitboard access
    Bitboard getPieceBitboard(PieceType type, Color color) const {
        return pieceBitboards[getPieceIndex(type, color)];
    }
    int countAttackedSquares(Color color) const;
    Bitboard getWhitePieces() const { return whitePieces; }
    Bitboard getBlackPieces() const { return blackPieces; }
    Bitboard getAllPieces() const { return allPieces; }

    // FEN notation
    bool fromFEN(const std::string& fen);

private:
    static int getPieceIndex(PieceType type, Color color) {
        return static_cast<int>(type) + static_cast<int>(color) * 6;
    }

    void addPiece(Square sq, Piece piece);
    void removePiece(Square sq);
    void updateCastlingRights(const Move& move, PieceType movedType);
    void normalizeEnPassant();
    uint64_t castlingHash() const;
    void recomputeHash();
};

#endif // BOARD_H
