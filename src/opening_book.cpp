#include "opening_book.h"
#include "movegen.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>

OpeningBook::OpeningBook() : rng(std::random_device{}()) {}
OpeningBook::~OpeningBook() {}

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
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (line.empty()) {
            if (inGame && !currentMoves.empty()) {
                processGame(currentEcoCode, currentName, currentMoves);
                currentMoves.clear();
                inGame = false;
            }
            continue;
        }

        if (line[0] == '[') {
            if (inGame && !currentMoves.empty()) {
                processGame(currentEcoCode, currentName, currentMoves);
                currentMoves.clear();
            }
            inGame = false;

            if (line.find("[Site") == 0) {
                size_t start = line.find('"') + 1;
                size_t end = line.find('"', start);
                if (start != std::string::npos && end != std::string::npos)
                    currentEcoCode = line.substr(start, end - start);
            } else if (line.find("[White") == 0) {
                size_t start = line.find('"') + 1;
                size_t end = line.find('"', start);
                if (start != std::string::npos && end != std::string::npos)
                    currentName = line.substr(start, end - start);
            }
        } else {
            inGame = true;
            std::istringstream iss(line);
            std::string token;
            while (iss >> token) {
                if (token.back() == '.') continue;
                if (token == "1-0" || token == "0-1" || token == "1/2-1/2" || token == "*") continue;
                if (token.empty() || token[0] == '{' || token[0] == ';') continue;
                while (!token.empty() && (token.back() == '!' || token.back() == '?' ||
                                          token.back() == '+' || token.back() == '#'))
                    token.pop_back();
                if (!token.empty()) currentMoves.push_back(token);
            }
        }
    }

    if (inGame && !currentMoves.empty())
        processGame(currentEcoCode, currentName, currentMoves);

    file.close();
    std::cerr << "Loaded " << book.size() << " positions from opening book" << std::endl;
    return true;
}

void OpeningBook::processGame(const std::string& ecoCode, const std::string& name,
                              const std::vector<std::string>& moves) {
    if (moves.empty()) return;

    Board board;
    std::string positionKey = positionToKey(board);

    for (size_t i = 0; i < moves.size(); i++) {
        Move move = parseMove(moves[i], board);
        if (move.from == move.to && move.from == 0) return;

        OpeningMove openingMove;
        openingMove.move = move;
        openingMove.ecoCode = ecoCode;
        openingMove.name = name;
        openingMove.frequency = 1;

        addMoveToBook(positionKey, openingMove);

        board.makeMove(move);
        positionKey = positionToKey(board);
    }
}

void OpeningBook::addMoveToBook(const std::string& positionKey, const OpeningMove& openingMove) {
    auto it = book.find(positionKey);
    if (it != book.end()) {
        for (auto& existing : it->second) {
            if (existing.move.from == openingMove.move.from &&
                existing.move.to == openingMove.move.to &&
                existing.move.promotion == openingMove.move.promotion) {
                existing.frequency++;
                return;
            }
        }
        it->second.push_back(openingMove);
    } else {
        book[positionKey] = {openingMove};
    }
}

std::string OpeningBook::positionToKey(const Board& board) {
    std::ostringstream oss;
    for (Square sq = 0; sq < 64; sq++) {
        Piece piece = board.pieceAt(sq);
        if (!piece.isEmpty())
            oss << static_cast<int>(piece.type) << static_cast<int>(piece.color) << sq << '|';
    }
    oss << static_cast<int>(board.getSideToMove());
    oss << board.canCastle(Color::WHITE, true) << board.canCastle(Color::WHITE, false);
    oss << board.canCastle(Color::BLACK, true) << board.canCastle(Color::BLACK, false);
    oss << board.getEnPassantSquare();
    return oss.str();
}

Move OpeningBook::parseMove(const std::string& moveStr, const Board& board) {
    Move move;

    if (moveStr == "O-O" || moveStr == "0-0") {
        Square kingSquare = (board.getSideToMove() == Color::WHITE) ? E1 : E8;
        Square kingTarget = (board.getSideToMove() == Color::WHITE) ? G1 : G8;
        move = Move(kingSquare, kingTarget);
        move.isCastle = true;
        return move;
    }

    if (moveStr == "O-O-O" || moveStr == "0-0-0") {
        Square kingSquare = (board.getSideToMove() == Color::WHITE) ? E1 : E8;
        Square kingTarget = (board.getSideToMove() == Color::WHITE) ? C1 : C8;
        move = Move(kingSquare, kingTarget);
        move.isCastle = true;
        return move;
    }

    if (moveStr.length() == 4 || moveStr.length() == 5) {
        int fromFile = moveStr[0] - 'a';
        int fromRank = moveStr[1] - '1';
        int toFile   = moveStr[2] - 'a';
        int toRank   = moveStr[3] - '1';

        if (fromFile >= 0 && fromFile < 8 && fromRank >= 0 && fromRank < 8 &&
            toFile   >= 0 && toFile   < 8 && toRank   >= 0 && toRank   < 8) {
            move.from = makeSquare(fromFile, fromRank);
            move.to   = makeSquare(toFile,   toRank);

            if (moveStr.length() == 5) {
                char p = std::tolower(moveStr[4]);
                switch (p) {
                    case 'q': move.promotion = PieceType::QUEEN;  break;
                    case 'r': move.promotion = PieceType::ROOK;   break;
                    case 'b': move.promotion = PieceType::BISHOP; break;
                    case 'n': move.promotion = PieceType::KNIGHT; break;
                }
            }
            return move;
        }
    }

    std::vector<Move> legalMoves = MoveGenerator::generateLegalMoves(board);
    for (const Move& m : legalMoves)
        if (moveToString(m) == moveStr) return m;
    for (const Move& m : legalMoves)
        if (moveToAlgebraic(m, board) == moveStr) return m;

    return Move();
}

std::string OpeningBook::moveToString(const Move& move) {
    if (move.isCastle)
        return (move.to == G1 || move.to == G8) ? "O-O" : "O-O-O";

    std::ostringstream oss;
    oss << static_cast<char>('a' + fileOf(move.from)) << (rankOf(move.from) + 1)
        << static_cast<char>('a' + fileOf(move.to))   << (rankOf(move.to)   + 1);

    if (move.promotion != PieceType::NONE) {
        switch (move.promotion) {
            case PieceType::QUEEN:  oss << "Q"; break;
            case PieceType::ROOK:   oss << "R"; break;
            case PieceType::BISHOP: oss << "B"; break;
            case PieceType::KNIGHT: oss << "N"; break;
            default: break;
        }
    }
    return oss.str();
}

std::string OpeningBook::moveToAlgebraic(const Move& move, const Board& board) {
    if (move.isCastle)
        return (move.to == G1 || move.to == G8) ? "O-O" : "O-O-O";

    Piece piece = board.pieceAt(move.from);
    if (piece.isEmpty()) return "";

    std::ostringstream oss;
    switch (piece.type) {
        case PieceType::KNIGHT: oss << "N"; break;
        case PieceType::BISHOP: oss << "B"; break;
        case PieceType::ROOK:   oss << "R"; break;
        case PieceType::QUEEN:  oss << "Q"; break;
        case PieceType::KING:   oss << "K"; break;
        default: break;
    }

    bool isCapture = move.isCapture || move.isEnPassant || !board.pieceAt(move.to).isEmpty();
    if (isCapture && piece.type == PieceType::PAWN)
        oss << static_cast<char>('a' + fileOf(move.from));
    if (isCapture) oss << "x";

    oss << static_cast<char>('a' + fileOf(move.to)) << (rankOf(move.to) + 1);

    if (move.promotion != PieceType::NONE) {
        switch (move.promotion) {
            case PieceType::QUEEN:  oss << "Q"; break;
            case PieceType::ROOK:   oss << "R"; break;
            case PieceType::BISHOP: oss << "B"; break;
            case PieceType::KNIGHT: oss << "N"; break;
            default: break;
        }
    }
    return oss.str();
}

Move OpeningBook::getRandomMove(const Board& board) {
    auto it = book.find(positionToKey(board));
    if (it == book.end() || it->second.empty()) return Move();

    int totalWeight = 0;
    for (const auto& m : it->second) totalWeight += m.frequency;
    if (totalWeight == 0) return it->second[0].move;

    std::uniform_int_distribution<int> dist(1, totalWeight);
    int randomWeight = dist(rng);
    int currentWeight = 0;
    for (const auto& m : it->second) {
        currentWeight += m.frequency;
        if (randomWeight <= currentWeight) return m.move;
    }
    return it->second.back().move;
}

std::vector<OpeningMove> OpeningBook::getMoves(const Board& board) {
    auto it = book.find(positionToKey(board));
    return (it != book.end()) ? it->second : std::vector<OpeningMove>{};
}

bool OpeningBook::isInBook(const Board& board) {
    return book.find(positionToKey(board)) != book.end();
}

std::string OpeningBook::getEcoCode(const Board& board) {
    auto it = book.find(positionToKey(board));
    if (it == book.end() || it->second.empty()) return "";
    return it->second[0].ecoCode;
}
