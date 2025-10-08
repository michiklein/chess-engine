#include "board.h"
#include <sstream>
#include <cctype>

Board::Board() {
    setupStartingPosition();
}

void Board::setupStartingPosition() {
    // Clear all bitboards
    for (auto& bb : pieceBitboards) {
        bb = EMPTY_BOARD;
    }
    
    // Set up white pieces
    pieceBitboards[0] = setBit(setBit(setBit(setBit(setBit(setBit(setBit(setBit(EMPTY_BOARD, A2), B2), C2), D2), E2), F2), G2), H2); // White pawns
    pieceBitboards[1] = setBit(setBit(EMPTY_BOARD, B1), G1); // White knights
    pieceBitboards[2] = setBit(setBit(EMPTY_BOARD, C1), F1); // White bishops
    pieceBitboards[3] = setBit(setBit(EMPTY_BOARD, A1), H1); // White rooks
    pieceBitboards[4] = setBit(EMPTY_BOARD, D1); // White queen
    pieceBitboards[5] = setBit(EMPTY_BOARD, E1); // White king
    
    // Set up black pieces
    pieceBitboards[6] = setBit(setBit(setBit(setBit(setBit(setBit(setBit(setBit(EMPTY_BOARD, A7), B7), C7), D7), E7), F7), G7), H7); // Black pawns
    pieceBitboards[7] = setBit(setBit(EMPTY_BOARD, B8), G8); // Black knights
    pieceBitboards[8] = setBit(setBit(EMPTY_BOARD, C8), F8); // Black bishops
    pieceBitboards[9] = setBit(setBit(EMPTY_BOARD, A8), H8); // Black rooks
    pieceBitboards[10] = setBit(EMPTY_BOARD, D8); // Black queen
    pieceBitboards[11] = setBit(EMPTY_BOARD, E8); // Black king
    
    // Set initial game state
    sideToMove = Color::WHITE;
    canCastleKingSide[0] = canCastleKingSide[1] = true;
    canCastleQueenSide[0] = canCastleQueenSide[1] = true;
    enPassantSquare = 64; // Invalid square
    halfMoveClock = 0;
    fullMoveNumber = 1;
    
    updateCombinedBitboards();
}

Piece Board::pieceAt(Square sq) const {
    for (int i = 0; i < 12; i++) {
        if (getBit(pieceBitboards[i], sq)) {
            PieceType type = static_cast<PieceType>(i % 6);
            Color color = (i < 6) ? Color::WHITE : Color::BLACK;
            return Piece(type, color);
        }
    }
    return Piece(); // Empty piece
}

void Board::setPiece(Square sq, const Piece& piece) {
    // Clear the square first
    clearSquare(sq);
    
    if (!piece.isEmpty()) {
        int index = getPieceIndex(piece.type, piece.color);
        pieceBitboards[index] = setBit(pieceBitboards[index], sq);
    }
    
    updateCombinedBitboards();
}

void Board::clearSquare(Square sq) {
    for (auto& bb : pieceBitboards) {
        bb = clearBit(bb, sq);
    }
    updateCombinedBitboards();
}

bool Board::canCastle(Color color, bool kingSide) const {
    int colorIndex = static_cast<int>(color);
    return kingSide ? canCastleKingSide[colorIndex] : canCastleQueenSide[colorIndex];
}

void Board::setCastlingRights(Color color, bool kingSide, bool canCastle) {
    int colorIndex = static_cast<int>(color);
    if (kingSide) {
        canCastleKingSide[colorIndex] = canCastle;
    } else {
        canCastleQueenSide[colorIndex] = canCastle;
    }
}

void Board::makeMove(const Move& move) {
    // Save game state for unmake
    GameState state;
    state.capturedPiece = pieceAt(move.to);
    state.castlingRights[0] = canCastleKingSide[0];
    state.castlingRights[1] = canCastleQueenSide[0];
    state.castlingRights[2] = canCastleKingSide[1];
    state.castlingRights[3] = canCastleQueenSide[1];
    state.enPassantSquare = enPassantSquare;
    state.halfMoveClock = halfMoveClock;
    state.whitePieces = whitePieces;
    state.blackPieces = blackPieces;
    state.allPieces = allPieces;
    gameHistory.push_back(state);
    
    Piece movingPiece = pieceAt(move.from);
    Piece capturedPiece = pieceAt(move.to);
    
    // Clear the from square
    clearSquare(move.from);
    
    // Handle special moves
    if (move.isCastle) {
        // Move the rook for castling
        if (move.to == G1) { // White king-side castle
            clearSquare(H1);
            setPiece(F1, Piece(PieceType::ROOK, Color::WHITE));
        } else if (move.to == C1) { // White queen-side castle
            clearSquare(A1);
            setPiece(D1, Piece(PieceType::ROOK, Color::WHITE));
        } else if (move.to == G8) { // Black king-side castle
            clearSquare(H8);
            setPiece(F8, Piece(PieceType::ROOK, Color::BLACK));
        } else if (move.to == C8) { // Black queen-side castle
            clearSquare(A8);
            setPiece(D8, Piece(PieceType::ROOK, Color::BLACK));
        }
    }
    
    if (move.isEnPassant) {
        // Remove the captured pawn
        Square capturedSquare = sideToMove == Color::WHITE ? move.to - 8 : move.to + 8;
        clearSquare(capturedSquare);
    }
    
    // Set the piece on the destination square
    PieceType finalPieceType = (move.promotion != PieceType::NONE) ? move.promotion : movingPiece.type;
    setPiece(move.to, Piece(finalPieceType, movingPiece.color));
    
    // Update game state
    updateCastlingRights(move);
    updateEnPassant(move);
    
    // Update move counters
    if (movingPiece.type == PieceType::PAWN || !capturedPiece.isEmpty()) {
        halfMoveClock = 0;
    } else {
        halfMoveClock++;
    }
    
    if (sideToMove == Color::BLACK) {
        fullMoveNumber++;
    }
    
    switchSideToMove();
}

void Board::unmakeMove(const Move& move) {
    if (gameHistory.empty()) return;
    
    // Restore game state
    GameState state = gameHistory.back();
    gameHistory.pop_back();
    
    canCastleKingSide[0] = state.castlingRights[0];
    canCastleQueenSide[0] = state.castlingRights[1];
    canCastleKingSide[1] = state.castlingRights[2];
    canCastleQueenSide[1] = state.castlingRights[3];
    enPassantSquare = state.enPassantSquare;
    halfMoveClock = state.halfMoveClock;
    whitePieces = state.whitePieces;
    blackPieces = state.blackPieces;
    allPieces = state.allPieces;
    
    // Restore piece bitboards
    for (int i = 0; i < 12; i++) {
        pieceBitboards[i] = EMPTY_BOARD;
    }
    
    // Reconstruct piece bitboards from combined bitboards
    for (Square sq = 0; sq < 64; sq++) {
        if (getBit(whitePieces, sq)) {
            // Find which white piece is on this square
            for (int i = 0; i < 6; i++) {
                if (getBit(pieceBitboards[i], sq)) {
                    break;
                }
            }
        }
        if (getBit(blackPieces, sq)) {
            // Find which black piece is on this square
            for (int i = 6; i < 12; i++) {
                if (getBit(pieceBitboards[i], sq)) {
                    break;
                }
            }
        }
    }
    
    // Switch side to move back
    switchSideToMove();
    
    if (sideToMove == Color::BLACK) {
        fullMoveNumber--;
    }
}

bool Board::isInCheck(Color color) const {
    Square kingSquare = findKing(color);
    return kingSquare < 64 && isSquareAttacked(kingSquare, ~color);
}

bool Board::isCheckmate() const {
    if (!isInCheck(sideToMove)) {
        return false;
    }
    
    // Generate all legal moves for current side
    // For now, we'll use a simple approach - this will be optimized later
    for (Square sq = 0; sq < 64; sq++) {
        Piece piece = pieceAt(sq);
        if (!piece.isEmpty() && piece.color == sideToMove) {
            // Check if this piece has any legal moves
            // This is a simplified check - full implementation would generate all moves
        }
    }
    
    return false; // Simplified for now
}

bool Board::isStalemate() const {
    if (isInCheck(sideToMove)) {
        return false;
    }
    
    // Similar to checkmate but not in check
    return false; // Simplified for now
}

Square Board::findKing(Color color) const {
    int kingIndex = getPieceIndex(PieceType::KING, color);
    if (pieceBitboards[kingIndex] == EMPTY_BOARD) {
        return 64; // King not found
    }
    return firstSquare(pieceBitboards[kingIndex]);
}

bool Board::isSquareAttacked(Square sq, Color attacker) const {
    Bitboard attackerPieces = (attacker == Color::WHITE) ? whitePieces : blackPieces;
    
    // Check pawn attacks
    Bitboard pawnAttacks = getPawnAttacks(sq, attacker);
    if (pawnAttacks & getPieceBitboard(PieceType::PAWN, attacker)) {
        return true;
    }
    
    // Check knight attacks
    Bitboard knightAttacks = getKnightAttacks(sq);
    if (knightAttacks & getPieceBitboard(PieceType::KNIGHT, attacker)) {
        return true;
    }
    
    // Check bishop/queen diagonal attacks
    Bitboard bishopAttacks = getBishopAttacks(sq, allPieces);
    if (bishopAttacks & (getPieceBitboard(PieceType::BISHOP, attacker) | getPieceBitboard(PieceType::QUEEN, attacker))) {
        return true;
    }
    
    // Check rook/queen straight attacks
    Bitboard rookAttacks = getRookAttacks(sq, allPieces);
    if (rookAttacks & (getPieceBitboard(PieceType::ROOK, attacker) | getPieceBitboard(PieceType::QUEEN, attacker))) {
        return true;
    }
    
    // Check king attacks
    Bitboard kingAttacks = getKingAttacks(sq);
    if (kingAttacks & getPieceBitboard(PieceType::KING, attacker)) {
        return true;
    }
    
    return false;
}

Bitboard Board::getPieceBitboard(PieceType type, Color color) const {
    int index = getPieceIndex(type, color);
    return pieceBitboards[index];
}

std::string Board::toString() const {
    std::ostringstream oss;
    
    for (int rank = 7; rank >= 0; rank--) {
        oss << (rank + 1) << " ";
        for (int file = 0; file < 8; file++) {
            Square sq = makeSquare(file, rank);
            Piece piece = pieceAt(sq);
            
            char symbol = '.';
            if (!piece.isEmpty()) {
                switch (piece.type) {
                    case PieceType::PAWN:   symbol = 'P'; break;
                    case PieceType::KNIGHT: symbol = 'N'; break;
                    case PieceType::BISHOP: symbol = 'B'; break;
                    case PieceType::ROOK:   symbol = 'R'; break;
                    case PieceType::QUEEN:  symbol = 'Q'; break;
                    case PieceType::KING:   symbol = 'K'; break;
                    default: symbol = '?'; break;
                }
                if (piece.color == Color::BLACK) {
                    symbol = std::tolower(symbol);
                }
            }
            oss << symbol << " ";
        }
        oss << "\n";
    }
    oss << "  a b c d e f g h\n";
    oss << "Side to move: " << (sideToMove == Color::WHITE ? "White" : "Black") << "\n";
    
    return oss.str();
}

std::string Board::toFEN() const {
    // Simplified FEN implementation - placeholder
    return "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
}

bool Board::fromFEN(const std::string& fen) {
    // Simplified FEN parsing - placeholder
    setupStartingPosition();
    return true;
}

// Helper functions
int Board::getPieceIndex(PieceType type, Color color) const {
    int typeIndex = static_cast<int>(type);
    int colorIndex = static_cast<int>(color);
    return typeIndex + (colorIndex * 6);
}

void Board::updateCombinedBitboards() {
    whitePieces = pieceBitboards[0] | pieceBitboards[1] | pieceBitboards[2] | 
                  pieceBitboards[3] | pieceBitboards[4] | pieceBitboards[5];
    blackPieces = pieceBitboards[6] | pieceBitboards[7] | pieceBitboards[8] | 
                  pieceBitboards[9] | pieceBitboards[10] | pieceBitboards[11];
    allPieces = whitePieces | blackPieces;
}

void Board::updateCastlingRights(const Move& move) {
    Piece movingPiece = pieceAt(move.to);
    
    // If king moves, lose all castling rights for that color
    if (movingPiece.type == PieceType::KING) {
        setCastlingRights(sideToMove, true, false);
        setCastlingRights(sideToMove, false, false);
    }
    
    // If rook moves from starting position, lose castling rights for that side
    if (move.from == A1 || move.to == A1) setCastlingRights(Color::WHITE, false, false);
    if (move.from == H1 || move.to == H1) setCastlingRights(Color::WHITE, true, false);
    if (move.from == A8 || move.to == A8) setCastlingRights(Color::BLACK, false, false);
    if (move.from == H8 || move.to == H8) setCastlingRights(Color::BLACK, true, false);
}

void Board::updateEnPassant(const Move& move) {
    enPassantSquare = 64; // Clear en passant by default
    
    Piece movingPiece = pieceAt(move.to);
    // Check for pawn double move
    if (movingPiece.type == PieceType::PAWN) {
        int fromRank = rankOf(move.from);
        int toRank = rankOf(move.to);
        
        if (abs(toRank - fromRank) == 2) {
            enPassantSquare = (move.from + move.to) / 2;
        }
    }
}

// Bitboard attack generation functions
Bitboard Board::getPawnAttacks(Square sq, Color color) const {
    Bitboard attacks = EMPTY_BOARD;
    int file = fileOf(sq);
    int rank = rankOf(sq);
    
    if (color == Color::WHITE) {
        if (file > 0 && rank < 7) attacks = setBit(attacks, makeSquare(file - 1, rank + 1));
        if (file < 7 && rank < 7) attacks = setBit(attacks, makeSquare(file + 1, rank + 1));
    } else {
        if (file > 0 && rank > 0) attacks = setBit(attacks, makeSquare(file - 1, rank - 1));
        if (file < 7 && rank > 0) attacks = setBit(attacks, makeSquare(file + 1, rank - 1));
    }
    
    return attacks;
}

Bitboard Board::getKnightAttacks(Square sq) const {
    Bitboard attacks = EMPTY_BOARD;
    int file = fileOf(sq);
    int rank = rankOf(sq);
    
    static const int knightMoves[8][2] = {
        {-2, -1}, {-2, 1}, {-1, -2}, {-1, 2},
        {1, -2}, {1, 2}, {2, -1}, {2, 1}
    };
    
    for (const auto& delta : knightMoves) {
        int newFile = file + delta[0];
        int newRank = rank + delta[1];
        
        if (newFile >= 0 && newFile < 8 && newRank >= 0 && newRank < 8) {
            attacks = setBit(attacks, makeSquare(newFile, newRank));
        }
    }
    
    return attacks;
}

Bitboard Board::getBishopAttacks(Square sq, Bitboard occupied) const {
    Bitboard attacks = EMPTY_BOARD;
    int file = fileOf(sq);
    int rank = rankOf(sq);
    
    // Diagonal directions
    static const int diagonalDirections[4][2] = {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}};
    
    for (const auto& dir : diagonalDirections) {
        for (int i = 1; i < 8; i++) {
            int newFile = file + i * dir[0];
            int newRank = rank + i * dir[1];
            
            if (newFile < 0 || newFile >= 8 || newRank < 0 || newRank >= 8) break;
            
            Square checkSquare = makeSquare(newFile, newRank);
            attacks = setBit(attacks, checkSquare);
            
            if (getBit(occupied, checkSquare)) break; // Blocked
        }
    }
    
    return attacks;
}

Bitboard Board::getRookAttacks(Square sq, Bitboard occupied) const {
    Bitboard attacks = EMPTY_BOARD;
    int file = fileOf(sq);
    int rank = rankOf(sq);
    
    // Straight directions
    static const int straightDirections[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    
    for (const auto& dir : straightDirections) {
        for (int i = 1; i < 8; i++) {
            int newFile = file + i * dir[0];
            int newRank = rank + i * dir[1];
            
            if (newFile < 0 || newFile >= 8 || newRank < 0 || newRank >= 8) break;
            
            Square checkSquare = makeSquare(newFile, newRank);
            attacks = setBit(attacks, checkSquare);
            
            if (getBit(occupied, checkSquare)) break; // Blocked
        }
    }
    
    return attacks;
}

Bitboard Board::getQueenAttacks(Square sq, Bitboard occupied) const {
    return getBishopAttacks(sq, occupied) | getRookAttacks(sq, occupied);
}

Bitboard Board::getKingAttacks(Square sq) const {
    Bitboard attacks = EMPTY_BOARD;
    int file = fileOf(sq);
    int rank = rankOf(sq);
    
    static const int kingMoves[8][2] = {
        {-1, -1}, {-1, 0}, {-1, 1},
        {0, -1},           {0, 1},
        {1, -1},  {1, 0},  {1, 1}
    };
    
    for (const auto& delta : kingMoves) {
        int newFile = file + delta[0];
        int newRank = rank + delta[1];
        
        if (newFile >= 0 && newFile < 8 && newRank >= 0 && newRank < 8) {
            attacks = setBit(attacks, makeSquare(newFile, newRank));
        }
    }
    
    return attacks;
}
