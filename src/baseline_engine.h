#ifndef BASELINE_ENGINE_H
#define BASELINE_ENGINE_H

#include "search.h"

// Material-only evaluation, used by the tournament to benchmark against the full eval.
class BaselineEngine : public SearchEngine {
public:
    BaselineEngine() : SearchEngine() {}

protected:
    int evaluate(const Board& board) override {
        int score = 0;
        for (Square sq = 0; sq < 64; sq++) {
            const Piece& piece = board.pieceAt(sq);
            if (!piece.isEmpty()) {
                int v = getPieceValue(piece.type);
                score += (piece.color == Color::WHITE) ? v : -v;
            }
        }
        return score;
    }
};

#endif // BASELINE_ENGINE_H
