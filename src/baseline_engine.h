#ifndef BASELINE_ENGINE_H
#define BASELINE_ENGINE_H

#include "search.h"

class BaselineEngine : public SearchEngine {
public:
    BaselineEngine() : SearchEngine() {}
    
    // Override the evaluate function to use only material
    int evaluate(const Board& board) {
        int score = 0;
        
        // Check for immediate checkmate (highest priority)
        if (board.isCheckmate()) {
            Color sideToMove = board.getSideToMove();
            return (sideToMove == Color::WHITE) ? -MATE_SCORE : MATE_SCORE;
        }
        
        // Simple material evaluation only (baseline)
        for (Square sq = 0; sq < 64; sq++) {
            const Piece& piece = board.pieceAt(sq);
            if (!piece.isEmpty()) {
                int pieceValue = getPieceValue(piece.type);
                
                if (piece.color == Color::WHITE) {
                    score += pieceValue;
                } else {
                    score -= pieceValue;
                }
            }
        }
        
        return score;
    }
};

#endif // BASELINE_ENGINE_H
