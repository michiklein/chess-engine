#include "opening_book.h"
#include "eco_book.h"
#include "movegen.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iostream>

OpeningBook::OpeningBook() : rng(std::random_device{}()) {}
OpeningBook::~OpeningBook() {}

bool OpeningBook::loadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) return false;  // caller may probe several paths
    return parseStream(file, filename);
}

bool OpeningBook::loadEmbedded() {
    std::istringstream stream(std::string(
        reinterpret_cast<const char*>(ECO_BOOK_DATA), ECO_BOOK_SIZE));
    return parseStream(stream, "embedded");
}

bool OpeningBook::parseStream(std::istream& file, const std::string& sourceName) {
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
                if (token.empty() || token[0] == '{' || token[0] == ';' || token[0] == '$') continue;
                while (!token.empty() && (token.back() == '!' || token.back() == '?' ||
                                          token.back() == '+' || token.back() == '#'))
                    token.pop_back();
                if (!token.empty()) currentMoves.push_back(token);
            }
        }
    }

    if (inGame && !currentMoves.empty())
        processGame(currentEcoCode, currentName, currentMoves);

    std::cerr << "Loaded " << book.size() << " positions from opening book ("
              << sourceName << ")" << std::endl;
    return !book.empty();
}

void OpeningBook::processGame(const std::string& ecoCode, const std::string& name,
                              const std::vector<std::string>& moves) {
    if (moves.empty()) return;

    Board board;

    for (size_t i = 0; i < moves.size(); i++) {
        Move move = parseMove(moves[i], board);
        if (move.from == move.to && move.from == 0) return;

        OpeningMove openingMove;
        openingMove.move = move;
        openingMove.ecoCode = ecoCode;
        openingMove.name = name;
        openingMove.frequency = 1;

        addMoveToBook(board.getHash(), openingMove);

        board.makeMove(move);
    }
}

void OpeningBook::addMoveToBook(uint64_t positionKey, const OpeningMove& openingMove) {
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

// Resolves a move string (castle, coordinate, or SAN) to one of the position's
// legal moves, so the returned move always carries correct capture/castle/
// en-passant flags. Returns a null move (from == to) if it can't be matched.
Move OpeningBook::parseMove(const std::string& moveStr, const Board& board) {
    std::vector<Move> legalMoves = MoveGenerator::generateLegalMoves(board);

    if (moveStr == "O-O" || moveStr == "0-0" || moveStr == "O-O-O" || moveStr == "0-0-0") {
        bool kingSide = (moveStr == "O-O" || moveStr == "0-0");
        for (const Move& m : legalMoves)
            if (m.isCastle && (fileOf(m.to) == 6) == kingSide) return m;
        return Move();
    }

    if (moveStr.length() == 4 || moveStr.length() == 5) {
        int fromFile = moveStr[0] - 'a';
        int fromRank = moveStr[1] - '1';
        int toFile   = moveStr[2] - 'a';
        int toRank   = moveStr[3] - '1';

        if (fromFile >= 0 && fromFile < 8 && fromRank >= 0 && fromRank < 8 &&
            toFile   >= 0 && toFile   < 8 && toRank   >= 0 && toRank   < 8) {
            Square from = makeSquare(fromFile, fromRank);
            Square to   = makeSquare(toFile,   toRank);

            PieceType promotion = PieceType::NONE;
            if (moveStr.length() == 5) {
                switch (std::tolower(moveStr[4])) {
                    case 'q': promotion = PieceType::QUEEN;  break;
                    case 'r': promotion = PieceType::ROOK;   break;
                    case 'b': promotion = PieceType::BISHOP; break;
                    case 'n': promotion = PieceType::KNIGHT; break;
                }
            }
            for (const Move& m : legalMoves)
                if (m.from == from && m.to == to && m.promotion == promotion) return m;
            return Move();
        }
    }

    // SAN matching; normalize "e8=Q" to "e8Q" first
    std::string san = moveStr;
    san.erase(std::remove(san.begin(), san.end(), '='), san.end());

    for (const Move& m : legalMoves)
        if (moveToString(m) == san) return m;
    for (const Move& m : legalMoves)
        if (moveToAlgebraic(m, board) == san) return m;

    // Disambiguated piece moves (Nbd2, R1a3, Qh4e1): compare against the plain
    // algebraic form with the from-file/rank hint(s) inserted after the piece.
    for (const Move& m : legalMoves) {
        std::string alg = moveToAlgebraic(m, board);
        if (alg.empty() || !std::isupper(static_cast<unsigned char>(alg[0]))) continue;
        std::string piece(1, alg[0]);
        std::string tail = alg.substr(1);
        std::string f(1, static_cast<char>('a' + fileOf(m.from)));
        std::string r(1, static_cast<char>('1' + rankOf(m.from)));
        if (san == piece + f + tail || san == piece + r + tail ||
            san == piece + f + r + tail) return m;
    }

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
    auto it = book.find(board.getHash());
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
