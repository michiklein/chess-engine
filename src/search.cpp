#include "search.h"
#include "movegen.h"
#include <algorithm>
#include <iostream>

SearchEngine::SearchEngine() : maxDepth(8), timeLimit(5000), nodesSearched(0), currentDepth(0), useOpeningBook(false), quietMode(false) {
    // Initialize killer moves table
    for (int i = 0; i < 32; i++) {
        for (int j = 0; j < MAX_KILLER_MOVES; j++) {
            killerMoves[i][j] = Move();
        }
    }
    
    // Initialize history table
    for (int i = 0; i < 64; i++) {
        for (int j = 0; j < 64; j++) {
            historyTable[i][j] = 0;
        }
    }
}


SearchResult SearchEngine::search(const Board& board, int depth) {
    SearchResult result;
    nodesSearched = 0;

    // Check opening book first
    if (useOpeningBook) {
        Move bookMove = openingBook.getRandomMove(board);
        if (bookMove.from != bookMove.to || bookMove.from != 0) {
            result.bestMove = bookMove;
            result.score = 0;
            result.depth = 0;
            result.nodesSearched = 0;
            if (!quietMode) {
                std::cout << "Playing from opening book: " << openingBook.getEcoCode(board) << std::endl;
            }
            return result;
        }
    }

    std::vector<Move> moves = MoveGenerator::generateLegalMoves(board);
    if (moves.empty()) {
        result.score = board.isInCheck(board.getSideToMove()) ? -MATE_SCORE : DRAW_SCORE;
        return result;
    }

    bool isMaximizing = (board.getSideToMove() == Color::WHITE);
    Move bestMove = moves[0];
    int bestScore = isMaximizing ? std::numeric_limits<int>::min() : std::numeric_limits<int>::max();

    Board mutableBoard = board;

    // Iterative deepening: search at increasing depths so the last completed
    // depth always gives a valid best move (useful for future time management)
    for (int d = 1; d <= depth; d++) {
        currentDepth = d;
        Move iterBestMove = moves[0];
        int iterBestScore = isMaximizing ? std::numeric_limits<int>::min() : std::numeric_limits<int>::max();

        orderMoves(mutableBoard, moves);

        int alpha = -(MATE_SCORE + 1);
        int beta  =  (MATE_SCORE + 1);

        for (const Move& move : moves) {
            mutableBoard.makeMove(move);
            int score = alphaBeta(mutableBoard, d - 1, alpha, beta, !isMaximizing);
            mutableBoard.unmakeMove(move);

            if (isMaximizing) {
                if (score > iterBestScore) {
                    iterBestScore = score;
                    iterBestMove = move;
                }
                alpha = std::max(alpha, score);
            } else {
                if (score < iterBestScore) {
                    iterBestScore = score;
                    iterBestMove = move;
                }
                beta = std::min(beta, score);
            }
        }

        bestMove  = iterBestMove;
        bestScore = iterBestScore;
    }

    result.bestMove = bestMove;
    result.score = bestScore;
    result.depth = depth;
    result.nodesSearched = nodesSearched;

    if (!quietMode) {
        std::cout << "depth=" << depth << " score=" << bestScore
                  << " nodes=" << nodesSearched << std::endl;
    }

    return result;
}



int SearchEngine::alphaBeta(Board& board, int depth, int alpha, int beta, bool maximizing) {
    nodesSearched++;

    if (depth == 0) {
        return quiescence(board, alpha, beta, maximizing);
    }

    std::vector<Move> moves = MoveGenerator::generateLegalMoves(board);

    if (moves.empty()) {
        Color stm = board.getSideToMove();
        if (board.isInCheck(stm)) {
            // Checkmate: add depth bonus so shorter mates score higher
            return stm == Color::WHITE ? -(MATE_SCORE - depth) : (MATE_SCORE - depth);
        }
        return DRAW_SCORE; // Stalemate
    }

    orderMoves(board, moves);

    if (maximizing) {
        int maxEval = std::numeric_limits<int>::min();
        for (const Move& move : moves) {
            board.makeMove(move);
            int eval = alphaBeta(board, depth - 1, alpha, beta, false);
            board.unmakeMove(move);

            maxEval = std::max(maxEval, eval);
            alpha   = std::max(alpha,   eval);

            if (beta <= alpha) {
                if (!move.isCapture) {
                    recordKillerMove(move, depth);
                    recordHistoryMove(move, depth);
                }
                break;
            }
        }
        return maxEval;
    } else {
        int minEval = std::numeric_limits<int>::max();
        for (const Move& move : moves) {
            board.makeMove(move);
            int eval = alphaBeta(board, depth - 1, alpha, beta, true);
            board.unmakeMove(move);

            minEval = std::min(minEval, eval);
            beta    = std::min(beta,    eval);

            if (beta <= alpha) {
                if (!move.isCapture) {
                    recordKillerMove(move, depth);
                    recordHistoryMove(move, depth);
                }
                break;
            }
        }
        return minEval;
    }
}


int SearchEngine::quiescence(Board& board, int alpha, int beta, bool maximizing) {
    nodesSearched++;

    int standPat = evaluate(board);

    if (maximizing) {
        if (standPat >= beta) return beta;
        if (standPat > alpha) alpha = standPat;
    } else {
        if (standPat <= alpha) return alpha;
        if (standPat < beta)  beta  = standPat;
    }

    std::vector<Move> allMoves = MoveGenerator::generateLegalMoves(board);

    // Only search captures (and promotions) to resolve tactical sequences
    std::vector<Move> captures;
    captures.reserve(allMoves.size());
    for (const Move& m : allMoves) {
        if (m.isCapture || m.promotion != PieceType::NONE) {
            captures.push_back(m);
        }
    }

    orderMoves(board, captures);

    if (maximizing) {
        for (const Move& move : captures) {
            board.makeMove(move);
            int score = quiescence(board, alpha, beta, false);
            board.unmakeMove(move);
            if (score > alpha) alpha = score;
            if (beta <= alpha) return beta;
        }
        return alpha;
    } else {
        for (const Move& move : captures) {
            board.makeMove(move);
            int score = quiescence(board, alpha, beta, true);
            board.unmakeMove(move);
            if (score < beta) beta = score;
            if (beta <= alpha) return alpha;
        }
        return beta;
    }
}

int SearchEngine::evaluate(const Board& board) {
    int score = 0;

    for (Square sq = 0; sq < 64; sq++) {
        const Piece& piece = board.pieceAt(sq);
        if (!piece.isEmpty()) {
            int value = getPieceValue(piece.type) + getPositionalValue(piece.type, sq, piece.color);
            if (piece.color == Color::WHITE) {
                score += value;
            } else {
                score -= value;
            }
        }
    }

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

    // White tables are from white's perspective (rank 0 = rank 1).
    // For black, mirror vertically so black pieces are evaluated symmetrically.
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

        default:
            return 0;
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
    
    for (int i = 0; i < MAX_KILLER_MOVES; i++) {
        if (killerMoves[depth][i].from == move.from && 
            killerMoves[depth][i].to == move.to &&
            killerMoves[depth][i].promotion == move.promotion) {
            return true;
        }
    }
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
    if (historyTable[move.from][move.to] > 1000000) {
        for (int i = 0; i < 64; i++)
            for (int j = 0; j < 64; j++)
                historyTable[i][j] /= 2;
    }
}

bool SearchEngine::loadOpeningBook(const std::string& filename) {
    useOpeningBook = openingBook.loadFromFile(filename);
    return useOpeningBook;
}

bool SearchEngine::isMoveInOpeningBook(const Board& board, const Move& move) {
    if (!useOpeningBook) return false;
    for (const auto& bookMove : openingBook.getMoves(board)) {
        if (bookMove.move.from == move.from &&
            bookMove.move.to   == move.to   &&
            bookMove.move.promotion == move.promotion) {
            return true;
        }
    }
    return false;
}
