#include "uci.h"
#include "board.h"
#include <iostream>

int main(int argc, char* argv[]) {
    // Force unbuffered stdout so UCI GUIs receive each line immediately
    std::cout << std::unitbuf;
    UCIEngine engine(argc > 0 ? argv[0] : "");
    engine.run();
    return 0;
}