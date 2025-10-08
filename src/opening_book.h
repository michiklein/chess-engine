#ifndef OPENING_BOOK_H
#define OPENING_BOOK_H

#include "types.h"
#include "board.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <random>

struct OpeningMove {
    Move move;
    std::string ecoCode;
    std::string name;
    int frequency; // How often this move appears in the database
};

class OpeningBook {
public:
    OpeningBook();
    ~OpeningBook();
    
    // Load opening book from ECO.pgn file
    bool loadFromFile(const std::string& filename);
    
    
    // Get a random opening move for the current position
    Move getRandomMove(const Board& board);
    
    // Get all possible opening moves for the current position
    std::vector<OpeningMove> getMoves(const Board& board);
    
    // Check if position is in opening book
    bool isInBook(const Board& board);
    
    // Get ECO code for current position
    std::string getEcoCode(const Board& board);
    
private:
    // Hash table to store positions and their moves
    std::unordered_map<std::string, std::vector<OpeningMove>> book;
    
    
    // Random number generator
    std::mt19937 rng;
    
    // Helper functions
    std::string positionToKey(const Board& board);
    Move parseMove(const std::string& moveStr, const Board& board);
    std::string moveToString(const Move& move);
    std::string moveToAlgebraic(const Move& move, const Board& board);
    void addMoveToBook(const std::string& positionKey, const OpeningMove& openingMove);
    
    // PGN parsing helpers
    bool parsePGNLine(const std::string& line, std::string& ecoCode, std::string& name, std::vector<std::string>& moves);
    void processGame(const std::string& ecoCode, const std::string& name, const std::vector<std::string>& moves);
};

#endif // OPENING_BOOK_H
