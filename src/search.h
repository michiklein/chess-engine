#ifndef SEARCH_H
#define SEARCH_H

#include "types.h"
#include "board.h"
#include "opening_book.h"
#include <atomic>
#include <chrono>
#include <limits>
#include <vector>

struct TTEntry {
    uint64_t hash  = 0;
    int      score = 0;
    int8_t   depth = -1;
    int8_t   flag  = 0;  // 0=exact  1=lower bound  2=upper bound
    Move     bestMove;
};

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

    // Reset transposition table and move-ordering heuristics (ucinewgame)
    void newGame();

    // Hard limit: the search aborts mid-iteration when it is reached.
    void setTimeLimit(int milliseconds) { timeLimit = milliseconds; }
    // Soft limit: target think time; iteration loop stops around it, stretched
    // when the score drops or shrunk when the best move is stable.
    void setSoftTimeLimit(int milliseconds) { softLimit = milliseconds; }
    void setNodeLimit(int limit) { nodeLimit = limit; }
    void setQuietMode(bool quiet) { quietMode = quiet; }
    // Disable the book for a search (e.g. "go infinite" = analysis mode)
    void setBookEnabled(bool enabled) { bookEnabled = enabled; }
    void setStopFlag(std::atomic<bool>* flag) { stopFlag = flag; }

    bool loadOpeningBook(const std::string& filename);
    bool loadEmbeddedOpeningBook();

    int getPieceValue(PieceType type);

protected:
    virtual int evaluate(const Board& board);

private:
    int timeLimit;      // ms; 0 = unlimited (hard)
    int softLimit{0};   // ms; 0 = derive from timeLimit
    int nodeLimit;  // node count; 0 = unlimited
    int nodesSearched;
    int currentDepth;
    bool quietMode;
    std::chrono::steady_clock::time_point searchStart;
    std::atomic<bool>* stopFlag{nullptr};
    mutable bool timeUpFlag{false};  // latched result of the periodic clock check

    static constexpr int TT_SIZE = 1 << 20;  // ~1M entries
    std::vector<TTEntry> tt;

    bool isTimeUp() const;

    OpeningBook openingBook;
    bool useOpeningBook;
    bool bookEnabled{true};

    static const int MAX_KILLER_MOVES = 2;
    Move killerMoves[32][MAX_KILLER_MOVES];
    int historyTable[64][64];

    int alphaBeta(Board& board, int depth, int alpha, int beta, bool nullMoveAllowed);
    int quiescence(Board& board, int alpha, int beta);

    // Static exchange evaluation: expected material outcome of a capture
    // after all profitable recaptures on the target square
    int see(const Board& board, const Move& move);

    int getPositionalValue(PieceType type, Square square, Color color, bool endgame);
    // ttMove (if valid, i.e. from != to) is ordered first; depth selects the
    // killer-move slot.
    void orderMoves(const Board& board, std::vector<Move>& moves,
                    const Move& ttMove, int depth);

    bool isKillerMove(const Move& move, int depth);
    int getHistoryScore(const Move& move);
    void recordKillerMove(const Move& move, int depth);
    void recordHistoryMove(const Move& move, int depth);
};

constexpr int MATE_SCORE = 10000;
constexpr int DRAW_SCORE = 0;

#endif // SEARCH_H
