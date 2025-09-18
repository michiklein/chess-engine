#include "uci.h"
#include "board.h"
#include <iostream>

int main() {
    std::cout << "ChessEngine v1.0 - Starting..." << std::endl;
    
    UCIEngine engine;
    engine.run();
    
    return 0;
}