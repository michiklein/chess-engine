#include "uci.h"
#include "movegen.h"
#include <iostream>
#include <sstream>
#include <cctype>

UCIEngine::UCIEngine() : isRunning(false) {
    board.setupStartingPosition();
    
    // Load opening book
    if (search.loadOpeningBook("src/eco.pgn")) {
        std::cout << "Opening book loaded successfully" << std::endl;
    } else {
        std::cout << "Warning: Could not load opening book" << std::endl;
    }
}

void UCIEngine::run() {
    isRunning = true;
    std::string line;
    
    while (isRunning && std::getline(std::cin, line)) {
        if (!line.empty()) {
            handleCommand(line);
        }
    }
}

void UCIEngine::handleCommand(const std::string& command) {
    std::vector<std::string> tokens = split(command);
    
    if (tokens.empty()) return;
    
    const std::string& cmd = tokens[0];
    
    if (cmd == "uci") {
        handleUCI();
    } else if (cmd == "isready") {
        handleIsReady();
    } else if (cmd == "ucinewgame") {
        handleNewGame();
    } else if (cmd == "position") {
        handlePosition(tokens);
    } else if (cmd == "go") {
        handleGo(tokens);
    } else if (cmd == "stop") {
        handleStop();
    } else if (cmd == "quit") {
        handleQuit();
    }
}

void UCIEngine::handleUCI() {
    std::cout << "id name ChessEngine v1.0" << std::endl;
    std::cout << "id author Chess Engine Project" << std::endl;
    std::cout << "uciok" << std::endl;
}

void UCIEngine::handleIsReady() {
    std::cout << "readyok" << std::endl;
}

void UCIEngine::handleNewGame() {
    board.setupStartingPosition();
}

void UCIEngine::handlePosition(const std::vector<std::string>& tokens) {
    if (tokens.size() < 2) return;
    
    if (tokens[1] == "startpos") {
        board.setupStartingPosition();
        
        // Handle moves
        for (size_t i = 2; i < tokens.size(); i++) {
            if (tokens[i] == "moves") {
                for (size_t j = i + 1; j < tokens.size(); j++) {
                    Move move = parseMove(tokens[j]);
                    board.makeMove(move);
                }
                break;
            }
        }
    } else if (tokens[1] == "fen") {
        // Handle FEN position
        std::string fen;
        size_t movesIndex = tokens.size();
        
        for (size_t i = 2; i < tokens.size() && tokens[i] != "moves"; i++) {
            if (i > 2) fen += " ";
            fen += tokens[i];
            movesIndex = i + 1;
        }
        
        board.fromFEN(fen);
        
        // Handle moves after FEN
        if (movesIndex < tokens.size() && tokens[movesIndex] == "moves") {
            for (size_t i = movesIndex + 1; i < tokens.size(); i++) {
                Move move = parseMove(tokens[i]);
                board.makeMove(move);
            }
        }
    }
}

void UCIEngine::handleGo(const std::vector<std::string>& tokens) {
    int depth = 4; // Default depth
    
    // Parse go command parameters
    for (size_t i = 1; i < tokens.size(); i++) {
        if (tokens[i] == "depth" && i + 1 < tokens.size()) {
            depth = std::stoi(tokens[i + 1]);
        }
    }
    
    SearchResult result = search.search(board, depth);
    sendInfo(result);
    sendBestMove(result.bestMove);
}

void UCIEngine::handleStop() {
    // Stop search (placeholder)
}

void UCIEngine::handleQuit() {
    isRunning = false;
}

std::vector<std::string> UCIEngine::split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    
    while (std::getline(ss, token, delimiter)) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    
    return tokens;
}

Move UCIEngine::parseMove(const std::string& moveStr) {
    if (moveStr.length() < 4) {
        return Move(); // Invalid move
    }
    
    // Parse from square
    int fromFile = moveStr[0] - 'a';
    int fromRank = moveStr[1] - '1';
    Square from = makeSquare(fromFile, fromRank);
    
    // Parse to square
    int toFile = moveStr[2] - 'a';
    int toRank = moveStr[3] - '1';
    Square to = makeSquare(toFile, toRank);
    
    Move move(from, to);
    
    // Handle promotion
    if (moveStr.length() == 5) {
        char promotionChar = std::tolower(moveStr[4]);
        switch (promotionChar) {
            case 'q': move.promotion = PieceType::QUEEN; break;
            case 'r': move.promotion = PieceType::ROOK; break;
            case 'b': move.promotion = PieceType::BISHOP; break;
            case 'n': move.promotion = PieceType::KNIGHT; break;
        }
    }
    
    return move;
}

std::string UCIEngine::moveToString(const Move& move) {
    std::string result;
    
    // From square
    result += static_cast<char>('a' + fileOf(move.from));
    result += static_cast<char>('1' + rankOf(move.from));
    
    // To square
    result += static_cast<char>('a' + fileOf(move.to));
    result += static_cast<char>('1' + rankOf(move.to));
    
    // Promotion
    if (move.promotion != PieceType::NONE) {
        switch (move.promotion) {
            case PieceType::QUEEN:  result += 'q'; break;
            case PieceType::ROOK:   result += 'r'; break;
            case PieceType::BISHOP: result += 'b'; break;
            case PieceType::KNIGHT: result += 'n'; break;
            default: break;
        }
    }
    
    return result;
}

void UCIEngine::sendBestMove(const Move& move) {
    std::cout << "bestmove " << moveToString(move) << std::endl;
}

void UCIEngine::sendInfo(const SearchResult& result) {
    std::cout << "info depth " << result.depth 
              << " score cp " << result.score 
              << " nodes " << result.nodesSearched 
              << " pv " << moveToString(result.bestMove) << std::endl;
}