#ifndef UCI_H
#define UCI_H

#include "board.h"
#include "search.h"
#include <string>
#include <vector>

class UCIEngine {
private:
    Board board;
    SearchEngine search;
    bool isRunning;
    
public:
    UCIEngine();
    
    // Main UCI loop
    void run();
    
    // Handle UCI commands
    void handleCommand(const std::string& command);
    
private:
    // UCI command handlers
    void handleUCI();
    void handleIsReady();
    void handleNewGame();
    void handlePosition(const std::vector<std::string>& tokens);
    void handleGo(const std::vector<std::string>& tokens);
    void handleStop();
    void handleQuit();
    
    // Utility functions
    std::vector<std::string> split(const std::string& str, char delimiter = ' ');
    Move parseMove(const std::string& moveStr);
    std::string moveToString(const Move& move);
    void sendBestMove(const Move& move);
    void sendInfo(const SearchResult& result);
};

#endif // UCI_H