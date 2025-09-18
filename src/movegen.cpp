#include "movegen.h"

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
    
    // Generate moves for all pieces of the current side
    for (Square sq = 0; sq < 64; sq++) {
        const Piece& piece = board.pieceAt(sq);
        if (piece.isEmpty() || piece.color != sideToMove) {
            continue;
        }
        
        switch (piece.type) {
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
    
    // Generate castling moves
    generateCastlingMoves(board, moves);
    
    return moves;
}

bool MoveGenerator::isLegalMove(const Board& board, const Move& move) {
    // Make a copy of the board and try the move
    Board testBoard = board;
    testBoard.makeMove(move);
    
    // Check if the move leaves the king in check
    return !testBoard.isInCheck(board.getSideToMove());
}

void MoveGenerator::generatePawnMoves(const Board& board, Square sq, std::vector<Move>& moves) {
    Color color = board.pieceAt(sq).color;
    int direction = (color == Color::WHITE) ? 8 : -8;
    int startRank = (color == Color::WHITE) ? 1 : 6;
    int promotionRank = (color == Color::WHITE) ? 7 : 0;
    
    int file = fileOf(sq);
    int rank = rankOf(sq);
    
    // Forward move
    Square forward = sq + direction;
    if (forward < 64 && board.pieceAt(forward).isEmpty()) {
        if (rankOf(forward) == promotionRank) {
            // Promotion moves
            moves.emplace_back(sq, forward);
            moves.back().promotion = PieceType::QUEEN;
            moves.emplace_back(sq, forward);
            moves.back().promotion = PieceType::ROOK;
            moves.emplace_back(sq, forward);
            moves.back().promotion = PieceType::BISHOP;
            moves.emplace_back(sq, forward);
            moves.back().promotion = PieceType::KNIGHT;
        } else {
            moves.emplace_back(sq, forward);
        }
        
        // Double forward move from starting position
        if (rank == startRank) {
            Square doubleForward = sq + 2 * direction;
            if (doubleForward < 64 && board.pieceAt(doubleForward).isEmpty()) {
                moves.emplace_back(sq, doubleForward);
            }
        }
    }
    
    // Capture moves
    for (int captureFile : {file - 1, file + 1}) {
        if (captureFile >= 0 && captureFile < 8) {
            Square captureSquare = makeSquare(captureFile, rank + direction / 8);
            if (captureSquare < 64) {
                const Piece& target = board.pieceAt(captureSquare);
                if (!target.isEmpty() && target.color != color) {
                    Move move(sq, captureSquare);
                    move.isCapture = true;
                    if (rankOf(captureSquare) == promotionRank) {
                        // Promotion captures
                        move.promotion = PieceType::QUEEN;
                        moves.push_back(move);
                        move.promotion = PieceType::ROOK;
                        moves.push_back(move);
                        move.promotion = PieceType::BISHOP;
                        moves.push_back(move);
                        move.promotion = PieceType::KNIGHT;
                        moves.push_back(move);
                    } else {
                        moves.push_back(move);
                    }
                }
                
                // En passant capture
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
    static const int knightMoves[8][2] = {
        {-2, -1}, {-2, 1}, {-1, -2}, {-1, 2},
        {1, -2}, {1, 2}, {2, -1}, {2, 1}
    };
    
    Color color = board.pieceAt(sq).color;
    int file = fileOf(sq);
    int rank = rankOf(sq);
    
    for (const auto& delta : knightMoves) {
        int newFile = file + delta[0];
        int newRank = rank + delta[1];
        
        if (isSquareOnBoard(newFile, newRank)) {
            Square targetSquare = makeSquare(newFile, newRank);
            if (canMoveTo(board, sq, targetSquare, color)) {
                Move move(sq, targetSquare);
                move.isCapture = !board.pieceAt(targetSquare).isEmpty();
                moves.push_back(move);
            }
        }
    }
}

void MoveGenerator::generateBishopMoves(const Board& board, Square sq, std::vector<Move>& moves) {
    static const int directions[4][2] = {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}};
    
    Color color = board.pieceAt(sq).color;
    int file = fileOf(sq);
    int rank = rankOf(sq);
    
    for (const auto& dir : directions) {
        for (int i = 1; i < 8; i++) {
            int newFile = file + i * dir[0];
            int newRank = rank + i * dir[1];
            
            if (!isSquareOnBoard(newFile, newRank)) break;
            
            Square targetSquare = makeSquare(newFile, newRank);
            const Piece& target = board.pieceAt(targetSquare);
            
            if (target.isEmpty()) {
                moves.emplace_back(sq, targetSquare);
            } else {
                if (target.color != color) {
                    Move move(sq, targetSquare);
                    move.isCapture = true;
                    moves.push_back(move);
                }
                break; // Blocked
            }
        }
    }
}

void MoveGenerator::generateRookMoves(const Board& board, Square sq, std::vector<Move>& moves) {
    static const int directions[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    
    Color color = board.pieceAt(sq).color;
    int file = fileOf(sq);
    int rank = rankOf(sq);
    
    for (const auto& dir : directions) {
        for (int i = 1; i < 8; i++) {
            int newFile = file + i * dir[0];
            int newRank = rank + i * dir[1];
            
            if (!isSquareOnBoard(newFile, newRank)) break;
            
            Square targetSquare = makeSquare(newFile, newRank);
            const Piece& target = board.pieceAt(targetSquare);
            
            if (target.isEmpty()) {
                moves.emplace_back(sq, targetSquare);
            } else {
                if (target.color != color) {
                    Move move(sq, targetSquare);
                    move.isCapture = true;
                    moves.push_back(move);
                }
                break; // Blocked
            }
        }
    }
}

void MoveGenerator::generateQueenMoves(const Board& board, Square sq, std::vector<Move>& moves) {
    generateBishopMoves(board, sq, moves);
    generateRookMoves(board, sq, moves);
}

void MoveGenerator::generateKingMoves(const Board& board, Square sq, std::vector<Move>& moves) {
    static const int kingMoves[8][2] = {
        {-1, -1}, {-1, 0}, {-1, 1},
        {0, -1},           {0, 1},
        {1, -1},  {1, 0},  {1, 1}
    };
    
    Color color = board.pieceAt(sq).color;
    int file = fileOf(sq);
    int rank = rankOf(sq);
    
    for (const auto& delta : kingMoves) {
        int newFile = file + delta[0];
        int newRank = rank + delta[1];
        
        if (isSquareOnBoard(newFile, newRank)) {
            Square targetSquare = makeSquare(newFile, newRank);
            if (canMoveTo(board, sq, targetSquare, color)) {
                Move move(sq, targetSquare);
                move.isCapture = !board.pieceAt(targetSquare).isEmpty();
                moves.push_back(move);
            }
        }
    }
}

void MoveGenerator::generateCastlingMoves(const Board& board, std::vector<Move>& moves) {
    Color color = board.getSideToMove();
    
    if (board.isInCheck(color)) return; // Cannot castle in check
    
    // King-side castling
    if (board.canCastle(color, true)) {
        Square kingSquare = (color == Color::WHITE) ? E1 : E8;
        Square rookSquare = (color == Color::WHITE) ? H1 : H8;
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
        Square rookSquare = (color == Color::WHITE) ? A1 : A8;
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

bool MoveGenerator::isSquareOnBoard(int file, int rank) {
    return file >= 0 && file < 8 && rank >= 0 && rank < 8;
}

bool MoveGenerator::canMoveTo(const Board& board, Square from, Square to, Color movingColor) {
    const Piece& target = board.pieceAt(to);
    return target.isEmpty() || target.color != movingColor;
}