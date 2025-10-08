#include "opening_book.h"
#include "movegen.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>

OpeningBook::OpeningBook() : rng(std::random_device{}()) {
}

OpeningBook::~OpeningBook() {
}

bool OpeningBook::loadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Could not open opening book file: " << filename << std::endl;
        return false;
    }
    
    std::string line;
    std::string currentEcoCode;
    std::string currentName;
    std::vector<std::string> currentMoves;
    bool inGame = false;
    
    while (std::getline(file, line)) {
        // Remove carriage return if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        // Skip empty lines
        if (line.empty()) {
            if (inGame && !currentMoves.empty()) {
                processGame(currentEcoCode, currentName, currentMoves);
                currentMoves.clear();
                inGame = false;
            }
            continue;
        }
        
        // Parse header lines
        if (line[0] == '[') {
            if (inGame && !currentMoves.empty()) {
                processGame(currentEcoCode, currentName, currentMoves);
                currentMoves.clear();
            }
            inGame = false;
            
            if (line.find("[Site") == 0) {
                // Extract ECO code
                size_t start = line.find('"') + 1;
                size_t end = line.find('"', start);
                if (start != std::string::npos && end != std::string::npos) {
                    currentEcoCode = line.substr(start, end - start);
                }
            } else if (line.find("[White") == 0) {
                // Extract opening name
                size_t start = line.find('"') + 1;
                size_t end = line.find('"', start);
                if (start != std::string::npos && end != std::string::npos) {
                    currentName = line.substr(start, end - start);
                }
            }
        } else {
            // Parse move line
            inGame = true;
            std::istringstream iss(line);
            std::string token;
            
            while (iss >> token) {
                // Skip move numbers (e.g., "1.", "2.", etc.)
                if (token.back() == '.') {
                    continue;
                }
                
                // Skip result markers
                if (token == "1-0" || token == "0-1" || token == "1/2-1/2" || token == "*") {
                    continue;
                }
                
                // Skip empty tokens
                if (token.empty()) {
                    continue;
                }
                
                // Add move to current game
                currentMoves.push_back(token);
            }
        }
    }
    
    // Process last game if exists
    if (inGame && !currentMoves.empty()) {
        processGame(currentEcoCode, currentName, currentMoves);
    }
    
    file.close();
    std::cout << "Loaded " << book.size() << " positions from opening book" << std::endl;
    return true;
}

void OpeningBook::processGame(const std::string& ecoCode, const std::string& name, const std::vector<std::string>& moves) {
    if (moves.empty()) return;
    
    // Create a board and play through the moves
    Board board;
    std::string positionKey = positionToKey(board);
    
    for (size_t i = 0; i < moves.size(); i++) {
        Move move = parseMove(moves[i], board);
        if (move.from == move.to && move.from == 0) {
            // Invalid move, skip this game
            return;
        }
        
        // Add this move to the book for the current position
        OpeningMove openingMove;
        openingMove.move = move;
        openingMove.ecoCode = ecoCode;
        openingMove.name = name;
        openingMove.frequency = 1;
        
        addMoveToBook(positionKey, openingMove);
        
        // Make the move and update position key
        board.makeMove(move);
        positionKey = positionToKey(board);
    }
}

void OpeningBook::addMoveToBook(const std::string& positionKey, const OpeningMove& openingMove) {
    auto it = book.find(positionKey);
    if (it != book.end()) {
        // Position exists, check if this move already exists
        bool found = false;
        for (auto& existingMove : it->second) {
            if (existingMove.move.from == openingMove.move.from && 
                existingMove.move.to == openingMove.move.to &&
                existingMove.move.promotion == openingMove.move.promotion) {
                existingMove.frequency++;
                found = true;
                break;
            }
        }
        if (!found) {
            it->second.push_back(openingMove);
        }
    } else {
        // New position
        book[positionKey] = {openingMove};
    }
}

std::string OpeningBook::positionToKey(const Board& board) {
    // Create a simple position key based on piece positions
    std::ostringstream oss;
    
    // Add piece positions
    for (Square sq = 0; sq < 64; sq++) {
        Piece piece = board.pieceAt(sq);
        if (!piece.isEmpty()) {
            oss << static_cast<int>(piece.type) << static_cast<int>(piece.color) << sq << "|";
        }
    }
    
    // Add side to move
    oss << static_cast<int>(board.getSideToMove());
    
    // Add castling rights
    oss << board.canCastle(Color::WHITE, true) << board.canCastle(Color::WHITE, false);
    oss << board.canCastle(Color::BLACK, true) << board.canCastle(Color::BLACK, false);
    
    // Add en passant square
    oss << board.getEnPassantSquare();
    
    return oss.str();
}

Move OpeningBook::parseMove(const std::string& moveStr, const Board& board) {
    Move move;
    
    // Handle special moves
    if (moveStr == "O-O" || moveStr == "0-0") {
        // King-side castling
        Square kingSquare = (board.getSideToMove() == Color::WHITE) ? E1 : E8;
        Square kingTarget = (board.getSideToMove() == Color::WHITE) ? G1 : G8;
        move = Move(kingSquare, kingTarget);
        move.isCastle = true;
        return move;
    }
    
    if (moveStr == "O-O-O" || moveStr == "0-0-0") {
        // Queen-side castling
        Square kingSquare = (board.getSideToMove() == Color::WHITE) ? E1 : E8;
        Square kingTarget = (board.getSideToMove() == Color::WHITE) ? C1 : C8;
        move = Move(kingSquare, kingTarget);
        move.isCastle = true;
        return move;
    }
    
    // Handle coordinate notation (e2e4)
    if (moveStr.length() == 4 || moveStr.length() == 5) {
        int fromFile = moveStr[0] - 'a';
        int fromRank = moveStr[1] - '1';
        int toFile = moveStr[2] - 'a';
        int toRank = moveStr[3] - '1';
        
        if (fromFile >= 0 && fromFile < 8 && fromRank >= 0 && fromRank < 8 &&
            toFile >= 0 && toFile < 8 && toRank >= 0 && toRank < 8) {
            move.from = makeSquare(fromFile, fromRank);
            move.to = makeSquare(toFile, toRank);
            
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
    }
    
    // Parse algebraic notation by matching against legal moves
    std::vector<Move> legalMoves = MoveGenerator::generateLegalMoves(board);
    
    // First try to match coordinate notation
    for (const Move& legalMove : legalMoves) {
        std::string legalMoveStr = moveToString(legalMove);
        if (legalMoveStr == moveStr) {
            return legalMove;
        }
    }
    
    // Then try to match algebraic notation by converting to coordinate notation
    for (const Move& legalMove : legalMoves) {
        std::string algebraicStr = moveToAlgebraic(legalMove, board);
        if (algebraicStr == moveStr) {
            return legalMove;
        }
    }
    
    // If we can't parse the move, return an invalid move
    return Move();
}

std::string OpeningBook::moveToString(const Move& move) {
    std::ostringstream oss;
    
    if (move.isCastle) {
        if (move.to == G1 || move.to == G8) {
            return "O-O";
        } else {
            return "O-O-O";
        }
    }
    
    // Convert square to algebraic notation
    int fromFile = fileOf(move.from);
    int fromRank = rankOf(move.from);
    int toFile = fileOf(move.to);
    int toRank = rankOf(move.to);
    
    oss << static_cast<char>('a' + fromFile) << (fromRank + 1);
    oss << static_cast<char>('a' + toFile) << (toRank + 1);
    
    if (move.promotion != PieceType::NONE) {
        switch (move.promotion) {
            case PieceType::QUEEN: oss << "Q"; break;
            case PieceType::ROOK: oss << "R"; break;
            case PieceType::BISHOP: oss << "B"; break;
            case PieceType::KNIGHT: oss << "N"; break;
            default: break;
        }
    }
    
    return oss.str();
}

std::string OpeningBook::moveToAlgebraic(const Move& move, const Board& board) {
    std::ostringstream oss;
    
    if (move.isCastle) {
        if (move.to == G1 || move.to == G8) {
            return "O-O";
        } else {
            return "O-O-O";
        }
    }
    
    // Get the piece type
    Piece piece = board.pieceAt(move.from);
    if (piece.isEmpty()) return "";
    
    // Add piece symbol (except for pawns)
    switch (piece.type) {
        case PieceType::KNIGHT: oss << "N"; break;
        case PieceType::BISHOP: oss << "B"; break;
        case PieceType::ROOK: oss << "R"; break;
        case PieceType::QUEEN: oss << "Q"; break;
        case PieceType::KING: oss << "K"; break;
        default: break; // Pawn - no symbol
    }
    
    // Add destination square
    int toFile = fileOf(move.to);
    int toRank = rankOf(move.to);
    oss << static_cast<char>('a' + toFile) << (toRank + 1);
    
    // Add promotion
    if (move.promotion != PieceType::NONE) {
        switch (move.promotion) {
            case PieceType::QUEEN: oss << "Q"; break;
            case PieceType::ROOK: oss << "R"; break;
            case PieceType::BISHOP: oss << "B"; break;
            case PieceType::KNIGHT: oss << "N"; break;
            default: break;
        }
    }
    
    return oss.str();
}

Move OpeningBook::getRandomMove(const Board& board) {
    std::string positionKey = positionToKey(board);
    auto it = book.find(positionKey);
    
    if (it == book.end() || it->second.empty()) {
        return Move(); // No moves in book
    }
    
    // Weight moves by frequency
    int totalWeight = 0;
    for (const auto& openingMove : it->second) {
        totalWeight += openingMove.frequency;
    }
    
    if (totalWeight == 0) {
        return it->second[0].move;
    }
    
    std::uniform_int_distribution<int> dist(1, totalWeight);
    int randomWeight = dist(rng);
    
    int currentWeight = 0;
    for (const auto& openingMove : it->second) {
        currentWeight += openingMove.frequency;
        if (randomWeight <= currentWeight) {
            return openingMove.move;
        }
    }
    
    return it->second.back().move;
}

std::vector<OpeningMove> OpeningBook::getMoves(const Board& board) {
    std::string positionKey = positionToKey(board);
    auto it = book.find(positionKey);
    
    if (it == book.end()) {
        return {};
    }
    
    return it->second;
}

bool OpeningBook::isInBook(const Board& board) {
    std::string positionKey = positionToKey(board);
    return book.find(positionKey) != book.end();
}

std::string OpeningBook::getEcoCode(const Board& board) {
    std::string positionKey = positionToKey(board);
    auto it = book.find(positionKey);
    
    if (it == book.end() || it->second.empty()) {
        return "";
    }
    
    return it->second[0].ecoCode;
}
