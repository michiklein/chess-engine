#include "search.h"
#include "movegen.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {
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

    // Every square attacked by a pawn of the given colour.
    Bitboard pawnCover(const Board& board, Color c) {
        Bitboard bb = board.getPieceBitboard(PieceType::PAWN, c), out = 0;
        while (bb) {
            Square s = firstSquare(bb); bb &= bb - 1;
            out |= MoveGenerator::getPawnAttacks(s, c);
        }
        return out;
    }

    // Overall scale of the king-danger term, in percent.
    constexpr int KING_DANGER_SCALE = 100;

    // An unfinished king-safety rework, kept switched off but left in place
    // because the measurements below are worth not repeating.
    //   KS_NEW_TERMS - pawn attackers, pawn storm, defenders, open lines
    //   KS_NEW_RAMP  - attacker count amplifies danger instead of discounting it
    //
    // Both on scored 26% over 63 games vs the build without them (~-180 Elo):
    // the two multiply, so inflated units meet an amplified ramp and a single
    // position can swing ~960cp, which is more than a queen. Each half alone
    // came out level with the baseline over 60 games, so neither is bad by
    // itself - the product is. The way forward is both on with
    // KING_DANGER_SCALE somewhere near 25-35, not either half in isolation.
    //
    // Caveat on all of the above: 60-game matches at 60+0.6 only resolve
    // effects around +/-100 Elo. A control that was eval-identical to the
    // baseline and 11% faster measured -17 Elo over the same 60 games, so
    // the two "level" readings mean "not a disaster", nothing finer.
    constexpr bool KS_NEW_TERMS = false;
    constexpr bool KS_NEW_RAMP  = false;

    // Danger grows faster than the count of attackers: one piece pointed at
    // the king is nothing, two is an annoyance, four with a line open is
    // usually decisive. Percentages, so entries above 100 amplify.
    constexpr int RAMP_NEW[8] = {0, 0, 100, 175, 250, 300, 340, 360};
    // The original weighting, which can only ever discount the raw units.
    constexpr int RAMP_OLD[8] = {0, 0,  50,  75,  88,  94,  97,  99};

    // An enemy pawn near the king, by how many ranks away it still is. A
    // storm both opens lines and gains tempo, so it is dangerous well before
    // it makes contact.
    constexpr int STORM_BY_RANK_DIST[8] = {0, 70, 45, 25, 12, 5, 0, 0};

    struct KingZone {
        Square   sq   = 64;
        Bitboard zone = 0ULL;
        int attackers = 0;   // distinct enemy pieces bearing on the zone
        int units     = 0;   // weighted enemy pressure
        int defenders = 0;   // weighted friendly cover of the same squares
    };

    // Enemy pawns marching at the king. The pawn shield only looks at our own
    // pawns, so a storm that has not arrived yet is otherwise invisible - and
    // by the time it arrives the lines are already open. Two files either
    // side, because a king on c1 is very much a target for an a-file storm.
    int pawnStorm(const Board& board, Color us, Square king, int& stormers) {
        Bitboard pawns = board.getPieceBitboard(PieceType::PAWN, ~us);
        int kf = fileOf(king), kr = rankOf(king), total = 0;
        while (pawns) {
            Square s = firstSquare(pawns); pawns &= pawns - 1;
            if (std::abs(fileOf(s) - kf) > 2) continue;
            int d = std::abs(rankOf(s) - kr);
            total += STORM_BY_RANK_DIST[d];
            if (d <= 3) stormers++;
        }
        return total;
    }

    // A heavy piece on a line into the king with none of our pawns left on
    // it. This is what the storm is trying to create.
    int openLinesToKing(const Board& board, Color us, Square king) {
        Bitboard ourPawns = board.getPieceBitboard(PieceType::PAWN, us);
        Bitboard heavy = board.getPieceBitboard(PieceType::ROOK,  ~us)
                       | board.getPieceBitboard(PieceType::QUEEN, ~us);
        int kf = fileOf(king), total = 0;
        for (int f = std::max(0, kf - 2); f <= std::min(7, kf + 2); f++) {
            Bitboard fb = fileBB(f);
            if (ourPawns & fb) continue;              // still sheltered
            if (heavy & fb) total += (f == kf) ? 50 : 35;
        }
        return total;
    }

    // Mobility and king safety in one pass, white-relative.
    //
    // Mobility counts squares a piece could actually move to, priced per
    // piece type and game phase; squares covered by an enemy pawn are
    // excluded, since a piece cannot usefully sit where a pawn just takes it.
    //
    // King safety needs exactly the same per-piece attack sets, and those
    // sets are the most expensive thing in the whole evaluation, so the two
    // terms share one loop: every attack bitboard is computed once and then
    // asked three questions - where can this piece go, does it bear on the
    // enemy king, does it cover our own.
    void evaluateMobilityAndKingSafety(const Board& board, int& mg, int& eg) {
        static const int mobMG[4] = {4, 5, 2, 1};   // knight, bishop, rook, queen
        static const int mobEG[4] = {4, 5, 4, 2};
        static const int atkWeight[4] = {30, 30, 60, 100};  // per attacked zone square
        Bitboard occ = board.getAllPieces();

        KingZone kz[2];
        for (int i = 0; i < 2; i++) {
            kz[i].sq = board.findKing(static_cast<Color>(i));
            if (kz[i].sq < 64)
                kz[i].zone = MoveGenerator::getKingAttacks(kz[i].sq) | (1ULL << kz[i].sq);
        }

        for (int i = 0; i < 2; i++) {
            Color c = static_cast<Color>(i);
            int sign = (c == Color::WHITE) ? 1 : -1;
            Bitboard own = (c == Color::WHITE) ? board.getWhitePieces()
                                               : board.getBlackPieces();
            Bitboard bad = own | pawnCover(board, ~c);
            KingZone& them = kz[1 - i];   // the king these pieces attack
            KingZone& ours = kz[i];       // the king these pieces defend

            auto tally = [&](PieceType t, int idx, auto attackFn) {
                Bitboard bb = board.getPieceBitboard(t, c);
                while (bb) {
                    Square s = firstSquare(bb); bb &= bb - 1;
                    Bitboard att = attackFn(s);   // the expensive part, done once
                    int n = popCount(att & ~bad);
                    mg += sign * n * mobMG[idx];
                    eg += sign * n * mobEG[idx];
                    int hits = popCount(att & them.zone);
                    if (hits) { them.attackers++; them.units += atkWeight[idx] * hits; }
                    if (KS_NEW_TERMS)
                        ours.defenders += 15 * popCount(att & ours.zone);
                }
            };

            tally(PieceType::KNIGHT, 0, [&](Square s){ return MoveGenerator::getKnightAttacks(s); });
            tally(PieceType::BISHOP, 1, [&](Square s){ return MoveGenerator::getBishopAttacks(s, occ); });
            tally(PieceType::ROOK,   2, [&](Square s){ return MoveGenerator::getRookAttacks(s, occ); });
            tally(PieceType::QUEEN,  3, [&](Square s){ return MoveGenerator::getQueenAttacks(s, occ); });

            // Pawns bear on the zone too - a pawn on a3 beside a king on b2
            // is as dangerous as a piece - but they carry no mobility term.
            // The whole pawn contribution counts as a single attacker.
            if (KS_NEW_TERMS) {
                Bitboard pawns = board.getPieceBitboard(PieceType::PAWN, c);
                int pawnHits = 0;
                while (pawns) {
                    Square s = firstSquare(pawns); pawns &= pawns - 1;
                    pawnHits += popCount(MoveGenerator::getPawnAttacks(s, c) & them.zone);
                }
                if (pawnHits) { them.attackers++; them.units += 20 * pawnHits; }
            }
        }

        for (int i = 0; i < 2; i++) {
            Color c = static_cast<Color>(i);
            if (kz[i].sq >= 64) continue;
            int stormers = 0, storm = 0, openLines = 0;
            if (KS_NEW_TERMS) {
                storm     = pawnStorm(board, c, kz[i].sq, stormers);
                openLines = openLinesToKing(board, c, kz[i].sq);
            }
            // an attack is local superiority, so defenders come off the total
            int danger = kz[i].units + storm + openLines - kz[i].defenders;
            if (danger < 0) danger = 0;
            int n = kz[i].attackers + (stormers ? 1 : 0);
            danger = danger * (KS_NEW_RAMP ? RAMP_NEW : RAMP_OLD)[std::min(n, 7)] / 100;
            danger = danger * KING_DANGER_SCALE / 100;
            mg += (c == Color::WHITE) ? -danger : danger;
        }
    }

    // Rooks on files with no friendly pawn in the way. A fully open file (no
    // pawns of either colour) is worth more than a semi-open one, and both
    // matter most while there is still a middlegame to invade.
    void evaluateRookFiles(const Board& board, int& mg, int& eg) {
        Bitboard wp = board.getPieceBitboard(PieceType::PAWN, Color::WHITE);
        Bitboard bp = board.getPieceBitboard(PieceType::PAWN, Color::BLACK);

        for (Color c : {Color::WHITE, Color::BLACK}) {
            int sign = (c == Color::WHITE) ? 1 : -1;
            Bitboard ownPawns   = (c == Color::WHITE) ? wp : bp;
            Bitboard enemyPawns = (c == Color::WHITE) ? bp : wp;
            Bitboard rooks = board.getPieceBitboard(PieceType::ROOK, c);
            while (rooks) {
                Square s = firstSquare(rooks); rooks &= rooks - 1;
                Bitboard f = fileBB(fileOf(s));
                if (ownPawns & f) continue;
                if (enemyPawns & f) { mg += sign * 15; eg += sign *  8; }  // semi-open
                else                { mg += sign * 30; eg += sign * 15; }  // open
            }
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

    // White-relative pawn shelter score. The attack-based half of king safety
    // lives in evaluateMobilityAndKingSafety, which shares its attack sets
    // with mobility.
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
    : timeLimit(5000), nodeLimit(0), nodesSearched(0),
      currentDepth(0), quietMode(false), useOpeningBook(false) {
    tt.resize(TT_SIZE);
    newGame();
}

void SearchEngine::newGame() {
    for (auto& e : tt) e = TTEntry();
    for (int i = 0; i < MAX_PLY; i++)
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
    int stableCount = 0;      // iterations in a row with the same best move
    bool scoreDropped = false;  // eval fell sharply on the last iteration

    for (int d = 1; d <= depth; d++) {
        currentDepth = d;
        // Previous iteration's best move is searched first. The root is ply 0.
        orderMoves(mutableBoard, moves, bestMove, 0);

        // Aspiration window: search around the previous score first; on a
        // fail re-search that side with a full window.
        int alphaW = (d >= 3) ? std::max(bestScore - 35, -(MATE_SCORE + 1)) : -(MATE_SCORE + 1);
        int betaW  = (d >= 3) ? std::min(bestScore + 35,  (MATE_SCORE + 1)) :  (MATE_SCORE + 1);

        Move iterBest = moves[0];
        int iterBestScore = std::numeric_limits<int>::min();

        while (true) {
            iterBestScore = std::numeric_limits<int>::min();
            int alpha = alphaW;
            int beta  = betaW;

            for (size_t i = 0; i < moves.size(); i++) {
                mutableBoard.makeMove(moves[i]);
                int score;
                if (i == 0) {
                    score = -alphaBeta(mutableBoard, d - 1, -beta, -alpha, true, 1);
                } else {
                    score = -alphaBeta(mutableBoard, d - 1, -alpha - 1, -alpha, true, 1);
                    if (score > alpha && score < beta)
                        score = -alphaBeta(mutableBoard, d - 1, -beta, -alpha, true, 1);
                }
                mutableBoard.unmakeMove(moves[i]);

                if (isTimeUp()) break;  // scores from an aborted search are garbage

                if (score > iterBestScore) { iterBestScore = score; iterBest = moves[i]; }
                if (score > alpha) alpha = score;
            }

            if (isTimeUp()) break;
            if (iterBestScore <= alphaW && alphaW > -(MATE_SCORE + 1)) {
                alphaW = -(MATE_SCORE + 1);  // fail low: re-search
                continue;
            }
            if (iterBestScore >= betaW && betaW < MATE_SCORE + 1) {
                betaW = MATE_SCORE + 1;      // fail high: re-search
                continue;
            }
            break;
        }

        if ((!isTimeUp() || d == 1) &&
            iterBestScore != std::numeric_limits<int>::min()) {
            bool sameMove = iterBest.from == bestMove.from && iterBest.to == bestMove.to;
            stableCount  = (sameMove && d > 1) ? stableCount + 1 : 0;
            scoreDropped = (d > 2 && iterBestScore < bestScore - 40);
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
                uint64_t h = pvBoard.getHash();
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
                // Mate scores encode ply distance from the root, so the
                // distance falls straight out of the score.
                int plies  = MATE_SCORE - std::abs(bestScore);
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

        // Time strategy: aim for the soft budget, spend less when the best
        // move has been stable for several iterations, and stretch the budget
        // when the score just dropped — that is when extra thought pays most.
        // The hard limit (isTimeUp) still aborts mid-iteration regardless.
        if (timeLimit > 0) {
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - searchStart).count();
            double budget = (softLimit > 0) ? softLimit : timeLimit * 0.5;
            if (stableCount >= 4) budget *= 0.6;
            if (scoreDropped)     budget *= 2.5;
            if (budget > timeLimit) budget = timeLimit;
            if (ms >= budget) break;
            // A new iteration costs about as much as all previous combined:
            // don't start one that cannot possibly finish within the hard cap.
            if (ms * 2 >= timeLimit) break;
        }
    }

    result.bestMove      = bestMove;
    result.score         = bestScore;
    result.depth         = currentDepth;
    result.nodesSearched = nodesSearched;

    return result;
}

int SearchEngine::alphaBeta(Board& board, int depth, int alpha, int beta, bool nullMoveAllowed, int ply) {
    if (isTimeUp()) return 0;
    nodesSearched++;

    bool inCheck = board.isInCheck(board.getSideToMove());

    // A node in check extends without spending depth, so a long forcing line
    // can recurse without the depth counter ever reaching zero. Repetition
    // detection normally ends these, but cap the ply as a hard backstop.
    if (ply >= MAX_PLY - 1) return evaluate(board) * (board.getSideToMove() == Color::WHITE ? 1 : -1);

    // Draw detection: repetition, dead material, and fifty-move rule (unless
    // in check, where the mating side may still deliver mate on this move).
    if (board.isRepetition() || board.isInsufficientMaterial()) return DRAW_SCORE;
    if (board.isDrawByFiftyMoves() && !inCheck) return DRAW_SCORE;

    // Check extension: never drop into quiescence while in check
    if (inCheck) depth++;

    if (depth == 0) return quiescence(board, alpha, beta, ply);

    // TT probe
    uint64_t hash  = board.getHash();
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
            int nullScore = -alphaBeta(board, depth - R - 1, -beta, -beta + 1, false, ply + 1);
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

    // Pseudo-legal moves with lazy legality: each move is validated by making
    // it and testing for check, instead of filtering the whole list up front.
    std::vector<Move> moves = MoveGenerator::generatePseudoLegalMoves(board);
    orderMoves(board, moves, hasTTMove ? ttMove : Move(), ply);

    Color us           = board.getSideToMove();
    int  originalAlpha = alpha;
    Move bestMove;
    int  bestScore     = std::numeric_limits<int>::min();
    int  legalCount    = 0;

    for (size_t i = 0; i < moves.size(); i++) {
        const Move& move = moves[i];
        bool isQuiet = !move.isCapture && move.promotion == PieceType::NONE;

        // Futility pruning: skip quiet moves when static eval + margin can't beat alpha
        if (doFutility && isQuiet && legalCount > 0) {
            int margin = (depth == 1) ? 100 : 300;
            if (staticEval + margin <= alpha) continue;
        }

        board.makeMove(move);
        if (board.isInCheck(us)) { board.unmakeMove(move); continue; }
        legalCount++;
        int score;

        if (legalCount == 1) {
            // First move: full window
            score = -alphaBeta(board, depth - 1, -beta, -alpha, true, ply + 1);
        } else {
            // LMR: reduce quiet moves that are ordered late
            int reduction = 0;
            if (depth >= 3 && legalCount > 3 && isQuiet && !inCheck)
                reduction = 1 + (legalCount > 6 && depth >= 4 ? 1 : 0);

            // PVS: null window first
            score = -alphaBeta(board, depth - 1 - reduction, -alpha - 1, -alpha, true, ply + 1);

            // Re-search full window if it beat alpha (or was reduced)
            if (score > alpha && (reduction > 0 || score < beta))
                score = -alphaBeta(board, depth - 1, -beta, -alpha, true, ply + 1);
        }

        board.unmakeMove(move);

        if (score > bestScore) { bestScore = score; bestMove = move; }
        if (score > alpha) alpha = score;
        if (alpha >= beta) {
            if (isQuiet) { recordKillerMove(move, ply); recordHistoryMove(move, depth); }
            break;
        }
    }

    // Mate distance is measured from the root, so that a mate found closer to
    // the root scores as more decisive and the shortest mate is preferred.
    // Keying this to remaining depth instead inverts that preference.
    if (legalCount == 0)
        return inCheck ? -(MATE_SCORE - ply) : DRAW_SCORE;

    // Don't store aborted searches, and don't store mate scores: they encode
    // distance-to-mate relative to this node and are wrong elsewhere.
    if (!isTimeUp() && bestScore < MATE_SCORE - 100 && bestScore > -(MATE_SCORE - 100)) {
        int8_t flag = (bestScore >= beta) ? 1 : (bestScore <= originalAlpha ? 2 : 0);
        entry = {hash, bestScore, static_cast<int8_t>(depth), flag, bestMove};
    }

    return bestScore;
}

int SearchEngine::see(const Board& board, const Move& move) {
    Square to = move.to;
    Bitboard occ = board.getAllPieces();
    int gain[32];
    int d = 0;

    Color stm          = board.pieceAt(move.from).color;
    PieceType attacker = board.pieceAt(move.from).type;
    PieceType victim   = move.isEnPassant ? PieceType::PAWN : board.pieceAt(to).type;

    gain[0] = getPieceValue(victim);
    occ &= ~(1ULL << move.from);
    if (move.isEnPassant)
        occ &= ~(1ULL << (stm == Color::WHITE ? to - 8 : to + 8));
    stm = ~stm;

    // Swap algorithm: alternate least-valuable recaptures until a side has
    // none (recomputing attackers each round reveals x-ray attackers).
    while (d < 30) {
        Bitboard attackers = MoveGenerator::attackersTo(board, to, occ) & occ;
        Bitboard side = attackers & (stm == Color::WHITE ? board.getWhitePieces()
                                                         : board.getBlackPieces());
        if (!side) break;

        PieceType lva = PieceType::PAWN;
        Bitboard fromBB = 0;
        for (int t = 0; t < 6; t++) {
            Bitboard s = side & board.getPieceBitboard(static_cast<PieceType>(t), stm);
            if (s) { lva = static_cast<PieceType>(t); fromBB = s & (~s + 1); break; }
        }

        d++;
        gain[d] = getPieceValue(attacker) - gain[d - 1];
        attacker = lva;
        occ &= ~fromBB;
        stm = ~stm;
    }

    while (d > 0) { gain[d - 1] = -std::max(-gain[d - 1], gain[d]); d--; }
    return gain[0];
}

int SearchEngine::quiescence(Board& board, int alpha, int beta, int ply) {
    nodesSearched++;

    if (isTimeUp()) return alpha;
    if (ply >= MAX_PLY - 1)
        return evaluate(board) * (board.getSideToMove() == Color::WHITE ? 1 : -1);

    // A capture that gives check lands the next node here in check. Standing
    // pat is not an option then - the side to move has to answer the check -
    // and the answer is often a king move or a block, neither of which is a
    // capture. Searching only captures at those nodes hands back a score for
    // a position the side to move cannot actually hold, which is how forcing
    // tactics get missed.
    bool inCheck = board.isInCheck(board.getSideToMove());
    int standPat = 0;

    if (!inCheck) {
        standPat = evaluate(board);
        if (board.getSideToMove() == Color::BLACK) standPat = -standPat;
        if (standPat >= beta) return beta;
        if (standPat > alpha) alpha = standPat;
    }

    std::vector<Move> allMoves = MoveGenerator::generatePseudoLegalMoves(board);
    std::vector<Move> candidates;
    candidates.reserve(allMoves.size());
    for (const Move& m : allMoves)
        if (inCheck || m.isCapture || m.promotion != PieceType::NONE)
            candidates.push_back(m);   // in check: every evasion, not just captures

    orderMoves(board, candidates, Move(), ply);

    Color us = board.getSideToMove();
    int legalCount = 0;
    for (const Move& move : candidates) {
        // Both prunings assume we may decline the move, which is false in
        // check, so they only apply to ordinary quiescence nodes.
        if (!inCheck && move.promotion == PieceType::NONE) {
            // Delta pruning: even winning this piece can't lift alpha
            PieceType victim = move.isEnPassant ? PieceType::PAWN
                                                : board.pieceAt(move.to).type;
            if (standPat + getPieceValue(victim) + 200 <= alpha) continue;
            // Skip captures that lose material outright
            if (see(board, move) < 0) continue;
        }

        board.makeMove(move);
        if (board.isInCheck(us)) { board.unmakeMove(move); continue; }
        legalCount++;
        int score = -quiescence(board, -beta, -alpha, ply + 1);
        board.unmakeMove(move);
        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }

    // Only meaningful in check, where every legal move was generated: no
    // evasion means mate. Out of check the list was captures only, so an
    // empty one just means the position is quiet.
    if (inCheck && legalCount == 0) return -(MATE_SCORE - ply);

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
                int value   = getPieceValue(type);
                int valueEG = getPieceValueEG(type);
                mg += sign * (value   + getPositionalValue(type, sq, color, false));
                eg += sign * (valueEG + getPositionalValue(type, sq, color, true));
                if (type != PieceType::KING)
                    (color == Color::WHITE ? materialW : materialB) += value;
            }
        }
    }

    // Bishop pair
    if (popCount(board.getPieceBitboard(PieceType::BISHOP, Color::WHITE)) >= 2) { mg += 30; eg += 30; }
    if (popCount(board.getPieceBitboard(PieceType::BISHOP, Color::BLACK)) >= 2) { mg -= 30; eg -= 30; }

    // Mobility and attack-based king danger, sharing one pass over the pieces
    evaluateMobilityAndKingSafety(board, mg, eg);

    // Rooks on open and semi-open files
    evaluateRookFiles(board, mg, eg);

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
        case PieceType::QUEEN:  return 1000;
        case PieceType::KING:   return 20000;
        default: return 0;
    }
}

int SearchEngine::getPieceValueEG(PieceType type) {
    switch (type) {
        case PieceType::PAWN:   return 115;  // passed/promotion races matter more
        case PieceType::KNIGHT: return 300;  // no outposts left to jump into
        case PieceType::BISHOP: return 345;  // long diagonals open up
        case PieceType::ROOK:   return 520;  // the dominant endgame piece
        case PieceType::QUEEN:  return 1000;
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
                              const Move& ttMove, int ply) {
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
            if (getPieceValue(victim) < getPieceValue(board.pieceAt(m.from).type) &&
                see(board, m) < 0) {
                s = 20000 + see(board, m);                   // losing capture: try late
            } else {
                s = 100000 + getPieceValue(victim) * 10      // MVV-LVA
                           - getPieceValue(board.pieceAt(m.from).type);
            }
        } else if (m.promotion != PieceType::NONE) {
            s = 90000 + getPieceValue(m.promotion);
        } else if (isKillerMove(m, ply)) {
            s = 80000;
        } else {
            // History has to stay below the killer band, but clamping it
            // there flattens every well-established quiet move onto the same
            // score and throws the ordering away exactly where it matters.
            // Scale instead: the table is halved at 1e6, so /16 always lands
            // under the killers while preserving the relative order.
            s = getHistoryScore(m) / 16;
        }
        scored.emplace_back(s, m);
    }

    std::stable_sort(scored.begin(), scored.end(),
                     [](const auto& a, const auto& b) { return a.first > b.first; });
    for (size_t i = 0; i < moves.size(); i++) moves[i] = scored[i].second;
}

bool SearchEngine::isKillerMove(const Move& move, int ply) {
    if (ply < 0 || ply >= MAX_PLY) return false;
    for (int i = 0; i < MAX_KILLER_MOVES; i++)
        if (killerMoves[ply][i].from == move.from &&
            killerMoves[ply][i].to   == move.to   &&
            killerMoves[ply][i].promotion == move.promotion) return true;
    return false;
}

int SearchEngine::getHistoryScore(const Move& move) {
    if (move.from >= 64 || move.to >= 64) return 0;
    return historyTable[move.from][move.to];
}

void SearchEngine::recordKillerMove(const Move& move, int ply) {
    if (ply < 0 || ply >= MAX_PLY || move.isCapture) return;
    // Don't let the same move occupy both slots.
    if (killerMoves[ply][0].from == move.from &&
        killerMoves[ply][0].to   == move.to   &&
        killerMoves[ply][0].promotion == move.promotion) return;
    for (int i = MAX_KILLER_MOVES - 1; i > 0; i--)
        killerMoves[ply][i] = killerMoves[ply][i - 1];
    killerMoves[ply][0] = move;
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
