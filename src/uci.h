#ifndef UCI_H
#define UCI_H

#include "board.h"
#include "search.h"
#include <atomic>
#include <string>
#include <thread>
#include <vector>

class UCIEngine {
private:
    Board board;
    SearchEngine search;
    bool isRunning;
    std::thread searchThread;
    std::atomic<bool> stopRequested{false};
    
public:
    // exePath (argv[0]) is used to locate eco.pgn next to the executable
    explicit UCIEngine(const std::string& exePath = "");
    ~UCIEngine();

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
    bool tryApplyMove(const std::string& moveStr);
    void sendBestMove(const Move& move);
};

#endif // UCI_H