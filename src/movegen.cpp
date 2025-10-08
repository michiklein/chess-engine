#include "movegen.h"
#include <algorithm>
#include <iostream>

std::vector<Move> MoveGenerator::generateLegalMoves(const Board& board) {
    std::vector<Move> pseudoLegalMoves = generatePseudoLegalMoves(board);
    std::vector<Move> legalMoves;
    
    for (const Move& move : pseudoLegalMoves) {
        if (isLegalMove(board, move)) {
            legalMoves.push_back(move);
        }
    }
    
    return legalMoves;
}

std::vector<Move> MoveGenerator::generatePseudoLegalMoves(const Board& board) {
    std::vector<Move> moves;
    Color sideToMove = board.getSideToMove();
    
    // Generate moves for all pieces of the current side using bitboards
    for (int pieceType = 0; pieceType < 6; pieceType++) {
        int pieceIndex = pieceType + (static_cast<int>(sideToMove) * 6);
        Bitboard pieceBitboard = board.getPieceBitboard(static_cast<PieceType>(pieceType), sideToMove);
        
        // Iterate through all pieces of this type
        while (pieceBitboard != EMPTY_BOARD) {
            Square sq = firstSquare(pieceBitboard);
            pieceBitboard = clearBit(pieceBitboard, sq);
            
            switch (static_cast<PieceType>(pieceType)) {
                case PieceType::PAWN:
                    generatePawnMoves(board, sq, moves);
                    break;
                case PieceType::KNIGHT:
                    generateKnightMoves(board, sq, moves);
                    break;
                case PieceType::BISHOP:
                    generateBishopMoves(board, sq, moves);
                    break;
                case PieceType::ROOK:
                    generateRookMoves(board, sq, moves);
                    break;
                case PieceType::QUEEN:
                    generateQueenMoves(board, sq, moves);
                    break;
                case PieceType::KING:
                    generateKingMoves(board, sq, moves);
                    break;
                default:
                    break;
            }
        }
    }
    
    // Generate castling moves
    generateCastlingMoves(board, moves);
    
    return moves;
}

bool MoveGenerator::isLegalMove(const Board& board, const Move& move) {
    // Make a copy of the board and try the move
    Board testBoard = board;
    Color originalSide = board.getSideToMove();
    testBoard.makeMove(move);
    
    // Check if the move leaves the original side's king in check
    bool isLegal = !testBoard.isInCheck(originalSide);
    
    
    return isLegal;
}

void MoveGenerator::generatePawnMoves(const Board& board, Square sq, std::vector<Move>& moves) {
    Color color = board.pieceAt(sq).color;
    int file = fileOf(sq);
    int rank = rankOf(sq);
    
    if (color == Color::WHITE) {
        // Forward moves
        if (rank < 7) {
            Square forward = makeSquare(file, rank + 1);
            if (board.pieceAt(forward).isEmpty()) {
                // Single forward move
                Move move(sq, forward);
                if (rank == 6) {
                    // Promotion
                    for (PieceType promo : {PieceType::QUEEN, PieceType::ROOK, PieceType::BISHOP, PieceType::KNIGHT}) {
                        move.promotion = promo;
                        moves.push_back(move);
                    }
                } else {
                    moves.push_back(move);
                }
                
                // Double forward move from starting position
                if (rank == 1) {
                    Square doubleForward = makeSquare(file, rank + 2);
                    if (board.pieceAt(doubleForward).isEmpty()) {
                        moves.push_back(Move(sq, doubleForward));
                    }
                }
            }
        }
        
        // Captures
        for (int df : {-1, 1}) {
            int newFile = file + df;
            if (newFile >= 0 && newFile < 8 && rank < 7) {
                Square captureSquare = makeSquare(newFile, rank + 1);
                const Piece& targetPiece = board.pieceAt(captureSquare);
                
                if (!targetPiece.isEmpty() && targetPiece.color == Color::BLACK) {
                    Move move(sq, captureSquare);
                    move.isCapture = true;
                    if (rank == 6) {
                        // Promotion capture
                        for (PieceType promo : {PieceType::QUEEN, PieceType::ROOK, PieceType::BISHOP, PieceType::KNIGHT}) {
                            move.promotion = promo;
                            moves.push_back(move);
                        }
                    } else {
                        moves.push_back(move);
                    }
                }
                
                // En passant
                if (captureSquare == board.getEnPassantSquare()) {
                    Move move(sq, captureSquare);
                    move.isEnPassant = true;
                    move.isCapture = true;
                    moves.push_back(move);
                }
            }
        }
    } else {
        // Black pawn moves (similar logic, but moving down)
        if (rank > 0) {
            Square forward = makeSquare(file, rank - 1);
            if (board.pieceAt(forward).isEmpty()) {
                // Single forward move
                Move move(sq, forward);
                if (rank == 1) {
                    // Promotion
                    for (PieceType promo : {PieceType::QUEEN, PieceType::ROOK, PieceType::BISHOP, PieceType::KNIGHT}) {
                        move.promotion = promo;
                        moves.push_back(move);
                    }
                } else {
                    moves.push_back(move);
                }
                
                // Double forward move from starting position
                if (rank == 6) {
                    Square doubleForward = makeSquare(file, rank - 2);
                    if (board.pieceAt(doubleForward).isEmpty()) {
                        moves.push_back(Move(sq, doubleForward));
                    }
                }
            }
        }
        
        // Captures
        for (int df : {-1, 1}) {
            int newFile = file + df;
            if (newFile >= 0 && newFile < 8 && rank > 0) {
                Square captureSquare = makeSquare(newFile, rank - 1);
                const Piece& targetPiece = board.pieceAt(captureSquare);
                
                if (!targetPiece.isEmpty() && targetPiece.color == Color::WHITE) {
                    Move move(sq, captureSquare);
                    move.isCapture = true;
                    if (rank == 1) {
                        // Promotion capture
                        for (PieceType promo : {PieceType::QUEEN, PieceType::ROOK, PieceType::BISHOP, PieceType::KNIGHT}) {
                            move.promotion = promo;
                            moves.push_back(move);
                        }
                    } else {
                        moves.push_back(move);
                    }
                }
                
                // En passant
                if (captureSquare == board.getEnPassantSquare()) {
                    Move move(sq, captureSquare);
                    move.isEnPassant = true;
                    move.isCapture = true;
                    moves.push_back(move);
                }
            }
        }
    }
}

void MoveGenerator::generateKnightMoves(const Board& board, Square sq, std::vector<Move>& moves) {
    Color color = board.pieceAt(sq).color;
    Bitboard knightAttacks = getKnightAttacks(sq);
    Bitboard enemyPieces = (color == Color::WHITE) ? board.getBlackPieces() : board.getWhitePieces();
    Bitboard friendlyPieces = (color == Color::WHITE) ? board.getWhitePieces() : board.getBlackPieces();
    
    // Remove friendly pieces from attack squares
    knightAttacks &= ~friendlyPieces;
    
    // Generate moves
    while (knightAttacks != EMPTY_BOARD) {
        Square to = firstSquare(knightAttacks);
        knightAttacks = clearBit(knightAttacks, to);
        
        Move move(sq, to);
        if (getBit(enemyPieces, to)) {
            move.isCapture = true;
        }
        moves.push_back(move);
    }
}

void MoveGenerator::generateBishopMoves(const Board& board, Square sq, std::vector<Move>& moves) {
    Color color = board.pieceAt(sq).color;
    Bitboard bishopAttacks = getBishopAttacks(sq, board.getAllPieces());
    Bitboard enemyPieces = (color == Color::WHITE) ? board.getBlackPieces() : board.getWhitePieces();
    Bitboard friendlyPieces = (color == Color::WHITE) ? board.getWhitePieces() : board.getBlackPieces();
    
    // Remove friendly pieces from attack squares
    bishopAttacks &= ~friendlyPieces;
    
    // Generate moves
    while (bishopAttacks != EMPTY_BOARD) {
        Square to = firstSquare(bishopAttacks);
        bishopAttacks = clearBit(bishopAttacks, to);
        
        Move move(sq, to);
        if (getBit(enemyPieces, to)) {
            move.isCapture = true;
        }
        moves.push_back(move);
    }
}

void MoveGenerator::generateRookMoves(const Board& board, Square sq, std::vector<Move>& moves) {
    Color color = board.pieceAt(sq).color;
    Bitboard rookAttacks = getRookAttacks(sq, board.getAllPieces());
    Bitboard enemyPieces = (color == Color::WHITE) ? board.getBlackPieces() : board.getWhitePieces();
    Bitboard friendlyPieces = (color == Color::WHITE) ? board.getWhitePieces() : board.getBlackPieces();
    
    // Remove friendly pieces from attack squares
    rookAttacks &= ~friendlyPieces;
    
    // Generate moves
    while (rookAttacks != EMPTY_BOARD) {
        Square to = firstSquare(rookAttacks);
        rookAttacks = clearBit(rookAttacks, to);
        
        Move move(sq, to);
        if (getBit(enemyPieces, to)) {
            move.isCapture = true;
        }
        moves.push_back(move);
    }
}

void MoveGenerator::generateQueenMoves(const Board& board, Square sq, std::vector<Move>& moves) {
    Color color = board.pieceAt(sq).color;
    Bitboard queenAttacks = getQueenAttacks(sq, board.getAllPieces());
    Bitboard enemyPieces = (color == Color::WHITE) ? board.getBlackPieces() : board.getWhitePieces();
    Bitboard friendlyPieces = (color == Color::WHITE) ? board.getWhitePieces() : board.getBlackPieces();
    
    // Remove friendly pieces from attack squares
    queenAttacks &= ~friendlyPieces;
    
    // Generate moves
    while (queenAttacks != EMPTY_BOARD) {
        Square to = firstSquare(queenAttacks);
        queenAttacks = clearBit(queenAttacks, to);
        
        Move move(sq, to);
        if (getBit(enemyPieces, to)) {
            move.isCapture = true;
        }
        moves.push_back(move);
    }
}

void MoveGenerator::generateKingMoves(const Board& board, Square sq, std::vector<Move>& moves) {
    Color color = board.pieceAt(sq).color;
    Bitboard kingAttacks = getKingAttacks(sq);
    Bitboard enemyPieces = (color == Color::WHITE) ? board.getBlackPieces() : board.getWhitePieces();
    Bitboard friendlyPieces = (color == Color::WHITE) ? board.getWhitePieces() : board.getBlackPieces();
    
    // Remove friendly pieces from attack squares
    kingAttacks &= ~friendlyPieces;
    
    // Generate moves
    while (kingAttacks != EMPTY_BOARD) {
        Square to = firstSquare(kingAttacks);
        kingAttacks = clearBit(kingAttacks, to);
        
        Move move(sq, to);
        if (getBit(enemyPieces, to)) {
            move.isCapture = true;
        }
        moves.push_back(move);
    }
}

void MoveGenerator::generateCastlingMoves(const Board& board, std::vector<Move>& moves) {
    Color color = board.getSideToMove();
    
    if (board.isInCheck(color)) return; // Cannot castle in check
    
    // King-side castling
    if (board.canCastle(color, true)) {
        Square kingSquare = (color == Color::WHITE) ? E1 : E8;
        Square kingTarget = (color == Color::WHITE) ? G1 : G8;
        Square rookTarget = (color == Color::WHITE) ? F1 : F8;
        
        // Check if squares are empty and not attacked
        if (board.pieceAt(rookTarget).isEmpty() && board.pieceAt(kingTarget).isEmpty() &&
            !board.isSquareAttacked(rookTarget, ~color) && !board.isSquareAttacked(kingTarget, ~color)) {
            Move move(kingSquare, kingTarget);
            move.isCastle = true;
            moves.push_back(move);
        }
    }
    
    // Queen-side castling
    if (board.canCastle(color, false)) {
        Square kingSquare = (color == Color::WHITE) ? E1 : E8;
        Square kingTarget = (color == Color::WHITE) ? C1 : C8;
        Square rookTarget = (color == Color::WHITE) ? D1 : D8;
        Square extraSquare = (color == Color::WHITE) ? B1 : B8;
        
        // Check if squares are empty and not attacked
        if (board.pieceAt(rookTarget).isEmpty() && board.pieceAt(kingTarget).isEmpty() &&
            board.pieceAt(extraSquare).isEmpty() &&
            !board.isSquareAttacked(rookTarget, ~color) && !board.isSquareAttacked(kingTarget, ~color)) {
            Move move(kingSquare, kingTarget);
            move.isCastle = true;
            moves.push_back(move);
        }
    }
}

// Bitboard attack generation functions
Bitboard MoveGenerator::getPawnAttacks(Square sq, Color color) {
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

Bitboard MoveGenerator::getKnightAttacks(Square sq) {
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

Bitboard MoveGenerator::getBishopAttacks(Square sq, Bitboard occupied) {
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

Bitboard MoveGenerator::getRookAttacks(Square sq, Bitboard occupied) {
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

Bitboard MoveGenerator::getQueenAttacks(Square sq, Bitboard occupied) {
    return getBishopAttacks(sq, occupied) | getRookAttacks(sq, occupied);
}

Bitboard MoveGenerator::getKingAttacks(Square sq) {
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

bool MoveGenerator::isSquareOnBoard(int file, int rank) {
    return file >= 0 && file < 8 && rank >= 0 && rank < 8;
}

bool MoveGenerator::canMoveTo(const Board& board, Square from, Square to, Color movingColor) {
    const Piece& target = board.pieceAt(to);
    return target.isEmpty() || target.color != movingColor;
}
