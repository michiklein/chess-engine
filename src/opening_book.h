#ifndef OPENING_BOOK_H
#define OPENING_BOOK_H

#include "types.h"
#include "board.h"
#include <iosfwd>
#include <vector>
#include <string>
#include <unordered_map>
#include <random>

struct OpeningMove {
    Move move;
    std::string ecoCode;
    std::string name;
    int frequency;
};

class OpeningBook {
public:
    OpeningBook();
    ~OpeningBook();

    bool loadFromFile(const std::string& filename);
    bool loadEmbedded();  // book compiled into the binary (eco_book.cpp)

    Move getRandomMove(const Board& board);
    std::vector<OpeningMove> getMoves(const Board& board);
    bool isInBook(const Board& board);
    std::string getEcoCode(const Board& board);

private:
    std::unordered_map<std::string, std::vector<OpeningMove>> book;
    std::mt19937 rng;

    bool parseStream(std::istream& file, const std::string& sourceName);
    std::string positionToKey(const Board& board);
    Move parseMove(const std::string& moveStr, const Board& board);
    std::string moveToString(const Move& move);
    std::string moveToAlgebraic(const Move& move, const Board& board);
    void addMoveToBook(const std::string& positionKey, const OpeningMove& openingMove);

    void processGame(const std::string& ecoCode, const std::string& name, const std::vector<std::string>& moves);
};

#endif // OPENING_BOOK_H
