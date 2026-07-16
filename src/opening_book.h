#ifndef OPENING_BOOK_H
#define OPENING_BOOK_H

#include "types.h"
#include "board.h"
#include <iosfwd>
#include <vector>
#include <string>
#include <unordered_map>
#include <random>

class OpeningBook {
public:
    OpeningBook();
    ~OpeningBook();

    bool loadFromFile(const std::string& filename);
    bool loadEmbedded();  // book compiled into the binary (eco_book.cpp)

    // Uniform random choice among the position's book moves
    Move getRandomMove(const Board& board);

private:
    // Positions are keyed by the board's zobrist hash, so transpositions
    // (same position via a different move order) share book entries.
    std::unordered_map<uint64_t, std::vector<Move>> book;
    std::mt19937 rng;

    bool parseStream(std::istream& file, const std::string& sourceName);
    Move parseMove(const std::string& moveStr, const Board& board);
    std::string moveToString(const Move& move);
    std::string moveToAlgebraic(const Move& move, const Board& board);
    void addMoveToBook(uint64_t positionKey, const Move& move);
    void processGame(const std::vector<std::string>& moves);
};

#endif // OPENING_BOOK_H
