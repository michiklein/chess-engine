#include "board.h"
#include "movegen.h"
#include <algorithm>
#include <sstream>
#include <cctype>
#include <cmath>

// ---------------------------------------------------------------------------
// Zobrist keys (file-local, initialized once at startup)
// ---------------------------------------------------------------------------
namespace {
    uint64_t ZOB_PIECES[12][64];
    uint64_t ZOB_SIDE;
    uint64_t ZOB_CASTLE[4];
    uint64_t ZOB_EP[8];

    struct ZobristInit {
        ZobristInit() {
            uint64_t s = 0x9E3779B97F4A7C15ULL;
            auto rng = [&]() -> uint64_t {  // splitmix64
                s += 0x9E3779B97F4A7C15ULL;
                uint64_t z = s;
                z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
                z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
                return z ^ (z >> 31);
            };
            for (auto& pieceKeys : ZOB_PIECES)
                for (auto& key : pieceKeys) key = rng();
            ZOB_SIDE = rng();
            for (auto& key : ZOB_CASTLE) key = rng();
            for (auto& key : ZOB_EP)     key = rng();
        }
    } zobristInit;
}

// ---------------------------------------------------------------------------

Board::Board() {
    setupStartingPosition();
}

void Board::setupStartingPosition() {
    fromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

void Board::addPiece(Square sq, Piece piece) {
    int idx = getPieceIndex(piece.type, piece.color);
    Bitboard bit = 1ULL << sq;
    pieceBitboards[idx] |= bit;
    (piece.color == Color::WHITE ? whitePieces : blackPieces) |= bit;
    allPieces |= bit;
    squares[sq] = piece;
    hash ^= ZOB_PIECES[idx][sq];
}

void Board::removePiece(Square sq) {
    Piece piece = squares[sq];
    int idx = getPieceIndex(piece.type, piece.color);
    Bitboard bit = 1ULL << sq;
    pieceBitboards[idx] &= ~bit;
    (piece.color == Color::WHITE ? whitePieces : blackPieces) &= ~bit;
    allPieces &= ~bit;
    squares[sq] = Piece();
    hash ^= ZOB_PIECES[idx][sq];
}

uint64_t Board::castlingHash() const {
    uint64_t h = 0;
    if (canCastleKingSide[0])  h ^= ZOB_CASTLE[0];
    if (canCastleQueenSide[0]) h ^= ZOB_CASTLE[1];
    if (canCastleKingSide[1])  h ^= ZOB_CASTLE[2];
    if (canCastleQueenSide[1]) h ^= ZOB_CASTLE[3];
    return h;
}

void Board::recomputeHash() {
    hash = 0;
    for (Square sq = 0; sq < 64; sq++)
        if (!squares[sq].isEmpty())
            hash ^= ZOB_PIECES[getPieceIndex(squares[sq].type, squares[sq].color)][sq];
    if (sideToMove == Color::BLACK) hash ^= ZOB_SIDE;
    hash ^= castlingHash();
    if (enPassantSquare < 64) hash ^= ZOB_EP[fileOf(enPassantSquare)];
}

void Board::makeMove(const Move& move) {
    Undo u;
    u.captured       = Piece();
    u.capturedSquare = 64;
    u.castleK[0] = canCastleKingSide[0];  u.castleK[1] = canCastleKingSide[1];
    u.castleQ[0] = canCastleQueenSide[0]; u.castleQ[1] = canCastleQueenSide[1];
    u.enPassant     = enPassantSquare;
    u.halfMoveClock = halfMoveClock;
    u.hash          = hash;

    Piece moving = squares[move.from];

    // Remove the captured piece (en passant captures beside the target square)
    if (move.isEnPassant) {
        Square capSq = (sideToMove == Color::WHITE) ? move.to - 8 : move.to + 8;
        u.captured = squares[capSq];
        u.capturedSquare = capSq;
        removePiece(capSq);
    } else if (!squares[move.to].isEmpty()) {
        u.captured = squares[move.to];
        u.capturedSquare = move.to;
        removePiece(move.to);
    }

    removePiece(move.from);
    addPiece(move.to, (move.promotion != PieceType::NONE)
                          ? Piece(move.promotion, moving.color) : moving);

    if (move.isCastle) {
        if      (move.to == G1) { removePiece(H1); addPiece(F1, Piece(PieceType::ROOK, Color::WHITE)); }
        else if (move.to == C1) { removePiece(A1); addPiece(D1, Piece(PieceType::ROOK, Color::WHITE)); }
        else if (move.to == G8) { removePiece(H8); addPiece(F8, Piece(PieceType::ROOK, Color::BLACK)); }
        else if (move.to == C8) { removePiece(A8); addPiece(D8, Piece(PieceType::ROOK, Color::BLACK)); }
    }

    hash ^= castlingHash();
    updateCastlingRights(move, moving.type);
    hash ^= castlingHash();

    if (enPassantSquare < 64) hash ^= ZOB_EP[fileOf(enPassantSquare)];
    enPassantSquare = 64;
    if (moving.type == PieceType::PAWN &&
        std::abs(rankOf(move.to) - rankOf(move.from)) == 2)
        enPassantSquare = (move.from + move.to) / 2;

    halfMoveClock = (moving.type == PieceType::PAWN || u.capturedSquare < 64)
                        ? 0 : halfMoveClock + 1;
    if (sideToMove == Color::BLACK) fullMoveNumber++;
    sideToMove = ~sideToMove;
    hash ^= ZOB_SIDE;

    normalizeEnPassant();  // discard the ep square if no capture is possible
    if (enPassantSquare < 64) hash ^= ZOB_EP[fileOf(enPassantSquare)];

    undoStack.push_back(u);
}

void Board::unmakeMove(const Move& move) {
    if (undoStack.empty()) return;
    const Undo& u = undoStack.back();

    sideToMove = ~sideToMove;
    if (sideToMove == Color::BLACK) fullMoveNumber--;

    Piece moved = squares[move.to];
    removePiece(move.to);
    addPiece(move.from, (move.promotion != PieceType::NONE)
                            ? Piece(PieceType::PAWN, moved.color) : moved);

    if (move.isCastle) {
        if      (move.to == G1) { removePiece(F1); addPiece(H1, Piece(PieceType::ROOK, Color::WHITE)); }
        else if (move.to == C1) { removePiece(D1); addPiece(A1, Piece(PieceType::ROOK, Color::WHITE)); }
        else if (move.to == G8) { removePiece(F8); addPiece(H8, Piece(PieceType::ROOK, Color::BLACK)); }
        else if (move.to == C8) { removePiece(D8); addPiece(A8, Piece(PieceType::ROOK, Color::BLACK)); }
    }

    if (u.capturedSquare < 64) addPiece(u.capturedSquare, u.captured);

    canCastleKingSide[0]  = u.castleK[0];  canCastleKingSide[1]  = u.castleK[1];
    canCastleQueenSide[0] = u.castleQ[0];  canCastleQueenSide[1] = u.castleQ[1];
    enPassantSquare = u.enPassant;
    halfMoveClock   = u.halfMoveClock;
    hash            = u.hash;
    undoStack.pop_back();
}

void Board::makeNullMove() {
    Undo u;
    u.captured       = Piece();
    u.capturedSquare = 64;
    u.castleK[0] = canCastleKingSide[0];  u.castleK[1] = canCastleKingSide[1];
    u.castleQ[0] = canCastleQueenSide[0]; u.castleQ[1] = canCastleQueenSide[1];
    u.enPassant     = enPassantSquare;
    u.halfMoveClock = halfMoveClock;
    u.hash          = hash;

    if (enPassantSquare < 64) hash ^= ZOB_EP[fileOf(enPassantSquare)];
    enPassantSquare = 64;
    halfMoveClock++;
    sideToMove = ~sideToMove;
    hash ^= ZOB_SIDE;

    undoStack.push_back(u);
}

void Board::unmakeNullMove() {
    if (undoStack.empty()) return;
    const Undo& u = undoStack.back();
    sideToMove      = ~sideToMove;
    enPassantSquare = u.enPassant;
    halfMoveClock   = u.halfMoveClock;
    hash            = u.hash;
    undoStack.pop_back();
}

void Board::updateCastlingRights(const Move& move, PieceType movedType) {
    if (movedType == PieceType::KING) {
        int i = static_cast<int>(sideToMove);
        canCastleKingSide[i]  = false;
        canCastleQueenSide[i] = false;
    }
    if (move.from == A1 || move.to == A1) canCastleQueenSide[0] = false;
    if (move.from == H1 || move.to == H1) canCastleKingSide[0]  = false;
    if (move.from == A8 || move.to == A8) canCastleQueenSide[1] = false;
    if (move.from == H8 || move.to == H8) canCastleKingSide[1]  = false;
}

// Keep the en passant square only when the side to move can actually capture.
// A phantom ep square after every double push would make identical positions
// reached by different move orders (transpositions) hash and key differently,
// breaking opening book lookups, the TT, and repetition detection.
void Board::normalizeEnPassant() {
    if (enPassantSquare >= 64) return;
    if (!(MoveGenerator::getPawnAttacks(enPassantSquare, ~sideToMove) &
          getPieceBitboard(PieceType::PAWN, sideToMove)))
        enPassantSquare = 64;
}

bool Board::isRepetition() const {
    // Positions before the last irreversible move (pawn move / capture) can
    // never match, so only scan back as far as the half-move clock allows.
    int limit = std::min<int>(halfMoveClock, static_cast<int>(undoStack.size()));
    for (int i = 1; i <= limit; i++)
        if (undoStack[undoStack.size() - i].hash == hash) return true;
    return false;
}

bool Board::isInsufficientMaterial() const {
    for (Color c : {Color::WHITE, Color::BLACK}) {
        if (getPieceBitboard(PieceType::PAWN,  c) ||
            getPieceBitboard(PieceType::ROOK,  c) ||
            getPieceBitboard(PieceType::QUEEN, c)) return false;
    }
    int minors = popCount(getPieceBitboard(PieceType::KNIGHT, Color::WHITE) |
                          getPieceBitboard(PieceType::BISHOP, Color::WHITE) |
                          getPieceBitboard(PieceType::KNIGHT, Color::BLACK) |
                          getPieceBitboard(PieceType::BISHOP, Color::BLACK));
    return minors <= 1;  // a lone minor piece cannot force mate
}

int Board::countAttackedSquares(Color color) const {
    int count = 0;
    Bitboard pieces;

    pieces = getPieceBitboard(PieceType::PAWN, color);
    while (pieces) { Square sq = firstSquare(pieces); pieces &= pieces-1; count += popCount(MoveGenerator::getPawnAttacks(sq, color)); }

    pieces = getPieceBitboard(PieceType::KNIGHT, color);
    while (pieces) { Square sq = firstSquare(pieces); pieces &= pieces-1; count += popCount(MoveGenerator::getKnightAttacks(sq)); }

    pieces = getPieceBitboard(PieceType::BISHOP, color);
    while (pieces) { Square sq = firstSquare(pieces); pieces &= pieces-1; count += popCount(MoveGenerator::getBishopAttacks(sq, allPieces)); }

    pieces = getPieceBitboard(PieceType::ROOK, color);
    while (pieces) { Square sq = firstSquare(pieces); pieces &= pieces-1; count += popCount(MoveGenerator::getRookAttacks(sq, allPieces)); }

    pieces = getPieceBitboard(PieceType::QUEEN, color);
    while (pieces) { Square sq = firstSquare(pieces); pieces &= pieces-1; count += popCount(MoveGenerator::getQueenAttacks(sq, allPieces)); }

    return count;
}

bool Board::isInCheck(Color color) const {
    Square kingSquare = findKing(color);
    return kingSquare < 64 && isSquareAttacked(kingSquare, ~color);
}

Square Board::findKing(Color color) const {
    Bitboard king = getPieceBitboard(PieceType::KING, color);
    return king ? firstSquare(king) : 64;
}

bool Board::isSquareAttacked(Square sq, Color attacker) const {
    // A pawn of `attacker` attacks sq iff a pawn of the *defending* color on sq
    // would attack the attacker's pawn square (pawn attacks are not symmetric).
    if (MoveGenerator::getPawnAttacks(sq, ~attacker)  & getPieceBitboard(PieceType::PAWN,   attacker)) return true;
    if (MoveGenerator::getKnightAttacks(sq)           & getPieceBitboard(PieceType::KNIGHT, attacker)) return true;
    if (MoveGenerator::getKingAttacks(sq)             & getPieceBitboard(PieceType::KING,   attacker)) return true;

    Bitboard diag = MoveGenerator::getBishopAttacks(sq, allPieces);
    if (diag & (getPieceBitboard(PieceType::BISHOP, attacker) | getPieceBitboard(PieceType::QUEEN, attacker))) return true;

    Bitboard straight = MoveGenerator::getRookAttacks(sq, allPieces);
    if (straight & (getPieceBitboard(PieceType::ROOK, attacker) | getPieceBitboard(PieceType::QUEEN, attacker))) return true;

    return false;
}

bool Board::fromFEN(const std::string& fen) {
    std::istringstream iss(fen);
    std::string piecePlacement, sideStr, castlingStr, enPassantStr;
    int halfMove = 0, fullMove = 1;

    if (!(iss >> piecePlacement >> sideStr >> castlingStr >> enPassantStr >> halfMove >> fullMove)) {
        return false;
    }

    for (auto& bb : pieceBitboards) bb = EMPTY_BOARD;
    squares.fill(Piece());
    whitePieces = blackPieces = allPieces = 0;
    hash = 0;

    // Piece placement: ranks are given top (rank 8) to bottom (rank 1)
    int file = 0, rank = 7;
    for (char c : piecePlacement) {
        if (c == '/') {
            rank--;
            file = 0;
        } else if (c >= '1' && c <= '8') {
            file += c - '0';
        } else {
            Color color = std::isupper(c) ? Color::WHITE : Color::BLACK;
            PieceType type;
            switch (std::tolower(c)) {
                case 'p': type = PieceType::PAWN;   break;
                case 'n': type = PieceType::KNIGHT; break;
                case 'b': type = PieceType::BISHOP; break;
                case 'r': type = PieceType::ROOK;   break;
                case 'q': type = PieceType::QUEEN;  break;
                case 'k': type = PieceType::KING;   break;
                default: return false;
            }
            addPiece(makeSquare(file, rank), Piece(type, color));
            file++;
        }
    }

    sideToMove = (sideStr == "w") ? Color::WHITE : Color::BLACK;

    canCastleKingSide[0]  = castlingStr.find('K') != std::string::npos;
    canCastleQueenSide[0] = castlingStr.find('Q') != std::string::npos;
    canCastleKingSide[1]  = castlingStr.find('k') != std::string::npos;
    canCastleQueenSide[1] = castlingStr.find('q') != std::string::npos;

    if (enPassantStr == "-") {
        enPassantSquare = 64;
    } else {
        enPassantSquare = makeSquare(enPassantStr[0] - 'a', enPassantStr[1] - '1');
    }

    halfMoveClock  = halfMove;
    fullMoveNumber = fullMove;

    undoStack.clear();
    normalizeEnPassant();  // GUIs often send phantom ep squares in FEN
    recomputeHash();
    return true;
}
