#include "uci.h"
#include "board.h"
#include <iostream>

int main() {
    // Force unbuffered stdout so UCI GUIs receive each line immediately
    std::cout << std::unitbuf;
    UCIEngine engine;
    engine.run();
    return 0;
}