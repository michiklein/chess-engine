#ifndef SEARCH_H
#define SEARCH_H

#include "types.h"
#include "board.h"
#include "opening_book.h"
#include <limits>
#include <vector>

// Forward declaration
class Board;

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
    void setQuietMode(bool quiet) { quietMode = quiet; }
    
    // Opening book
    bool loadOpeningBook(const std::string& filename);
    
    // Helper functions (public for inheritance)
    int getPieceValue(PieceType type);
    
    
private:
    int maxDepth;
    int timeLimit;
    int nodesSearched;
    int currentDepth;
    bool quietMode;
    
    // Opening book
    OpeningBook openingBook;
    bool useOpeningBook;
    
    // Killer moves table (moves that caused beta cutoffs)
    static const int MAX_KILLER_MOVES = 2;
    Move killerMoves[32][MAX_KILLER_MOVES]; // [depth][killer_index]
    
    // History heuristic table (moves that have been good)
    int historyTable[64][64]; // [from_square][to_square]
    
    // Search algorithms
    int minimax(Board& board, int depth, bool maximizing);
    int alphaBeta(Board& board, int depth, int alpha, int beta, bool maximizing);
    
    // Evaluation function
    int evaluate(const Board& board);
    
    int getPositionalValue(PieceType type, Square square, Color color);
    void orderMoves(const Board& board, std::vector<Move>& moves);
    
    // Move ordering helpers
    bool isKillerMove(const Move& move, int depth);
    int getHistoryScore(const Move& move);
    void recordKillerMove(const Move& move, int depth);
    void recordHistoryMove(const Move& move, int depth);
    bool isMoveInOpeningBook(const Board& board, const Move& move);
    bool isSafeCapture(const Board& board, const Move& move);
    
    // Enhanced evaluation functions
    int getMobilityValue(const Board& board, Square sq, const Piece& piece);
    int evaluateKingSafety(const Board& board, Color color);
    int evaluatePawnStructure(const Board& board, Color color);
    int evaluateCenterControl(const Board& board, Color color);
    int evaluateDevelopment(const Board& board, Color color);
    int evaluateTactics(const Board& board);
    int evaluateKingAttack(const Board& board, Color color);
    int evaluateCaptures(const Board& board);
    int evaluateHungPieces(const Board& board);
    
    // New enhanced evaluation functions
    int evaluateMaterial(const Board& board);
    int evaluateMobility(const Board& board);
    int evaluateKingSafety(const Board& board);
};

// Constants for evaluation
constexpr int MATE_SCORE = 10000;
constexpr int DRAW_SCORE = 0;


#endif // SEARCH_H