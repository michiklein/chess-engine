#include "uci.h"
#include "movegen.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <cctype>
#include <cstdlib>

UCIEngine::UCIEngine(const std::string& exePath) : isRunning(false) {
    board.setupStartingPosition();
    search.setQuietMode(true);

    // GUIs launch the engine from arbitrary working directories, so probe a
    // few likely locations for the opening book, including next to the binary.
    std::vector<std::string> candidates = {"src/eco.pgn", "eco.pgn", "../src/eco.pgn"};
    size_t slash = exePath.find_last_of('/');
    if (slash != std::string::npos) {
        std::string exeDir = exePath.substr(0, slash);
        candidates.push_back(exeDir + "/eco.pgn");
        candidates.push_back(exeDir + "/../src/eco.pgn");
    }
    for (const std::string& path : candidates)
        if (search.loadOpeningBook(path)) break;
}

UCIEngine::~UCIEngine() {
    stopRequested = true;
    if (searchThread.joinable()) searchThread.join();
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
    // Make sure no search is reading the engine state while we reset it
    stopRequested = true;
    if (searchThread.joinable()) searchThread.join();
    board.setupStartingPosition();
    search.newGame();
}

void UCIEngine::handlePosition(const std::vector<std::string>& tokens) {
    if (tokens.size() < 2) return;
    
    if (tokens[1] == "startpos") {
        board.setupStartingPosition();

        // Handle moves
        for (size_t i = 2; i < tokens.size(); i++) {
            if (tokens[i] == "moves") {
                for (size_t j = i + 1; j < tokens.size(); j++) {
                    if (!tryApplyMove(tokens[j])) break;
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
                if (!tryApplyMove(tokens[i])) break;
            }
        }
    }
}

// Parses a coordinate move, validates it against the legal moves of the
// current position, and applies it. An illegal or malformed move is rejected
// (returns false) so it can't corrupt the board state.
bool UCIEngine::tryApplyMove(const std::string& moveStr) {
    Move parsed = parseMove(moveStr);
    if (parsed.from == parsed.to) return false;

    for (const Move& legal : MoveGenerator::generateLegalMoves(board)) {
        if (legal.from == parsed.from && legal.to == parsed.to &&
            legal.promotion == parsed.promotion) {
            board.makeMove(legal);
            return true;
        }
    }
    std::cout << "info string ignoring illegal move " << moveStr << std::endl;
    return false;
}

void UCIEngine::handleGo(const std::vector<std::string>& tokens) {
    int depth     = 0;
    int nodes     = 0;
    int movetime  = 0;
    int wtime = 0, btime = 0, winc = 0, binc = 0, movestogo = 30;
    bool infinite = false;
    constexpr int MAX_PRACTICAL_DEPTH = 12;

    for (size_t i = 1; i < tokens.size(); i++) {
        auto intArg = [&]() -> int {
            if (i + 1 >= tokens.size()) return 0;
            try { return std::stoi(tokens[i + 1]); }
            catch (...) { return 0; }  // malformed number: ignore, don't crash
        };
        if      (tokens[i] == "depth")     depth     = intArg();
        else if (tokens[i] == "nodes")     nodes     = intArg();
        else if (tokens[i] == "movetime")  movetime  = intArg();
        else if (tokens[i] == "wtime")     wtime     = intArg();
        else if (tokens[i] == "btime")     btime     = intArg();
        else if (tokens[i] == "winc")      winc      = intArg();
        else if (tokens[i] == "binc")      binc      = intArg();
        else if (tokens[i] == "movestogo") movestogo = intArg();
        else if (tokens[i] == "infinite")  infinite  = true;
    }

    int timeLimitMs = 0;
    if (movetime > 0) {
        timeLimitMs = movetime;
    } else if (!infinite && (wtime > 0 || btime > 0)) {
        int myTime = (board.getSideToMove() == Color::WHITE) ? wtime : btime;
        int myInc  = (board.getSideToMove() == Color::WHITE) ? winc  : binc;
        if (movestogo <= 0) movestogo = 30;
        timeLimitMs = myTime / movestogo + myInc / 2;
        timeLimitMs = std::max(timeLimitMs, 50);
        timeLimitMs = std::min(timeLimitMs, myTime / 2);
        timeLimitMs = std::max(timeLimitMs, 1);  // 0 would mean "no limit"
    } else if (!infinite && depth == 0 && nodes == 0) {
        timeLimitMs = 5000;  // bare "go": think for 5 seconds instead of forever
    }

    // Stop any in-progress search before starting a new one
    stopRequested = true;
    if (searchThread.joinable()) searchThread.join();
    stopRequested = false;

    search.setTimeLimit(timeLimitMs);
    search.setNodeLimit(nodes);
    search.setStopFlag(&stopRequested);

    if (depth > MAX_PRACTICAL_DEPTH && timeLimitMs == 0 && nodes == 0 && !infinite) {
        std::cout << "info string requested depth " << depth
                  << " capped to " << MAX_PRACTICAL_DEPTH
                  << " (no time/nodes limit set)" << std::endl;
        depth = MAX_PRACTICAL_DEPTH;
    }

    int searchDepth = (depth > 0) ? depth : 64;
    Board boardCopy = board;  // snapshot so position commands don't race

    searchThread = std::thread([this, boardCopy, searchDepth]() mutable {
        SearchResult result = search.search(boardCopy, searchDepth);
        sendInfo(result);
        sendBestMove(result.bestMove);
    });
}

void UCIEngine::handleStop() {
    stopRequested = true;
    if (searchThread.joinable()) searchThread.join();
}

void UCIEngine::handleQuit() {
    stopRequested = true;
    if (searchThread.joinable()) searchThread.join();
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
        return Move();
    }

    int fromFile = moveStr[0] - 'a';
    int fromRank = moveStr[1] - '1';
    Square from = makeSquare(fromFile, fromRank);

    int toFile = moveStr[2] - 'a';
    int toRank = moveStr[3] - '1';
    Square to = makeSquare(toFile, toRank);

    Move move(from, to);

    // Promotion
    if (moveStr.length() == 5) {
        char promotionChar = std::tolower(moveStr[4]);
        switch (promotionChar) {
            case 'q': move.promotion = PieceType::QUEEN;  break;
            case 'r': move.promotion = PieceType::ROOK;   break;
            case 'b': move.promotion = PieceType::BISHOP; break;
            case 'n': move.promotion = PieceType::KNIGHT; break;
        }
    }

    // Set flags from board state so makeMove behaves correctly
    Piece fromPiece = board.pieceAt(from);

    // Castling: king moves two files
    if (fromPiece.type == PieceType::KING && std::abs(fileOf(from) - fileOf(to)) == 2) {
        move.isCastle = true;
    }

    // En passant: pawn moves diagonally to the en passant square (empty target)
    if (fromPiece.type == PieceType::PAWN &&
        to == board.getEnPassantSquare() &&
        board.pieceAt(to).isEmpty()) {
        move.isEnPassant = true;
        move.isCapture   = true;
    }

    // Regular capture
    if (!board.pieceAt(to).isEmpty()) {
        move.isCapture = true;
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
    // No legal move (mate/stalemate): UCI convention is "bestmove 0000"
    if (move.from == move.to)
        std::cout << "bestmove 0000" << std::endl;
    else
        std::cout << "bestmove " << moveToString(move) << std::endl;
}

void UCIEngine::sendInfo(const SearchResult& result) {
    std::cout << "info depth " << result.depth 
              << " score cp " << result.score 
              << " nodes " << result.nodesSearched 
              << " pv " << moveToString(result.bestMove) << std::endl;
}
