#include "search.h"
#include "movegen.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>

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
        for (int c = 0; c < 2; c++) {
            for (int t = 0; t < 6; t++) {
                Bitboard bb = board.getPieceBitboard(static_cast<PieceType>(t),
                                                     static_cast<Color>(c));
                while (bb) {
                    Square sq = firstSquare(bb);
                    bb &= bb - 1;
                    h ^= ZOB_PIECES[c * 6 + t][sq];
                }
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

    // White-relative pawn structure score, split into middlegame and endgame
    // components (passed pawns are worth much more as material comes off).
    void evaluatePawnStructure(const Board& board, int& mg, int& eg) {
        Bitboard wp = board.getPieceBitboard(PieceType::PAWN, Color::WHITE);
        Bitboard bp = board.getPieceBitboard(PieceType::PAWN, Color::BLACK);

        for (int file = 0; file < 8; file++) {
            Bitboard fb = fileBB(file);
            int wc = popCount(wp & fb);
            int bc = popCount(bp & fb);

            // Doubled pawns
            if (wc > 1) { mg -= 20 * (wc - 1); eg -= 20 * (wc - 1); }
            if (bc > 1) { mg += 20 * (bc - 1); eg += 20 * (bc - 1); }

            // Isolated pawns (no friendly pawn on adjacent files)
            Bitboard adj = 0;
            if (file > 0) adj |= fileBB(file - 1);
            if (file < 7) adj |= fileBB(file + 1);

            if (wc > 0 && !(wp & adj)) { mg -= 15 * wc; eg -= 15 * wc; }
            if (bc > 0 && !(bp & adj)) { mg += 15 * bc; eg += 15 * bc; }
        }

        // Passed pawns
        static const int passedMG[8] = {0, 10, 20, 35, 55,  80, 110, 0};
        static const int passedEG[8] = {0, 20, 35, 60, 95, 140, 200, 0};

        Bitboard w = wp;
        while (w) {
            Square sq = firstSquare(w); w &= w - 1;
            int f = fileOf(sq), r = rankOf(sq);
            Bitboard adjFiles = fileBB(f);
            if (f > 0) adjFiles |= fileBB(f - 1);
            if (f < 7) adjFiles |= fileBB(f + 1);
            if (!(bp & adjFiles & ranksAbove(r))) { mg += passedMG[r]; eg += passedEG[r]; }
        }

        Bitboard b = bp;
        while (b) {
            Square sq = firstSquare(b); b &= b - 1;
            int f = fileOf(sq), r = rankOf(sq);
            Bitboard adjFiles = fileBB(f);
            if (f > 0) adjFiles |= fileBB(f - 1);
            if (f < 7) adjFiles |= fileBB(f + 1);
            if (!(wp & adjFiles & ranksBelow(r))) { mg -= passedMG[7 - r]; eg -= passedEG[7 - r]; }
        }
    }

    // Game phase: 24 = all minor/major pieces on the board (middlegame),
    // 0 = bare kings and pawns (pure endgame).
    int gamePhase(const Board& board) {
        int phase = 0;
        for (Color c : {Color::WHITE, Color::BLACK}) {
            phase += popCount(board.getPieceBitboard(PieceType::KNIGHT, c));
            phase += popCount(board.getPieceBitboard(PieceType::BISHOP, c));
            phase += popCount(board.getPieceBitboard(PieceType::ROOK,   c)) * 2;
            phase += popCount(board.getPieceBitboard(PieceType::QUEEN,  c)) * 4;
        }
        return std::min(phase, 24);
    }

    // Endgame mop-up: when one side is a rook or more ahead and the defender
    // has no pawns, reward driving the enemy king to the edge and bringing
    // our king close so basic mates (KR-K, KQ-K) get converted.
    int mopUpBonus(Square winnerKing, Square loserKing) {
        int lf = fileOf(loserKing), lr = rankOf(loserKing);
        int centerDist = std::max(3 - std::min(lf, 7 - lf), 0) +
                         std::max(3 - std::min(lr, 7 - lr), 0);
        int kingDist = std::abs(fileOf(winnerKing) - lf) +
                       std::abs(rankOf(winnerKing) - lr);
        return 10 * centerDist + 4 * (14 - kingDist);
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

namespace {
    std::string moveToUci(const Move& move) {
        std::string s;
        s += static_cast<char>('a' + fileOf(move.from));
        s += static_cast<char>('1' + rankOf(move.from));
        s += static_cast<char>('a' + fileOf(move.to));
        s += static_cast<char>('1' + rankOf(move.to));
        switch (move.promotion) {
            case PieceType::QUEEN:  s += 'q'; break;
            case PieceType::ROOK:   s += 'r'; break;
            case PieceType::BISHOP: s += 'b'; break;
            case PieceType::KNIGHT: s += 'n'; break;
            default: break;
        }
        return s;
    }
}

SearchEngine::SearchEngine()
    : maxDepth(8), timeLimit(5000), nodeLimit(0), nodesSearched(0),
      currentDepth(0), quietMode(false), useOpeningBook(false) {
    initZobrist();
    tt.resize(TT_SIZE);
    newGame();
}

void SearchEngine::newGame() {
    for (auto& e : tt) e = TTEntry();
    for (int i = 0; i < 32; i++)
        for (int j = 0; j < MAX_KILLER_MOVES; j++)
            killerMoves[i][j] = Move();
    for (int i = 0; i < 64; i++)
        for (int j = 0; j < 64; j++)
            historyTable[i][j] = 0;
}

bool SearchEngine::isTimeUp() const {
    if (stopFlag && stopFlag->load(std::memory_order_relaxed)) return true;
    if (nodeLimit > 0 && nodesSearched >= nodeLimit) return true;
    if (timeLimit <= 0) return false;
    if (timeUpFlag) return true;
    // Only consult the clock every 1024 nodes; it is expensive per-node.
    if ((nodesSearched & 1023) != 0) return false;
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - searchStart).count();
    timeUpFlag = elapsed >= timeLimit;
    return timeUpFlag;
}

SearchResult SearchEngine::search(const Board& board, int depth) {
    SearchResult result;
    nodesSearched = 0;
    currentDepth  = 0;
    timeUpFlag    = false;
    searchStart   = std::chrono::steady_clock::now();

    std::vector<Move> moves = MoveGenerator::generateLegalMoves(board);
    if (moves.empty()) {
        result.score = board.isInCheck(board.getSideToMove()) ? -MATE_SCORE : DRAW_SCORE;
        return result;
    }

    // Opening book: pick a random weighted book move, but only play it if it
    // matches a legal move (the legal move carries the correct flags).
    if (useOpeningBook && bookEnabled) {
        Move bookMove = openingBook.getRandomMove(board);
        if (bookMove.from != bookMove.to) {
            for (const Move& m : moves) {
                if (m.from == bookMove.from && m.to == bookMove.to &&
                    m.promotion == bookMove.promotion) {
                    result.bestMove = m;
                    return result;
                }
            }
        }
    }

    Move bestMove = moves[0];
    int bestScore = 0;

    if (moves.size() == 1) {
        result.bestMove = bestMove;
        return result;
    }

    Board mutableBoard = board;

    for (int d = 1; d <= depth; d++) {
        currentDepth = d;
        // Previous iteration's best move is searched first.
        orderMoves(mutableBoard, moves, bestMove, d);

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

            if (isTimeUp()) break;  // scores from an aborted search are garbage

            if (score > iterBestScore) { iterBestScore = score; iterBest = moves[i]; }
            if (score > alpha) alpha = score;
        }

        if ((!isTimeUp() || d == 1) &&
            iterBestScore != std::numeric_limits<int>::min()) {
            bestMove  = iterBest;
            bestScore = iterBestScore;
        }
        if (isTimeUp()) break;

        if (!quietMode) {
            // Principal variation: best root move, then follow TT best moves
            std::string pv = moveToUci(bestMove);
            Board pvBoard = board;
            pvBoard.makeMove(bestMove);
            for (int len = 1; len < d; len++) {
                uint64_t h = computeHash(pvBoard);
                const TTEntry& e = tt[h & (TT_SIZE - 1)];
                if (e.hash != h) break;
                bool extended = false;
                for (const Move& m : MoveGenerator::generateLegalMoves(pvBoard)) {
                    if (m.from == e.bestMove.from && m.to == e.bestMove.to &&
                        m.promotion == e.bestMove.promotion) {
                        pv += " " + moveToUci(m);
                        pvBoard.makeMove(m);
                        extended = true;
                        break;
                    }
                }
                if (!extended) break;
            }

            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - searchStart).count();
            long long nps = (ms > 0) ? nodesSearched * 1000LL / ms : 0;

            std::cout << "info depth " << d;
            if (bestScore > MATE_SCORE - 1000 || bestScore < -(MATE_SCORE - 1000)) {
                int plies = std::max(1, d - (MATE_SCORE - std::abs(bestScore)));
                int mateIn = (plies + 1) / 2;
                std::cout << " score mate " << (bestScore > 0 ? mateIn : -mateIn);
            } else {
                std::cout << " score cp " << bestScore;
            }
            std::cout << " time " << ms << " nodes " << nodesSearched
                      << " nps " << nps << " pv " << pv << std::endl;
        }

        // Forced mate found: deeper search cannot improve it.
        if (bestScore > MATE_SCORE - 100 || bestScore < -(MATE_SCORE - 100)) break;
    }

    result.bestMove      = bestMove;
    result.score         = bestScore;
    result.depth         = currentDepth;
    result.nodesSearched = nodesSearched;

    return result;
}

int SearchEngine::alphaBeta(Board& board, int depth, int alpha, int beta, bool nullMoveAllowed) {
    if (isTimeUp()) return 0;
    nodesSearched++;

    bool inCheck = board.isInCheck(board.getSideToMove());

    // Draw detection: repetition, dead material, and fifty-move rule (unless
    // in check, where the mating side may still deliver mate on this move).
    if (board.isRepetition() || board.isInsufficientMaterial()) return DRAW_SCORE;
    if (board.isDrawByFiftyMoves() && !inCheck) return DRAW_SCORE;

    // Check extension: never drop into quiescence while in check
    if (inCheck) depth++;

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

    orderMoves(board, moves, hasTTMove ? ttMove : Move(), depth);

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

    // Don't store aborted searches, and don't store mate scores: they encode
    // distance-to-mate relative to this node and are wrong elsewhere.
    if (!isTimeUp() && bestScore < MATE_SCORE - 100 && bestScore > -(MATE_SCORE - 100)) {
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

    if (isTimeUp()) return alpha;

    std::vector<Move> allMoves = MoveGenerator::generateLegalMoves(board);
    std::vector<Move> captures;
    captures.reserve(allMoves.size());
    for (const Move& m : allMoves)
        if (m.isCapture || m.promotion != PieceType::NONE)
            captures.push_back(m);

    orderMoves(board, captures, Move(), 0);

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
    // Positions where no side can ever mate are dead draws
    if (board.isInsufficientMaterial()) return 0;

    // Tapered eval: score the position from a middlegame and an endgame
    // perspective and blend by how much material is left, so e.g. the king
    // hides behind pawns early but centralizes once the queens come off.
    int mg = 0, eg = 0;
    int materialW = 0, materialB = 0;

    for (int c = 0; c < 2; c++) {
        Color color = static_cast<Color>(c);
        int sign = (color == Color::WHITE) ? 1 : -1;
        for (int t = 0; t < 6; t++) {
            PieceType type = static_cast<PieceType>(t);
            Bitboard bb = board.getPieceBitboard(type, color);
            while (bb) {
                Square sq = firstSquare(bb);
                bb &= bb - 1;
                int value = getPieceValue(type);
                mg += sign * (value + getPositionalValue(type, sq, color, false));
                eg += sign * (value + getPositionalValue(type, sq, color, true));
                if (type != PieceType::KING)
                    (color == Color::WHITE ? materialW : materialB) += value;
            }
        }
    }

    // Bishop pair
    if (popCount(board.getPieceBitboard(PieceType::BISHOP, Color::WHITE)) >= 2) { mg += 30; eg += 30; }
    if (popCount(board.getPieceBitboard(PieceType::BISHOP, Color::BLACK)) >= 2) { mg -= 30; eg -= 30; }

    // Mobility (attacked squares per side)
    int mobility = (board.countAttackedSquares(Color::WHITE) -
                    board.countAttackedSquares(Color::BLACK)) * 3;
    mg += mobility;
    eg += mobility;

    // Pawn structure
    evaluatePawnStructure(board, mg, eg);

    // King safety matters while there is attacking material; fade it out
    mg += evaluateKingSafety(board);

    // Mop-up knowledge for converting big material advantages without pawns
    Square wk = board.findKing(Color::WHITE);
    Square bk = board.findKing(Color::BLACK);
    if (wk < 64 && bk < 64) {
        if (materialW - materialB >= 400 && board.getPieceBitboard(PieceType::PAWN, Color::BLACK) == 0)
            eg += mopUpBonus(wk, bk);
        else if (materialB - materialW >= 400 && board.getPieceBitboard(PieceType::PAWN, Color::WHITE) == 0)
            eg -= mopUpBonus(bk, wk);
    }

    int phase = gamePhase(board);
    return (mg * phase + eg * (24 - phase)) / 24;
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

int SearchEngine::getPositionalValue(PieceType type, Square square, Color color, bool endgame) {
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
            // Endgame: only advancement matters, and it matters a lot
            static const int pawnTableEG[64] = {
                  0,   0,   0,   0,   0,   0,   0,   0,
                 10,  10,  10,  10,  10,  10,  10,  10,
                 10,  10,  10,  10,  10,  10,  10,  10,
                 20,  20,  20,  20,  20,  20,  20,  20,
                 35,  35,  35,  35,  35,  35,  35,  35,
                 60,  60,  60,  60,  60,  60,  60,  60,
                100, 100, 100, 100, 100, 100, 100, 100,
                  0,   0,   0,   0,   0,   0,   0,   0
            };
            return endgame ? pawnTableEG[idx] : pawnTable[idx];
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
            // Middlegame: stay castled behind the pawns
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
            // Endgame: the king is a fighting piece — centralize it
            static const int kingTableEG[64] = {
                -50,-40,-30,-20,-20,-30,-40,-50,
                -30,-20,-10,  0,  0,-10,-20,-30,
                -30,-10, 20, 30, 30, 20,-10,-30,
                -30,-10, 30, 40, 40, 30,-10,-30,
                -30,-10, 30, 40, 40, 30,-10,-30,
                -30,-10, 20, 30, 30, 20,-10,-30,
                -30,-30,  0,  0,  0,  0,-30,-30,
                -50,-30,-30,-30,-30,-30,-30,-50
            };
            return endgame ? kingTableEG[idx] : kingTable[idx];
        }
        default: return 0;
    }
}

void SearchEngine::orderMoves(const Board& board, std::vector<Move>& moves,
                              const Move& ttMove, int depth) {
    bool hasTTMove = ttMove.from != ttMove.to;

    // Score each move once, then sort by score.
    std::vector<std::pair<int, Move>> scored;
    scored.reserve(moves.size());
    for (const Move& m : moves) {
        int s;
        if (hasTTMove && m.from == ttMove.from && m.to == ttMove.to &&
            m.promotion == ttMove.promotion) {
            s = 1000000;                                     // hash/PV move first
        } else if (m.isCapture) {
            PieceType victim = m.isEnPassant ? PieceType::PAWN
                                             : board.pieceAt(m.to).type;
            s = 100000 + getPieceValue(victim) * 10          // MVV-LVA
                       - getPieceValue(board.pieceAt(m.from).type);
        } else if (m.promotion != PieceType::NONE) {
            s = 90000 + getPieceValue(m.promotion);
        } else if (isKillerMove(m, depth)) {
            s = 80000;
        } else {
            s = std::min(getHistoryScore(m), 79000);
        }
        scored.emplace_back(s, m);
    }

    std::stable_sort(scored.begin(), scored.end(),
                     [](const auto& a, const auto& b) { return a.first > b.first; });
    for (size_t i = 0; i < moves.size(); i++) moves[i] = scored[i].second;
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

bool SearchEngine::loadEmbeddedOpeningBook() {
    useOpeningBook = openingBook.loadEmbedded();
    return useOpeningBook;
}
