#include "board.h"
#include "movegen.h"
#include <algorithm>
#include <sstream>
#include <cctype>
#include <cmath>

Board::Board() {
    setupStartingPosition();
}

void Board::setupStartingPosition() {
    for (auto& bb : pieceBitboards) bb = EMPTY_BOARD;

    pieceBitboards[0] = setBit(setBit(setBit(setBit(setBit(setBit(setBit(setBit(EMPTY_BOARD, A2), B2), C2), D2), E2), F2), G2), H2);
    pieceBitboards[1] = setBit(setBit(EMPTY_BOARD, B1), G1);
    pieceBitboards[2] = setBit(setBit(EMPTY_BOARD, C1), F1);
    pieceBitboards[3] = setBit(setBit(EMPTY_BOARD, A1), H1);
    pieceBitboards[4] = setBit(EMPTY_BOARD, D1);
    pieceBitboards[5] = setBit(EMPTY_BOARD, E1);
    pieceBitboards[6] = setBit(setBit(setBit(setBit(setBit(setBit(setBit(setBit(EMPTY_BOARD, A7), B7), C7), D7), E7), F7), G7), H7);
    pieceBitboards[7] = setBit(setBit(EMPTY_BOARD, B8), G8);
    pieceBitboards[8] = setBit(setBit(EMPTY_BOARD, C8), F8);
    pieceBitboards[9] = setBit(setBit(EMPTY_BOARD, A8), H8);
    pieceBitboards[10] = setBit(EMPTY_BOARD, D8);
    pieceBitboards[11] = setBit(EMPTY_BOARD, E8);

    sideToMove = Color::WHITE;
    canCastleKingSide[0] = canCastleKingSide[1] = true;
    canCastleQueenSide[0] = canCastleQueenSide[1] = true;
    enPassantSquare = 64;
    halfMoveClock = 0;
    fullMoveNumber = 1;
    gameHistory.clear();

    updateCombinedBitboards();
}

Piece Board::pieceAt(Square sq) const {
    for (int i = 0; i < 12; i++) {
        if (getBit(pieceBitboards[i], sq)) {
            PieceType type = static_cast<PieceType>(i % 6);
            Color color = (i < 6) ? Color::WHITE : Color::BLACK;
            return Piece(type, color);
        }
    }
    return Piece(); // Empty piece
}

void Board::setPiece(Square sq, const Piece& piece) {
    clearSquare(sq);
    if (!piece.isEmpty()) {
        int index = getPieceIndex(piece.type, piece.color);
        pieceBitboards[index] = setBit(pieceBitboards[index], sq);
    }
    updateCombinedBitboards();
}

void Board::clearSquare(Square sq) {
    for (auto& bb : pieceBitboards) {
        bb = clearBit(bb, sq);
    }
    updateCombinedBitboards();
}

bool Board::canCastle(Color color, bool kingSide) const {
    int colorIndex = static_cast<int>(color);
    return kingSide ? canCastleKingSide[colorIndex] : canCastleQueenSide[colorIndex];
}

void Board::setCastlingRights(Color color, bool kingSide, bool canCastle) {
    int colorIndex = static_cast<int>(color);
    if (kingSide) {
        canCastleKingSide[colorIndex] = canCastle;
    } else {
        canCastleQueenSide[colorIndex] = canCastle;
    }
}

void Board::pushState() {
    GameState state;
    state.castlingRights[0] = canCastleKingSide[0];
    state.castlingRights[1] = canCastleQueenSide[0];
    state.castlingRights[2] = canCastleKingSide[1];
    state.castlingRights[3] = canCastleQueenSide[1];
    state.enPassantSquare = enPassantSquare;
    state.halfMoveClock = halfMoveClock;
    state.fullMoveNumber = fullMoveNumber;
    state.sideToMove = sideToMove;
    state.pieceBitboards = pieceBitboards;
    state.whitePieces = whitePieces;
    state.blackPieces = blackPieces;
    state.allPieces = allPieces;
    gameHistory.push_back(state);
}

void Board::popState() {
    const GameState& state = gameHistory.back();
    canCastleKingSide[0]  = state.castlingRights[0];
    canCastleQueenSide[0] = state.castlingRights[1];
    canCastleKingSide[1]  = state.castlingRights[2];
    canCastleQueenSide[1] = state.castlingRights[3];
    enPassantSquare = state.enPassantSquare;
    halfMoveClock   = state.halfMoveClock;
    fullMoveNumber  = state.fullMoveNumber;
    sideToMove      = state.sideToMove;
    pieceBitboards  = state.pieceBitboards;
    whitePieces     = state.whitePieces;
    blackPieces     = state.blackPieces;
    allPieces       = state.allPieces;
    gameHistory.pop_back();
}

void Board::makeMove(const Move& move) {
    pushState();

    Piece movingPiece   = pieceAt(move.from);
    bool  isCapture     = !pieceAt(move.to).isEmpty() || move.isEnPassant;

    clearSquare(move.from);

    if (move.isCastle) {
        if      (move.to == G1) { clearSquare(H1); setPiece(F1, Piece(PieceType::ROOK, Color::WHITE)); }
        else if (move.to == C1) { clearSquare(A1); setPiece(D1, Piece(PieceType::ROOK, Color::WHITE)); }
        else if (move.to == G8) { clearSquare(H8); setPiece(F8, Piece(PieceType::ROOK, Color::BLACK)); }
        else if (move.to == C8) { clearSquare(A8); setPiece(D8, Piece(PieceType::ROOK, Color::BLACK)); }
    }

    if (move.isEnPassant)
        clearSquare(sideToMove == Color::WHITE ? move.to - 8 : move.to + 8);

    PieceType finalType = (move.promotion != PieceType::NONE) ? move.promotion : movingPiece.type;
    setPiece(move.to, Piece(finalType, movingPiece.color));

    updateCastlingRights(move);
    updateEnPassant(move);

    halfMoveClock = (movingPiece.type == PieceType::PAWN || isCapture) ? 0 : halfMoveClock + 1;

    if (sideToMove == Color::BLACK) fullMoveNumber++;

    switchSideToMove();
    normalizeEnPassant();
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

void Board::unmakeMove(const Move& move) {
    (void)move; // full state is restored from history
    if (gameHistory.empty()) return;
    popState();
}

void Board::makeNullMove() {
    pushState();
    enPassantSquare = 64;
    halfMoveClock++;
    switchSideToMove();
}

void Board::unmakeNullMove() {
    if (gameHistory.empty()) return;
    popState();
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

bool Board::isRepetition() const {
    // Positions before the last irreversible move (pawn move / capture) can
    // never match, so only scan back as far as the half-move clock allows.
    int limit = std::min<int>(halfMoveClock, static_cast<int>(gameHistory.size()));
    for (int i = 1; i <= limit; i++) {
        const GameState& s = gameHistory[gameHistory.size() - i];
        if (s.sideToMove == sideToMove &&
            s.pieceBitboards == pieceBitboards &&
            s.enPassantSquare == enPassantSquare &&
            s.castlingRights[0] == canCastleKingSide[0] &&
            s.castlingRights[1] == canCastleQueenSide[0] &&
            s.castlingRights[2] == canCastleKingSide[1] &&
            s.castlingRights[3] == canCastleQueenSide[1])
            return true;
    }
    return false;
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

bool Board::isCheckmate() const {
    return isInCheck(sideToMove) && MoveGenerator::generateLegalMoves(*this).empty();
}

bool Board::isStalemate() const {
    return !isInCheck(sideToMove) && MoveGenerator::generateLegalMoves(*this).empty();
}

Square Board::findKing(Color color) const {
    int idx = getPieceIndex(PieceType::KING, color);
    return pieceBitboards[idx] ? firstSquare(pieceBitboards[idx]) : 64;
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

Bitboard Board::getPieceBitboard(PieceType type, Color color) const {
    return pieceBitboards[getPieceIndex(type, color)];
}

std::string Board::toFEN() const {
    std::ostringstream oss;

    // Piece placement (rank 8 down to rank 1)
    for (int rank = 7; rank >= 0; rank--) {
        int empty = 0;
        for (int file = 0; file < 8; file++) {
            Piece piece = pieceAt(makeSquare(file, rank));
            if (piece.isEmpty()) {
                empty++;
            } else {
                if (empty > 0) { oss << empty; empty = 0; }
                char c;
                switch (piece.type) {
                    case PieceType::PAWN:   c = 'p'; break;
                    case PieceType::KNIGHT: c = 'n'; break;
                    case PieceType::BISHOP: c = 'b'; break;
                    case PieceType::ROOK:   c = 'r'; break;
                    case PieceType::QUEEN:  c = 'q'; break;
                    case PieceType::KING:   c = 'k'; break;
                    default:                c = '?'; break;
                }
                if (piece.color == Color::WHITE) c = std::toupper(c);
                oss << c;
            }
        }
        if (empty > 0) oss << empty;
        if (rank > 0) oss << '/';
    }

    // Side to move
    oss << (sideToMove == Color::WHITE ? " w " : " b ");

    // Castling rights
    std::string castling;
    if (canCastleKingSide[0])  castling += 'K';
    if (canCastleQueenSide[0]) castling += 'Q';
    if (canCastleKingSide[1])  castling += 'k';
    if (canCastleQueenSide[1]) castling += 'q';
    oss << (castling.empty() ? "-" : castling);

    // En passant square
    if (enPassantSquare >= 64) {
        oss << " -";
    } else {
        oss << " " << static_cast<char>('a' + fileOf(enPassantSquare))
                   << static_cast<char>('1' + rankOf(enPassantSquare));
    }

    // Half-move clock and full-move number
    oss << " " << halfMoveClock << " " << fullMoveNumber;

    return oss.str();
}

bool Board::fromFEN(const std::string& fen) {
    std::istringstream iss(fen);
    std::string piecePlacement, sideStr, castlingStr, enPassantStr;
    int halfMove = 0, fullMove = 1;

    if (!(iss >> piecePlacement >> sideStr >> castlingStr >> enPassantStr >> halfMove >> fullMove)) {
        return false;
    }

    // Clear all bitboards
    for (auto& bb : pieceBitboards) bb = EMPTY_BOARD;

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
            int idx = static_cast<int>(type) + static_cast<int>(color) * 6;
            pieceBitboards[idx] = setBit(pieceBitboards[idx], makeSquare(file, rank));
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

    gameHistory.clear();
    updateCombinedBitboards();
    normalizeEnPassant();  // GUIs often send phantom ep squares in FEN
    return true;
}

int Board::getPieceIndex(PieceType type, Color color) const {
    return static_cast<int>(type) + static_cast<int>(color) * 6;
}

void Board::updateCombinedBitboards() {
    whitePieces = pieceBitboards[0] | pieceBitboards[1] | pieceBitboards[2] | 
                  pieceBitboards[3] | pieceBitboards[4] | pieceBitboards[5];
    blackPieces = pieceBitboards[6] | pieceBitboards[7] | pieceBitboards[8] | 
                  pieceBitboards[9] | pieceBitboards[10] | pieceBitboards[11];
    allPieces = whitePieces | blackPieces;
}

void Board::updateCastlingRights(const Move& move) {
    if (pieceAt(move.to).type == PieceType::KING) {
        setCastlingRights(sideToMove, true,  false);
        setCastlingRights(sideToMove, false, false);
    }
    if (move.from == A1 || move.to == A1) setCastlingRights(Color::WHITE, false, false);
    if (move.from == H1 || move.to == H1) setCastlingRights(Color::WHITE, true,  false);
    if (move.from == A8 || move.to == A8) setCastlingRights(Color::BLACK, false, false);
    if (move.from == H8 || move.to == H8) setCastlingRights(Color::BLACK, true,  false);
}

void Board::updateEnPassant(const Move& move) {
    enPassantSquare = 64;
    Piece moved = pieceAt(move.to);
    if (moved.type == PieceType::PAWN && std::abs(rankOf(move.to) - rankOf(move.from)) == 2)
        enPassantSquare = (move.from + move.to) / 2;
}

