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
    // Simplified positional evaluation
    // In a real engine, this would use piece-square tables
    
    int file = fileOf(square);
    int rank = rankOf(square);
    
    // Adjust rank for black pieces
    if (color == Color::BLACK) {
        rank = 7 - rank;
    }
    
    switch (type) {
        case PieceType::PAWN:
            // Encourage pawn advancement
            return rank * 10;
            
        case PieceType::KNIGHT:
            // Knights are better in the center
            return std::max(0, 4 - std::max(abs(file - 3.5), abs(rank - 3.5))) * 5;
            
        case PieceType::BISHOP:
            // Bishops are better in the center
            return std::max(0, 4 - std::max(abs(file - 3.5), abs(rank - 3.5))) * 3;
            
        case PieceType::KING:
            // In opening/middlegame, king should stay safe
            if (rank == 0) return 10; // Bonus for staying on back rank
            return -abs(rank) * 10;   // Penalty for king advancement
            
        default:
            return 0;
    }
}