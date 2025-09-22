#include "search.h"
#include "movegen.h"
#include <algorithm>

SearchEngine::SearchEngine() : maxDepth(4), timeLimit(5000), nodesSearched(0) {}

SearchResult SearchEngine::search(const Board& board, int depth) {
    SearchResult result;
    nodesSearched = 0;
    
    std::vector<Move> moves = MoveGenerator::generateLegalMoves(board);
    
    if (moves.empty()) {
        result.score = board.isInCheck(board.getSideToMove()) ? -MATE_SCORE : DRAW_SCORE;
        return result;
    }
    
    // Order moves for better search efficiency
    orderMoves(board, moves);
    
    int bestScore = std::numeric_limits<int>::min();
    Move bestMove = moves[0];
    
    for (const Move& move : moves) {
        Board testBoard = board;
        testBoard.makeMove(move);
        
        int score = -alphaBeta(testBoard, depth - 1, 
                              std::numeric_limits<int>::min(), 
                              std::numeric_limits<int>::max(), 
                              false);
        
        if (score > bestScore) {
            bestScore = score;
            bestMove = move;
        }
    }
    
    result.bestMove = bestMove;
    result.score = bestScore;
    result.depth = depth;
    result.nodesSearched = nodesSearched;
    
    return result;
}

int SearchEngine::alphaBeta(Board& board, int depth, int alpha, int beta, bool maximizing) {
    nodesSearched++;
    
    if (depth == 0) {
        return evaluate(board);
    }
    
    std::vector<Move> moves = MoveGenerator::generateLegalMoves(board);
    
    if (moves.empty()) {
        return board.isInCheck(board.getSideToMove()) ? -MATE_SCORE : DRAW_SCORE;
    }
    
    // Order moves for better search efficiency
    orderMoves(board, moves);
    
    if (maximizing) {
        int maxEval = std::numeric_limits<int>::min();
        for (const Move& move : moves) {
            board.makeMove(move);
            int eval = alphaBeta(board, depth - 1, alpha, beta, false);
            board.unmakeMove(move);
            
            maxEval = std::max(maxEval, eval);
            alpha = std::max(alpha, eval);
            
            if (beta <= alpha) break; // Beta cutoff
        }
        return maxEval;
    } else {
        int minEval = std::numeric_limits<int>::max();
        for (const Move& move : moves) {
            board.makeMove(move);
            int eval = alphaBeta(board, depth - 1, alpha, beta, true);
            board.unmakeMove(move);
            
            minEval = std::min(minEval, eval);
            beta = std::min(beta, eval);
            
            if (beta <= alpha) break; // Alpha cutoff
        }
        return minEval;
    }
}

int SearchEngine::evaluate(const Board& board) {
    int score = 0;
    
    // Material evaluation
    for (Square sq = 0; sq < 64; sq++) {
        const Piece& piece = board.pieceAt(sq);
        if (!piece.isEmpty()) {
            int pieceValue = getPieceValue(piece.type);
            int positionalValue = getPositionalValue(piece.type, sq, piece.color);
            
            if (piece.color == Color::WHITE) {
                score += pieceValue + positionalValue;
            } else {
                score -= pieceValue + positionalValue;
            }
        }
    }
    
    // Adjust score based on side to move
    return board.getSideToMove() == Color::WHITE ? score : -score;
}

int SearchEngine::getPieceValue(PieceType type) {
    switch (type) {
        case PieceType::PAWN:   return 100;
        case PieceType::KNIGHT: return 300;
        case PieceType::BISHOP: return 300;
        case PieceType::ROOK:   return 500;
        case PieceType::QUEEN:  return 900;
        case PieceType::KING:   return 10000;
        default: return 0;
    }
}

int SearchEngine::getPositionalValue(PieceType type, Square square, Color color) {
    int file = fileOf(square);
    int rank = rankOf(square);
    
    // Adjust rank for black pieces (flip the board)
    if (color == Color::BLACK) {
        rank = 7 - rank;
    }
    
    // Distance from center (center is files 3,4 and ranks 3,4)
    int centerDistance = std::max(abs(file - 3), abs(file - 4)) + std::max(abs(rank - 3), abs(rank - 4));
    
    switch (type) {
        case PieceType::PAWN: {
            // Pawn structure bonuses
            int pawnValue = 0;
            
            // Center pawns are more valuable
            if (file >= 3 && file <= 4) pawnValue += 10;
            
            // Pawn advancement (but not too far)
            if (rank >= 2 && rank <= 5) pawnValue += rank * 5;
            else if (rank > 5) pawnValue += 20; // Advanced pawns
            
            // Avoid moving pawns too early in opening
            if (rank == 1) pawnValue -= 5; // Slight penalty for moving from starting position too early
            
            return pawnValue;
        }
            
        case PieceType::KNIGHT: {
            // Knights are excellent in the center
            int knightValue = 0;
            if (centerDistance <= 1) knightValue += 20; // Center
            else if (centerDistance == 2) knightValue += 10; // Near center
            else if (centerDistance >= 4) knightValue -= 10; // Edge
            
            // Avoid corners
            if ((file == 0 || file == 7) && (rank == 0 || rank == 7)) knightValue -= 15;
            
            return knightValue;
        }
            
        case PieceType::BISHOP: {
            // Bishops like long diagonals and center
            int bishopValue = 0;
            if (centerDistance <= 1) bishopValue += 15;
            else if (centerDistance == 2) bishopValue += 8;
            
            // Bishops on long diagonals are good
            if (file == rank || file == 7 - rank) bishopValue += 5;
            
            return bishopValue;
        }
            
        case PieceType::ROOK: {
            // Rooks like open files and ranks
            int rookValue = 0;
            
            // Rooks are better on open files (simplified - assume file is open)
            if (file >= 2 && file <= 5) rookValue += 5;
            
            // Rooks on 7th rank are powerful
            if (rank == 6) rookValue += 15;
            
            return rookValue;
        }
            
        case PieceType::QUEEN: {
            // Queen likes center but not too early
            int queenValue = 0;
            if (centerDistance <= 2) queenValue += 5;
            
            // Don't bring queen out too early
            if (rank <= 1) queenValue -= 10;
            
            return queenValue;
        }
            
        case PieceType::KING: {
            // King safety - stay back in opening/middlegame
            int kingValue = 0;
            if (rank == 0) kingValue += 20; // Back rank is safe
            else if (rank == 1) kingValue += 10;
            else if (rank >= 3) kingValue -= 15; // Too far forward
            
            // King likes corners in opening
            if (file <= 1 || file >= 6) kingValue += 5;
            
            return kingValue;
        }
            
        default:
            return 0;
    }
}

void SearchEngine::orderMoves(const Board& board, std::vector<Move>& moves) {
    // Simple move ordering: captures first, then other moves
    std::sort(moves.begin(), moves.end(), [&](const Move& a, const Move& b) {
        // Captures first
        bool aCapture = a.isCapture;
        bool bCapture = b.isCapture;
        
        if (aCapture && !bCapture) return true;
        if (!aCapture && bCapture) return false;
        
        // If both captures or both non-captures, order by piece value
        if (aCapture && bCapture) {
            // Higher value captures first
            int aValue = getPieceValue(board.pieceAt(a.to).type);
            int bValue = getPieceValue(board.pieceAt(b.to).type);
            return aValue > bValue;
        }
        
        // For non-captures, prefer center moves
        int aCenterDist = std::max(abs(fileOf(a.to) - 3), abs(fileOf(a.to) - 4)) + 
                         std::max(abs(rankOf(a.to) - 3), abs(rankOf(a.to) - 4));
        int bCenterDist = std::max(abs(fileOf(b.to) - 3), abs(fileOf(b.to) - 4)) + 
                         std::max(abs(rankOf(b.to) - 3), abs(rankOf(b.to) - 4));
        
        return aCenterDist < bCenterDist; // Closer to center first
    });
}