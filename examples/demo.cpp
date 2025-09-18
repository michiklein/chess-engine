#include "../src/board.h"
#include "../src/movegen.h"
#include "../src/search.h"
#include <iostream>
#include <iomanip>

void printSeparator() {
    std::cout << std::string(50, '=') << std::endl;
}

int main() {
    std::cout << "ChessEngine Framework Demo" << std::endl;
    printSeparator();
    
    // Initialize board
    Board board;
    std::cout << "Initial position:" << std::endl;
    std::cout << board.toString() << std::endl;
    
    // Show legal moves
    auto moves = MoveGenerator::generateLegalMoves(board);
    std::cout << "Legal moves from starting position: " << moves.size() << std::endl;
    
    printSeparator();
    
    // Demonstrate search
    SearchEngine search;
    std::cout << "Searching at depth 3..." << std::endl;
    SearchResult result = search.search(board, 3);
    
    std::cout << "Search results:" << std::endl;
    std::cout << "- Best move: " << static_cast<char>('a' + fileOf(result.bestMove.from))
              << static_cast<char>('1' + rankOf(result.bestMove.from))
              << static_cast<char>('a' + fileOf(result.bestMove.to))
              << static_cast<char>('1' + rankOf(result.bestMove.to)) << std::endl;
    std::cout << "- Evaluation score: " << result.score << std::endl;
    std::cout << "- Nodes searched: " << result.nodesSearched << std::endl;
    std::cout << "- Search depth: " << result.depth << std::endl;
    
    printSeparator();
    
    // Make the best move and show resulting position
    board.makeMove(result.bestMove);
    std::cout << "Position after best move:" << std::endl;
    std::cout << board.toString() << std::endl;
    
    // Show opponent's legal moves
    auto opponentMoves = MoveGenerator::generateLegalMoves(board);
    std::cout << "Opponent has " << opponentMoves.size() << " legal moves" << std::endl;
    
    printSeparator();
    std::cout << "Demo completed!" << std::endl;
    
    return 0;
}