#include "board.h"
#include <sstream>
#include <cctype>

Board::Board() {
    setupStartingPosition();
}

void Board::setupStartingPosition() {
    // Clear the board
    for (auto& square : squares) {
        square = Piece();
    }
    
    // Set up white pieces
    squares[A1] = Piece(PieceType::ROOK, Color::WHITE);
    squares[B1] = Piece(PieceType::KNIGHT, Color::WHITE);
    squares[C1] = Piece(PieceType::BISHOP, Color::WHITE);
    squares[D1] = Piece(PieceType::QUEEN, Color::WHITE);
    squares[E1] = Piece(PieceType::KING, Color::WHITE);
    squares[F1] = Piece(PieceType::BISHOP, Color::WHITE);
    squares[G1] = Piece(PieceType::KNIGHT, Color::WHITE);
    squares[H1] = Piece(PieceType::ROOK, Color::WHITE);
    
    for (int i = 0; i < 8; i++) {
        squares[A1 + 8 + i] = Piece(PieceType::PAWN, Color::WHITE);
    }
    
    // Set up black pieces
    squares[A8] = Piece(PieceType::ROOK, Color::BLACK);
    squares[B8] = Piece(PieceType::KNIGHT, Color::BLACK);
    squares[C8] = Piece(PieceType::BISHOP, Color::BLACK);
    squares[D8] = Piece(PieceType::QUEEN, Color::BLACK);
    squares[E8] = Piece(PieceType::KING, Color::BLACK);
    squares[F8] = Piece(PieceType::BISHOP, Color::BLACK);
    squares[G8] = Piece(PieceType::KNIGHT, Color::BLACK);
    squares[H8] = Piece(PieceType::ROOK, Color::BLACK);
    
    for (int i = 0; i < 8; i++) {
        squares[A8 - 8 + i] = Piece(PieceType::PAWN, Color::BLACK);
    }
    
    // Set initial game state
    sideToMove = Color::WHITE;
    canCastleKingSide[0] = canCastleKingSide[1] = true;
    canCastleQueenSide[0] = canCastleQueenSide[1] = true;
    enPassantSquare = 64; // Invalid square
    halfMoveClock = 0;
    fullMoveNumber = 1;
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
    state.capturedPiece = squares[move.to];
    state.castlingRights[0] = canCastleKingSide[0];
    state.castlingRights[1] = canCastleQueenSide[0];
    state.castlingRights[2] = canCastleKingSide[1];
    state.castlingRights[3] = canCastleQueenSide[1];
    state.enPassantSquare = enPassantSquare;
    state.halfMoveClock = halfMoveClock;
    gameHistory.push_back(state);
    
    Piece movingPiece = squares[move.from];
    Piece capturedPiece = squares[move.to];
    
    // Move the piece
    squares[move.to] = movingPiece;
    squares[move.from] = Piece();
    
    // Handle special moves
    if (move.isCastle) {
        // Move the rook for castling
        if (move.to == G1) { // White king-side castle
            squares[F1] = squares[H1];
            squares[H1] = Piece();
        } else if (move.to == C1) { // White queen-side castle
            squares[D1] = squares[A1];
            squares[A1] = Piece();
        } else if (move.to == G8) { // Black king-side castle
            squares[F8] = squares[H8];
            squares[H8] = Piece();
        } else if (move.to == C8) { // Black queen-side castle
            squares[D8] = squares[A8];
            squares[A8] = Piece();
        }
    }
    
    if (move.isEnPassant) {
        // Remove the captured pawn
        Square capturedSquare = sideToMove == Color::WHITE ? move.to - 8 : move.to + 8;
        squares[capturedSquare] = Piece();
    }
    
    if (move.promotion != PieceType::NONE) {
        squares[move.to] = Piece(move.promotion, sideToMove);
    }
    
    // Update game state
    updateCastlingRights(move);
    updateEnPassant(move);
    
    // Update move counters
    if (movingPiece.type == PieceType::PAWN || move.isCapture) {
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
    
    // Switch side to move back
    switchSideToMove();
    
    if (sideToMove == Color::BLACK) {
        fullMoveNumber--;
    }
    
    // Restore pieces
    Piece movingPiece = squares[move.to];
    squares[move.from] = movingPiece;
    squares[move.to] = state.capturedPiece;
    
    // Handle special moves
    if (move.isCastle) {
        // Unmove the rook
        if (move.to == G1) { // White king-side castle
            squares[H1] = squares[F1];
            squares[F1] = Piece();
        } else if (move.to == C1) { // White queen-side castle
            squares[A1] = squares[D1];
            squares[D1] = Piece();
        } else if (move.to == G8) { // Black king-side castle
            squares[H8] = squares[F8];
            squares[F8] = Piece();
        } else if (move.to == C8) { // Black queen-side castle
            squares[A8] = squares[D8];
            squares[D8] = Piece();
        }
    }
    
    if (move.isEnPassant) {
        // Restore the captured pawn
        Square capturedSquare = sideToMove == Color::WHITE ? move.to - 8 : move.to + 8;
        squares[capturedSquare] = Piece(PieceType::PAWN, ~sideToMove);
    }
    
    if (move.promotion != PieceType::NONE) {
        // Restore original pawn
        squares[move.from] = Piece(PieceType::PAWN, sideToMove);
    }
}

void Board::updateCastlingRights(const Move& move) {
    // If king moves, lose all castling rights for that color
    if (squares[move.to].type == PieceType::KING) {
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
    
    // Check for pawn double move
    if (squares[move.to].type == PieceType::PAWN) {
        int fromRank = rankOf(move.from);
        int toRank = rankOf(move.to);
        
        if (abs(toRank - fromRank) == 2) {
            enPassantSquare = (move.from + move.to) / 2;
        }
    }
}

Square Board::findKing(Color color) const {
    for (Square sq = 0; sq < 64; sq++) {
        if (squares[sq].type == PieceType::KING && squares[sq].color == color) {
            return sq;
        }
    }
    return 64; // Invalid square if king not found
}

bool Board::isInCheck(Color color) const {
    Square kingSquare = findKing(color);
    return kingSquare < 64 && isSquareAttacked(kingSquare, ~color);
}

bool Board::isSquareAttacked(Square sq, Color attacker) const {
    int file = fileOf(sq);
    int rank = rankOf(sq);
    
    // Check for pawn attacks
    int pawnDirection = (attacker == Color::WHITE) ? 1 : -1;
    int pawnRank = rank - pawnDirection;
    if (pawnRank >= 0 && pawnRank < 8) {
        for (int pawnFile : {file - 1, file + 1}) {
            if (pawnFile >= 0 && pawnFile < 8) {
                Square pawnSquare = makeSquare(pawnFile, pawnRank);
                const Piece& piece = squares[pawnSquare];
                if (piece.type == PieceType::PAWN && piece.color == attacker) {
                    return true;
                }
            }
        }
    }
    
    // Check for knight attacks
    static const int knightMoves[8][2] = {
        {-2, -1}, {-2, 1}, {-1, -2}, {-1, 2},
        {1, -2}, {1, 2}, {2, -1}, {2, 1}
    };
    
    for (const auto& delta : knightMoves) {
        int newFile = file + delta[0];
        int newRank = rank + delta[1];
        
        if (newFile >= 0 && newFile < 8 && newRank >= 0 && newRank < 8) {
            Square knightSquare = makeSquare(newFile, newRank);
            const Piece& piece = squares[knightSquare];
            if (piece.type == PieceType::KNIGHT && piece.color == attacker) {
                return true;
            }
        }
    }
    
    // Check for bishop/queen diagonal attacks
    static const int diagonalDirections[4][2] = {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}};
    for (const auto& dir : diagonalDirections) {
        for (int i = 1; i < 8; i++) {
            int newFile = file + i * dir[0];
            int newRank = rank + i * dir[1];
            
            if (newFile < 0 || newFile >= 8 || newRank < 0 || newRank >= 8) break;
            
            Square checkSquare = makeSquare(newFile, newRank);
            const Piece& piece = squares[checkSquare];
            
            if (!piece.isEmpty()) {
                if (piece.color == attacker && 
                    (piece.type == PieceType::BISHOP || piece.type == PieceType::QUEEN)) {
                    return true;
                }
                break; // Blocked
            }
        }
    }
    
    // Check for rook/queen straight attacks
    static const int straightDirections[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    for (const auto& dir : straightDirections) {
        for (int i = 1; i < 8; i++) {
            int newFile = file + i * dir[0];
            int newRank = rank + i * dir[1];
            
            if (newFile < 0 || newFile >= 8 || newRank < 0 || newRank >= 8) break;
            
            Square checkSquare = makeSquare(newFile, newRank);
            const Piece& piece = squares[checkSquare];
            
            if (!piece.isEmpty()) {
                if (piece.color == attacker && 
                    (piece.type == PieceType::ROOK || piece.type == PieceType::QUEEN)) {
                    return true;
                }
                break; // Blocked
            }
        }
    }
    
    // Check for king attacks
    static const int kingMoves[8][2] = {
        {-1, -1}, {-1, 0}, {-1, 1},
        {0, -1},           {0, 1},
        {1, -1},  {1, 0},  {1, 1}
    };
    
    for (const auto& delta : kingMoves) {
        int newFile = file + delta[0];
        int newRank = rank + delta[1];
        
        if (newFile >= 0 && newFile < 8 && newRank >= 0 && newRank < 8) {
            Square kingSquare = makeSquare(newFile, newRank);
            const Piece& piece = squares[kingSquare];
            if (piece.type == PieceType::KING && piece.color == attacker) {
                return true;
            }
        }
    }
    
    return false;
}

std::string Board::toString() const {
    std::ostringstream oss;
    
    for (int rank = 7; rank >= 0; rank--) {
        oss << (rank + 1) << " ";
        for (int file = 0; file < 8; file++) {
            Square sq = makeSquare(file, rank);
            const Piece& piece = squares[sq];
            
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