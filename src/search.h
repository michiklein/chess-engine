#ifndef SEARCH_H
#define SEARCH_H

#include "types.h"
#include "board.h"
#include <limits>
#include <vector>

struct SearchResult {
    Move bestMove;
    int score;
    int depth;
    int nodesSearched;
    
    SearchResult() : bestMove(), score(0), depth(0), nodesSearched(0) {}
};

class SearchEngine {
public:
    SearchEngine();
    
    // Main search function
    SearchResult search(const Board& board, int depth);
    
    // Set search parameters
    void setMaxDepth(int depth) { maxDepth = depth; }
    void setTimeLimit(int milliseconds) { timeLimit = milliseconds; }
    
private:
    int maxDepth;
    int timeLimit;
    int nodesSearched;
    
    // Search algorithms
    int minimax(Board& board, int depth, bool maximizing);
    int alphaBeta(Board& board, int depth, int alpha, int beta, bool maximizing);
    
    // Evaluation function
    int evaluate(const Board& board);
    
    // Helper functions
    int getPieceValue(PieceType type);
    int getPositionalValue(PieceType type, Square square, Color color);
    void orderMoves(const Board& board, std::vector<Move>& moves);
};

// Constants for evaluation
constexpr int MATE_SCORE = 10000;
constexpr int DRAW_SCORE = 0;

#endif // SEARCH_H