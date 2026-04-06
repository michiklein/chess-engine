#ifndef SEARCH_H
#define SEARCH_H

#include "types.h"
#include "board.h"
#include "opening_book.h"
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

    SearchResult search(const Board& board, int depth);

    void setMaxDepth(int depth) { maxDepth = depth; }
    void setTimeLimit(int milliseconds) { timeLimit = milliseconds; }
    void setQuietMode(bool quiet) { quietMode = quiet; }

    bool loadOpeningBook(const std::string& filename);

    int getPieceValue(PieceType type);

protected:
    virtual int evaluate(const Board& board);

private:
    int maxDepth;
    int timeLimit;
    int nodesSearched;
    int currentDepth;
    bool quietMode;

    OpeningBook openingBook;
    bool useOpeningBook;

    static const int MAX_KILLER_MOVES = 2;
    Move killerMoves[32][MAX_KILLER_MOVES];
    int historyTable[64][64];

    int alphaBeta(Board& board, int depth, int alpha, int beta, bool maximizing);
    int quiescence(Board& board, int alpha, int beta, bool maximizing);

    int getPositionalValue(PieceType type, Square square, Color color);
    void orderMoves(const Board& board, std::vector<Move>& moves);

    bool isKillerMove(const Move& move, int depth);
    int getHistoryScore(const Move& move);
    void recordKillerMove(const Move& move, int depth);
    void recordHistoryMove(const Move& move, int depth);
    bool isMoveInOpeningBook(const Board& board, const Move& move);
};

constexpr int MATE_SCORE = 10000;
constexpr int DRAW_SCORE = 0;

#endif // SEARCH_H
