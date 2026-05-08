#include "search.h"
#include "movegen.h"
#include <algorithm>
#include <iostream>

// ---------------------------------------------------------------------------
// Zobrist hashing (file-local)
// ---------------------------------------------------------------------------
namespace {
    uint64_t ZOB_PIECES[12][64];
    uint64_t ZOB_SIDE;
    uint64_t ZOB_CASTLE[4];
    uint64_t ZOB_EP[8];

    void initZobrist() {
        uint64_t s = 0x9E3779B97F4A7C15ULL;
        auto rng = [&]() -> uint64_t {
            s += 0x9E3779B97F4A7C15ULL;
            uint64_t z = s;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
            return z ^ (z >> 31);
        };
        for (int p = 0; p < 12; p++)
            for (int sq = 0; sq < 64; sq++)
                ZOB_PIECES[p][sq] = rng();
        ZOB_SIDE = rng();
        for (int i = 0; i < 4; i++) ZOB_CASTLE[i] = rng();
        for (int i = 0; i < 8; i++) ZOB_EP[i]     = rng();
    }

    uint64_t computeHash(const Board& board) {
        uint64_t h = 0;
        for (Square sq = 0; sq < 64; sq++) {
            Piece p = board.pieceAt(sq);
            if (!p.isEmpty()) {
                int idx = static_cast<int>(p.color) * 6 + static_cast<int>(p.type);
                h ^= ZOB_PIECES[idx][sq];
            }
        }
        if (board.getSideToMove() == Color::BLACK) h ^= ZOB_SIDE;
        if (board.canCastle(Color::WHITE, true))   h ^= ZOB_CASTLE[0];
        if (board.canCastle(Color::WHITE, false))  h ^= ZOB_CASTLE[1];
        if (board.canCastle(Color::BLACK, true))   h ^= ZOB_CASTLE[2];
        if (board.canCastle(Color::BLACK, false))  h ^= ZOB_CASTLE[3];
        Square ep = board.getEnPassantSquare();
        if (ep < 64) h ^= ZOB_EP[fileOf(ep)];
        return h;
    }

    // File mask: all squares on a given file
    constexpr Bitboard fileBB(int file) { return 0x0101010101010101ULL << file; }

    // All squares strictly above rank r
    constexpr Bitboard ranksAbove(int r) {
        return (r < 7) ? ~((1ULL << ((r + 1) * 8)) - 1) : 0ULL;
    }

    // All squares strictly below rank r
    constexpr Bitboard ranksBelow(int r) {
        return (r > 0) ? ((1ULL << (r * 8)) - 1) : 0ULL;
    }

    // White-relative pawn structure score
    int evaluatePawnStructure(const Board& board) {
        int score = 0;
        Bitboard wp = board.getPieceBitboard(PieceType::PAWN, Color::WHITE);
        Bitboard bp = board.getPieceBitboard(PieceType::PAWN, Color::BLACK);

        for (int file = 0; file < 8; file++) {
            Bitboard fb = fileBB(file);
            int wc = popCount(wp & fb);
            int bc = popCount(bp & fb);

            // Doubled pawns
            if (wc > 1) score -= 20 * (wc - 1);
            if (bc > 1) score += 20 * (bc - 1);

            // Isolated pawns (no friendly pawn on adjacent files)
            Bitboard adj = 0;
            if (file > 0) adj |= fileBB(file - 1);
            if (file < 7) adj |= fileBB(file + 1);

            if (wc > 0 && !(wp & adj)) score -= 15 * wc;
            if (bc > 0 && !(bp & adj)) score += 15 * bc;
        }

        // Passed pawns
        static const int passedBonus[8] = {0, 10, 20, 35, 55, 80, 110, 0};

        Bitboard w = wp;
        while (w) {
            Square sq = firstSquare(w); w &= w - 1;
            int f = fileOf(sq), r = rankOf(sq);
            Bitboard adjFiles = fileBB(f);
            if (f > 0) adjFiles |= fileBB(f - 1);
            if (f < 7) adjFiles |= fileBB(f + 1);
            if (!(bp & adjFiles & ranksAbove(r))) score += passedBonus[r];
        }

        Bitboard b = bp;
        while (b) {
            Square sq = firstSquare(b); b &= b - 1;
            int f = fileOf(sq), r = rankOf(sq);
            Bitboard adjFiles = fileBB(f);
            if (f > 0) adjFiles |= fileBB(f - 1);
            if (f < 7) adjFiles |= fileBB(f + 1);
            if (!(wp & adjFiles & ranksBelow(r))) score -= passedBonus[7 - r];
        }

        return score;
    }

    // White-relative king safety score
    int evaluateKingSafety(const Board& board) {
        int score = 0;

        auto pawnShield = [&](Color color) -> int {
            Square king = board.findKing(color);
            if (king >= 64) return 0;
            int kf = fileOf(king), kr = rankOf(king);
            // Only score when king is on the wing (castled position)
            if (kf >= 2 && kf <= 5) return 0;

            Bitboard pawns = board.getPieceBitboard(PieceType::PAWN, color);
            int shield = 0;
            int dir = (color == Color::WHITE) ? 1 : -1;
            for (int f = std::max(0, kf - 1); f <= std::min(7, kf + 1); f++) {
                bool found = false;
                for (int d = 1; d <= 2 && !found; d++) {
                    int r = kr + d * dir;
                    if (r < 0 || r >= 8) break;
                    if (getBit(pawns, makeSquare(f, r))) {
                        shield += (d == 1) ? 15 : 8;
                        found = true;
                    }
                }
                if (!found) shield -= 10; // open file in front of king
            }
            return shield;
        };

        score += pawnShield(Color::WHITE);
        score -= pawnShield(Color::BLACK);
        return score;
    }
}

// ---------------------------------------------------------------------------

SearchEngine::SearchEngine()
    : maxDepth(8), timeLimit(5000), nodesSearched(0), currentDepth(0),
      useOpeningBook(false), quietMode(false) {
    initZobrist();
    tt.resize(TT_SIZE);
    for (int i = 0; i < 32; i++)
        for (int j = 0; j < MAX_KILLER_MOVES; j++)
            killerMoves[i][j] = Move();
    for (int i = 0; i < 64; i++)
        for (int j = 0; j < 64; j++)
            historyTable[i][j] = 0;
}

bool SearchEngine::isTimeUp() const {
    if (stopFlag && stopFlag->load(std::memory_order_relaxed)) return true;
    if (timeLimit <= 0) return false;
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - searchStart).count();
    return elapsed >= timeLimit;
}

SearchResult SearchEngine::search(const Board& board, int depth) {
    SearchResult result;
    nodesSearched = 0;

    if (useOpeningBook) {
        Move bookMove = openingBook.getRandomMove(board);
        if (bookMove.from != bookMove.to || bookMove.from != 0) {
            result.bestMove = bookMove;
            return result;
        }
    }

    std::vector<Move> moves = MoveGenerator::generateLegalMoves(board);
    if (moves.empty()) {
        result.score = board.isInCheck(board.getSideToMove()) ? -MATE_SCORE : DRAW_SCORE;
        return result;
    }

    Board mutableBoard = board;
    searchStart = std::chrono::steady_clock::now();

    Move bestMove = moves[0];
    int bestScore = 0;

    for (int d = 1; d <= depth; d++) {
        currentDepth = d;
        orderMoves(mutableBoard, moves);

        int alpha = -(MATE_SCORE + 1);
        int beta  =  (MATE_SCORE + 1);
        Move iterBest = moves[0];
        int iterBestScore = std::numeric_limits<int>::min();

        for (size_t i = 0; i < moves.size(); i++) {
            mutableBoard.makeMove(moves[i]);
            int score;
            if (i == 0) {
                score = -alphaBeta(mutableBoard, d - 1, -beta, -alpha, true);
            } else {
                score = -alphaBeta(mutableBoard, d - 1, -alpha - 1, -alpha, true);
                if (score > alpha && score < beta)
                    score = -alphaBeta(mutableBoard, d - 1, -beta, -alpha, true);
            }
            mutableBoard.unmakeMove(moves[i]);

            if (score > iterBestScore) { iterBestScore = score; iterBest = moves[i]; }
            if (score > alpha) alpha = score;
        }

        if (!isTimeUp() || d == 1) {
            bestMove  = iterBest;
            bestScore = iterBestScore;
        }
        if (isTimeUp()) break;
    }

    result.bestMove     = bestMove;
    result.score        = bestScore;
    result.depth        = depth;
    result.nodesSearched = nodesSearched;

    if (!quietMode)
        std::cout << "info depth " << depth << " score cp " << bestScore
                  << " nodes " << nodesSearched << std::endl;

    return result;
}

int SearchEngine::alphaBeta(Board& board, int depth, int alpha, int beta, bool nullMoveAllowed) {
    if (isTimeUp()) return 0;
    nodesSearched++;

    if (depth == 0) return quiescence(board, alpha, beta);

    // TT probe
    uint64_t hash  = computeHash(board);
    int      ttIdx = static_cast<int>(hash & (TT_SIZE - 1));
    TTEntry& entry = tt[ttIdx];
    Move ttMove;
    bool hasTTMove = false;

    if (entry.hash == hash) {
        hasTTMove = true;
        ttMove    = entry.bestMove;
        if (entry.depth >= static_cast<int8_t>(depth)) {
            if (entry.flag == 0) return entry.score;
            if (entry.flag == 1 && entry.score >= beta)  return entry.score;
            if (entry.flag == 2 && entry.score <= alpha) return entry.score;
        }
    }

    bool inCheck = board.isInCheck(board.getSideToMove());

    // Null move pruning
    if (nullMoveAllowed && !inCheck && depth >= 3) {
        Color stm = board.getSideToMove();
        Bitboard mine = (stm == Color::WHITE) ? board.getWhitePieces() : board.getBlackPieces();
        Bitboard pawns = board.getPieceBitboard(PieceType::PAWN, stm);
        Bitboard king  = board.getPieceBitboard(PieceType::KING, stm);
        if (mine & ~pawns & ~king) {
            int R = (depth >= 6) ? 3 : 2;
            board.makeNullMove();
            int nullScore = -alphaBeta(board, depth - R - 1, -beta, -beta + 1, false);
            board.unmakeNullMove();
            if (nullScore >= beta) return beta;
        }
    }

    // Static eval for futility pruning (compute once, before move loop)
    int staticEval = 0;
    bool doFutility = !inCheck && depth <= 2;
    if (doFutility) {
        staticEval = evaluate(board);
        if (board.getSideToMove() == Color::BLACK) staticEval = -staticEval;
    }

    std::vector<Move> moves = MoveGenerator::generateLegalMoves(board);
    if (moves.empty())
        return inCheck ? -(MATE_SCORE - depth) : DRAW_SCORE;

    // TT best move to front
    if (hasTTMove) {
        for (size_t i = 1; i < moves.size(); i++) {
            if (moves[i].from == ttMove.from && moves[i].to == ttMove.to &&
                moves[i].promotion == ttMove.promotion) {
                std::swap(moves[i], moves[0]);
                break;
            }
        }
    }
    orderMoves(board, moves);

    int  originalAlpha = alpha;
    Move bestMove      = moves[0];
    int  bestScore     = std::numeric_limits<int>::min();

    for (size_t i = 0; i < moves.size(); i++) {
        const Move& move = moves[i];
        bool isQuiet = !move.isCapture && move.promotion == PieceType::NONE;

        // Futility pruning: skip quiet moves when static eval + margin can't beat alpha
        if (doFutility && isQuiet && i > 0) {
            int margin = (depth == 1) ? 100 : 300;
            if (staticEval + margin <= alpha) continue;
        }

        board.makeMove(move);
        int score;

        if (i == 0) {
            // First move: full window
            score = -alphaBeta(board, depth - 1, -beta, -alpha, true);
        } else {
            // LMR: reduce quiet moves that are ordered late
            int reduction = 0;
            if (depth >= 3 && i >= 3 && isQuiet && !inCheck)
                reduction = 1 + (i >= 6 && depth >= 4 ? 1 : 0);

            // PVS: null window first
            score = -alphaBeta(board, depth - 1 - reduction, -alpha - 1, -alpha, true);

            // Re-search full window if it beat alpha (or was reduced)
            if (score > alpha && (reduction > 0 || score < beta))
                score = -alphaBeta(board, depth - 1, -beta, -alpha, true);
        }

        board.unmakeMove(move);

        if (score > bestScore) { bestScore = score; bestMove = move; }
        if (score > alpha) alpha = score;
        if (alpha >= beta) {
            if (isQuiet) { recordKillerMove(move, depth); recordHistoryMove(move, depth); }
            break;
        }
    }

    if (!isTimeUp()) {
        int8_t flag = (bestScore >= beta) ? 1 : (bestScore <= originalAlpha ? 2 : 0);
        entry = {hash, bestScore, static_cast<int8_t>(depth), flag, bestMove};
    }

    return bestScore;
}

int SearchEngine::quiescence(Board& board, int alpha, int beta) {
    nodesSearched++;

    int standPat = evaluate(board);
    if (board.getSideToMove() == Color::BLACK) standPat = -standPat;

    if (standPat >= beta) return beta;
    if (standPat > alpha) alpha = standPat;

    std::vector<Move> allMoves = MoveGenerator::generateLegalMoves(board);
    std::vector<Move> captures;
    captures.reserve(allMoves.size());
    for (const Move& m : allMoves)
        if (m.isCapture || m.promotion != PieceType::NONE)
            captures.push_back(m);

    orderMoves(board, captures);

    for (const Move& move : captures) {
        board.makeMove(move);
        int score = -quiescence(board, -beta, -alpha);
        board.unmakeMove(move);
        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }

    return alpha;
}

int SearchEngine::evaluate(const Board& board) {
    int score = 0;

    // Material + piece-square tables
    for (Square sq = 0; sq < 64; sq++) {
        const Piece& piece = board.pieceAt(sq);
        if (!piece.isEmpty()) {
            int value = getPieceValue(piece.type) + getPositionalValue(piece.type, sq, piece.color);
            score += (piece.color == Color::WHITE) ? value : -value;
        }
    }

    // Mobility (attacked squares per side)
    score += (board.countAttackedSquares(Color::WHITE) -
              board.countAttackedSquares(Color::BLACK)) * 3;

    // Pawn structure
    score += evaluatePawnStructure(board);

    // King safety
    score += evaluateKingSafety(board);

    return score;
}

int SearchEngine::getPieceValue(PieceType type) {
    switch (type) {
        case PieceType::PAWN:   return 100;
        case PieceType::KNIGHT: return 320;
        case PieceType::BISHOP: return 330;
        case PieceType::ROOK:   return 500;
        case PieceType::QUEEN:  return 900;
        case PieceType::KING:   return 20000;
        default: return 0;
    }
}

int SearchEngine::getPositionalValue(PieceType type, Square square, Color color) {
    int file = fileOf(square);
    int rank = rankOf(square);
    int tableRank = (color == Color::BLACK) ? (7 - rank) : rank;
    int idx = tableRank * 8 + file;

    switch (type) {
        case PieceType::PAWN: {
            static const int pawnTable[64] = {
                 0,  0,  0,  0,  0,  0,  0,  0,
                 5, 10, 10,-20,-20, 10, 10,  5,
                 5, -5,-10,  0,  0,-10, -5,  5,
                 0,  0,  0, 20, 20,  0,  0,  0,
                 5,  5, 10, 25, 25, 10,  5,  5,
                10, 10, 20, 30, 30, 20, 10, 10,
                50, 50, 50, 50, 50, 50, 50, 50,
                 0,  0,  0,  0,  0,  0,  0,  0
            };
            return pawnTable[idx];
        }
        case PieceType::KNIGHT: {
            static const int knightTable[64] = {
                -50,-40,-30,-30,-30,-30,-40,-50,
                -40,-20,  0,  5,  5,  0,-20,-40,
                -30,  5, 10, 15, 15, 10,  5,-30,
                -30,  0, 15, 20, 20, 15,  0,-30,
                -30,  5, 15, 20, 20, 15,  5,-30,
                -30,  0, 10, 15, 15, 10,  0,-30,
                -40,-20,  0,  0,  0,  0,-20,-40,
                -50,-40,-30,-30,-30,-30,-40,-50
            };
            return knightTable[idx];
        }
        case PieceType::BISHOP: {
            static const int bishopTable[64] = {
                -20,-10,-10,-10,-10,-10,-10,-20,
                -10,  5,  0,  0,  0,  0,  5,-10,
                -10, 10, 10, 10, 10, 10, 10,-10,
                -10,  0, 10, 10, 10, 10,  0,-10,
                -10,  5,  5, 10, 10,  5,  5,-10,
                -10,  0,  5, 10, 10,  5,  0,-10,
                -10,  0,  0,  0,  0,  0,  0,-10,
                -20,-10,-10,-10,-10,-10,-10,-20
            };
            return bishopTable[idx];
        }
        case PieceType::ROOK: {
            static const int rookTable[64] = {
                 0,  0,  0,  5,  5,  0,  0,  0,
                -5,  0,  0,  0,  0,  0,  0, -5,
                -5,  0,  0,  0,  0,  0,  0, -5,
                -5,  0,  0,  0,  0,  0,  0, -5,
                -5,  0,  0,  0,  0,  0,  0, -5,
                -5,  0,  0,  0,  0,  0,  0, -5,
                 5, 10, 10, 10, 10, 10, 10,  5,
                 0,  0,  0,  0,  0,  0,  0,  0
            };
            return rookTable[idx];
        }
        case PieceType::QUEEN: {
            static const int queenTable[64] = {
                -20,-10,-10, -5, -5,-10,-10,-20,
                -10,  0,  5,  0,  0,  0,  0,-10,
                -10,  5,  5,  5,  5,  5,  0,-10,
                  0,  0,  5,  5,  5,  5,  0, -5,
                 -5,  0,  5,  5,  5,  5,  0, -5,
                -10,  0,  5,  5,  5,  5,  0,-10,
                -10,  0,  0,  0,  0,  0,  0,-10,
                -20,-10,-10, -5, -5,-10,-10,-20
            };
            return queenTable[idx];
        }
        case PieceType::KING: {
            static const int kingTable[64] = {
                 20, 30, 10,  0,  0, 10, 30, 20,
                 20, 20,  0,  0,  0,  0, 20, 20,
                -10,-20,-20,-20,-20,-20,-20,-10,
                -20,-30,-30,-40,-40,-30,-30,-20,
                -30,-40,-40,-50,-50,-40,-40,-30,
                -30,-40,-40,-50,-50,-40,-40,-30,
                -30,-40,-40,-50,-50,-40,-40,-30,
                -30,-40,-40,-50,-50,-40,-40,-30
            };
            return kingTable[idx];
        }
        default: return 0;
    }
}

void SearchEngine::orderMoves(const Board& board, std::vector<Move>& moves) {
    std::sort(moves.begin(), moves.end(), [&](const Move& a, const Move& b) {
        if (useOpeningBook) {
            bool aInBook = isMoveInOpeningBook(board, a);
            bool bInBook = isMoveInOpeningBook(board, b);
            if (aInBook != bInBook) return aInBook;
        }

        if (a.isCapture != b.isCapture) return a.isCapture;

        if (a.isCapture && b.isCapture) {
            int aScore = getPieceValue(board.pieceAt(a.to).type) * 10 - getPieceValue(board.pieceAt(a.from).type);
            int bScore = getPieceValue(board.pieceAt(b.to).type) * 10 - getPieceValue(board.pieceAt(b.from).type);
            if (aScore != bScore) return aScore > bScore;
        }

        bool aKiller = isKillerMove(a, currentDepth);
        bool bKiller = isKillerMove(b, currentDepth);
        if (aKiller != bKiller) return aKiller;

        return getHistoryScore(a) > getHistoryScore(b);
    });
}

bool SearchEngine::isKillerMove(const Move& move, int depth) {
    if (depth < 0 || depth >= 32) return false;
    for (int i = 0; i < MAX_KILLER_MOVES; i++)
        if (killerMoves[depth][i].from == move.from &&
            killerMoves[depth][i].to   == move.to   &&
            killerMoves[depth][i].promotion == move.promotion) return true;
    return false;
}

int SearchEngine::getHistoryScore(const Move& move) {
    if (move.from >= 64 || move.to >= 64) return 0;
    return historyTable[move.from][move.to];
}

void SearchEngine::recordKillerMove(const Move& move, int depth) {
    if (depth < 0 || depth >= 32 || move.isCapture) return;
    for (int i = MAX_KILLER_MOVES - 1; i > 0; i--)
        killerMoves[depth][i] = killerMoves[depth][i - 1];
    killerMoves[depth][0] = move;
}

void SearchEngine::recordHistoryMove(const Move& move, int depth) {
    if (move.from >= 64 || move.to >= 64) return;
    historyTable[move.from][move.to] += depth * depth;
    if (historyTable[move.from][move.to] > 1000000)
        for (int i = 0; i < 64; i++)
            for (int j = 0; j < 64; j++)
                historyTable[i][j] /= 2;
}

bool SearchEngine::loadOpeningBook(const std::string& filename) {
    useOpeningBook = openingBook.loadFromFile(filename);
    return useOpeningBook;
}

bool SearchEngine::isMoveInOpeningBook(const Board& board, const Move& move) {
    if (!useOpeningBook) return false;
    for (const auto& bookMove : openingBook.getMoves(board))
        if (bookMove.move.from == move.from &&
            bookMove.move.to   == move.to   &&
            bookMove.move.promotion == move.promotion) return true;
    return false;
}
