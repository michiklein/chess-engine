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

    // Random choice among the position's book moves, weighted by how many
    // book lines continue through each move (mainlines are preferred)
    Move getRandomMove(const Board& board);

private:
    struct BookMove {
        Move move;
        int weight;
    };

    // Positions are keyed by the board's zobrist hash, so transpositions
    // (same position via a different move order) share book entries.
    std::unordered_map<uint64_t, std::vector<BookMove>> book;
    std::mt19937 rng;

    bool parseStream(std::istream& file, const std::string& sourceName);
    Move parseMove(const std::string& moveStr, const Board& board);
    std::string moveToString(const Move& move);
    std::string moveToAlgebraic(const Move& move, const Board& board);
    // weight < 0: increment the existing count by 1 (legacy named-opening
    // counting); weight >= 0: set the count explicitly (e.g. a real game
    // count from a master database).
    void addMoveToBook(uint64_t positionKey, const Move& move, int weight = -1);
    // Each pair is (move token, explicit weight or -1)
    void processGame(const std::vector<std::pair<std::string, int>>& moves);
};

#endif // OPENING_BOOK_H
