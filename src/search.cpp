#include "search.h"
#include "movegen.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <iomanip>

SearchEngine::SearchEngine() : maxDepth(8), timeLimit(5000), nodesSearched(0), currentDepth(0), useOpeningBook(false) {
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
    currentDepth = depth;
    
    std::vector<Move> moves = MoveGenerator::generateLegalMoves(board);
    
    if (moves.empty()) {
        result.score = board.isInCheck(board.getSideToMove()) ? -MATE_SCORE : DRAW_SCORE;
        return result;
    }
    
    // Check opening book first (use it as long as positions are available)
    if (useOpeningBook) {
        Move bookMove = openingBook.getRandomMove(board);
        if (bookMove.from != bookMove.to || bookMove.from != 0) {
            result.bestMove = bookMove;
            result.score = 0; // Neutral score for book moves
            result.depth = depth;
            result.nodesSearched = 0;
            std::cout << "Playing from opening book: " << openingBook.getEcoCode(board) << std::endl;
            return result;
        } else {
            std::cout << "No opening book move found, using search" << std::endl;
        }
    }
    
    // Order moves for better search efficiency
    orderMoves(board, moves);
    
    int bestScore = std::numeric_limits<int>::min();
    Move bestMove = moves[0];
    
    for (const Move& move : moves) {
        Board testBoard = board;
        testBoard.makeMove(move);
        
        int score = -alphaBeta(testBoard, depth - 1, 
                              std::numeric_limits<int>::min(), 
                              std::numeric_limits<int>::max(), 
                              false);
        
        if (score > bestScore) {
            bestScore = score;
            bestMove = move;
        }
    }
    
    result.bestMove = bestMove;
    result.score = bestScore;
    result.depth = depth;
    result.nodesSearched = nodesSearched;
    
    return result;
}



int SearchEngine::alphaBeta(Board& board, int depth, int alpha, int beta, bool maximizing) {
    nodesSearched++;
    
    if (depth == 0) {
        return evaluate(board);
    }
    
    // Check opening book at every position during search
    if (useOpeningBook) {
        Move bookMove = openingBook.getRandomMove(board);
        if (bookMove.from != bookMove.to || bookMove.from != 0) {
            // Found a book move, return a neutral score
            return 0;
        }
    }
    
    std::vector<Move> moves = MoveGenerator::generateLegalMoves(board);
    
    if (moves.empty()) {
        return board.isInCheck(board.getSideToMove()) ? -MATE_SCORE : DRAW_SCORE;
    }
    
    // Order moves for better search efficiency
    orderMoves(board, moves);
    
    if (maximizing) {
        int maxEval = std::numeric_limits<int>::min();
        for (const Move& move : moves) {
            board.makeMove(move);
            int eval = alphaBeta(board, depth - 1, alpha, beta, false);
            board.unmakeMove(move);
            
            maxEval = std::max(maxEval, eval);
            alpha = std::max(alpha, eval);
            
            if (beta <= alpha) break; // Beta cutoff
        }
        return maxEval;
    } else {
        int minEval = std::numeric_limits<int>::max();
        for (const Move& move : moves) {
            board.makeMove(move);
            int eval = alphaBeta(board, depth - 1, alpha, beta, true);
            board.unmakeMove(move);
            
            minEval = std::min(minEval, eval);
            beta = std::min(beta, eval);
            
            if (beta <= alpha) break; // Alpha cutoff
        }
        return minEval;
    }
}


int SearchEngine::evaluate(const Board& board) {
    int score = 0;
    
    // Check for immediate checkmate (highest priority)
    if (board.isCheckmate()) {
        Color sideToMove = board.getSideToMove();
        return (sideToMove == Color::WHITE) ? -MATE_SCORE : MATE_SCORE;
    }
    
    // Check for immediate check (strong incentive)
    if (board.isInCheck(Color::WHITE)) {
        score -= 100; // Penalty for being in check
    }
    if (board.isInCheck(Color::BLACK)) {
        score += 100; // Bonus for putting opponent in check
    }
    
    // Material evaluation with piece-square tables
    for (Square sq = 0; sq < 64; sq++) {
        const Piece& piece = board.pieceAt(sq);
        if (!piece.isEmpty()) {
            int pieceValue = getPieceValue(piece.type);
            int positionalValue = getPositionalValue(piece.type, sq, piece.color);
            int mobilityValue = getMobilityValue(board, sq, piece);
            
            int totalValue = pieceValue + positionalValue + mobilityValue;
            
            if (piece.color == Color::WHITE) {
                score += totalValue;
            } else {
                score -= totalValue;
            }
        }
    }
    
    // King safety evaluation
    score += evaluateKingSafety(board, Color::WHITE);
    score -= evaluateKingSafety(board, Color::BLACK);
    
    // Pawn structure evaluation
    score += evaluatePawnStructure(board, Color::WHITE);
    score -= evaluatePawnStructure(board, Color::BLACK);
    
    // Center control
    score += evaluateCenterControl(board, Color::WHITE);
    score -= evaluateCenterControl(board, Color::BLACK);
    
    // Development (pieces off starting squares)
    score += evaluateDevelopment(board, Color::WHITE);
    score -= evaluateDevelopment(board, Color::BLACK);
    
    // Tactical bonuses
    score += evaluateTactics(board);
    
    // Capture evaluation (safe vs unsafe captures)
    score += evaluateCaptures(board);
    
    // Hung pieces evaluation
    score += evaluateHungPieces(board);
    
    // King attack bonuses (moves toward checkmate)
    score += evaluateKingAttack(board, Color::WHITE);
    score -= evaluateKingAttack(board, Color::BLACK);
    
    return score;
}

int SearchEngine::getPieceValue(PieceType type) {
    switch (type) {
        case PieceType::PAWN:   return 1;
        case PieceType::KNIGHT: return 3;
        case PieceType::BISHOP: return 3;
        case PieceType::ROOK:   return 5;
        case PieceType::QUEEN:  return 9;
        case PieceType::KING:   return 100;
        default: return 0;
    }
}

int SearchEngine::getPositionalValue(PieceType type, Square square, Color color) {
    int file = fileOf(square);
    int rank = rankOf(square);
    
    // Adjust rank for black pieces (flip the board)
    if (color == Color::BLACK) {
        rank = 7 - rank;
    }
    
    // Distance from center (center is files 3,4 and ranks 3,4)
    int centerDistance = std::max(abs(file - 3), abs(file - 4)) + std::max(abs(rank - 3), abs(rank - 4));
    
    switch (type) {
        case PieceType::PAWN: {
            // Enhanced pawn piece-square table
            static const int pawnTable[64] = {
                0,  0,  0,  0,  0,  0,  0,  0,
                50, 50, 50, 50, 50, 50, 50, 50,
                10, 10, 20, 30, 30, 20, 10, 10,
                5,  5, 10, 25, 25, 10,  5,  5,
                0,  0,  0, 20, 20,  0,  0,  0,
                5, -5,-10,  0,  0,-10, -5,  5,
                5, 10, 10,-20,-20, 10, 10,  5,
                0,  0,  0,  0,  0,  0,  0,  0
            };
            return pawnTable[square];
        }
            
        case PieceType::KNIGHT: {
            // Enhanced knight piece-square table
            static const int knightTable[64] = {
                -50,-40,-30,-30,-30,-30,-40,-50,
                -40,-20,  0,  0,  0,  0,-20,-40,
                -30,  0, 10, 15, 15, 10,  0,-30,
                -30,  5, 15, 20, 20, 15,  5,-30,
                -30,  0, 15, 20, 20, 15,  0,-30,
                -30,  5, 10, 15, 15, 10,  5,-30,
                -40,-20,  0,  5,  5,  0,-20,-40,
                -50,-40,-30,-30,-30,-30,-40,-50
            };
            return knightTable[square];
        }
            
        case PieceType::BISHOP: {
            // Enhanced bishop piece-square table
            static const int bishopTable[64] = {
                -20,-10,-10,-10,-10,-10,-10,-20,
                -10,  0,  0,  0,  0,  0,  0,-10,
                -10,  0,  5, 10, 10,  5,  0,-10,
                -10,  5,  5, 10, 10,  5,  5,-10,
                -10,  0, 10, 10, 10, 10,  0,-10,
                -10, 10, 10, 10, 10, 10, 10,-10,
                -10,  5,  0,  0,  0,  0,  5,-10,
                -20,-10,-10,-10,-10,-10,-10,-20
            };
            return bishopTable[square];
        }
            
        case PieceType::ROOK: {
            // Enhanced rook piece-square table
            static const int rookTable[64] = {
                 0,  0,  0,  0,  0,  0,  0,  0,
                 5, 10, 10, 10, 10, 10, 10,  5,
                -5,  0,  0,  0,  0,  0,  0, -5,
                -5,  0,  0,  0,  0,  0,  0, -5,
                -5,  0,  0,  0,  0,  0,  0, -5,
                -5,  0,  0,  0,  0,  0,  0, -5,
                -5,  0,  0,  0,  0,  0,  0, -5,
                 0,  0,  0,  5,  5,  0,  0,  0
            };
            return rookTable[square];
        }
            
        case PieceType::QUEEN: {
            // Enhanced queen piece-square table
            static const int queenTable[64] = {
                -20,-10,-10, -5, -5,-10,-10,-20,
                -10,  0,  0,  0,  0,  0,  0,-10,
                -10,  0,  5,  5,  5,  5,  0,-10,
                 -5,  0,  5,  5,  5,  5,  0, -5,
                  0,  0,  5,  5,  5,  5,  0, -5,
                -10,  5,  5,  5,  5,  5,  0,-10,
                -10,  0,  5,  0,  0,  0,  0,-10,
                -20,-10,-10, -5, -5,-10,-10,-20
            };
            return queenTable[square];
        }
            
        case PieceType::KING: {
            // Enhanced king piece-square table (opening/middlegame)
            static const int kingTable[64] = {
                -30,-40,-40,-50,-50,-40,-40,-30,
                -30,-40,-40,-50,-50,-40,-40,-30,
                -30,-40,-40,-50,-50,-40,-40,-30,
                -30,-40,-40,-50,-50,-40,-40,-30,
                -20,-30,-30,-40,-40,-30,-30,-20,
                -10,-20,-20,-20,-20,-20,-20,-10,
                 20, 20,  0,  0,  0,  0, 20, 20,
                 20, 30, 10,  0,  0, 10, 30, 20
            };
            return kingTable[square];
        }
            
        default:
            return 0;
    }
}

void SearchEngine::orderMoves(const Board& board, std::vector<Move>& moves) {
    // Enhanced move ordering with opening book moves first, then MVV-LVA and killer moves
    std::sort(moves.begin(), moves.end(), [&](const Move& a, const Move& b) {
        // 0. Opening book moves first (highest priority)
        if (useOpeningBook) {
            bool aInBook = isMoveInOpeningBook(board, a);
            bool bInBook = isMoveInOpeningBook(board, b);
            
            if (aInBook && !bInBook) return true;
            if (!aInBook && bInBook) return false;
        }
        
        // 1. Captures first (MVV-LVA: Most Valuable Victim - Least Valuable Attacker)
        bool aCapture = a.isCapture;
        bool bCapture = b.isCapture;
        
        if (aCapture && !bCapture) return true;
        if (!aCapture && bCapture) return false;
        
        if (aCapture && bCapture) {
            // Enhanced capture ordering: safe captures first, then MVV-LVA
            bool aSafe = isSafeCapture(board, a);
            bool bSafe = isSafeCapture(board, b);
            
            // Safe captures are always better than unsafe captures
            if (aSafe && !bSafe) return true;
            if (!aSafe && bSafe) return false;
            
            // If both are safe or both are unsafe, use MVV-LVA
            int aVictimValue = getPieceValue(board.pieceAt(a.to).type);
            int aAttackerValue = getPieceValue(board.pieceAt(a.from).type);
            int bVictimValue = getPieceValue(board.pieceAt(b.to).type);
            int bAttackerValue = getPieceValue(board.pieceAt(b.from).type);
            
            int aScore = aVictimValue * 10 - aAttackerValue;
            int bScore = bVictimValue * 10 - bAttackerValue;
            return aScore > bScore;
        }
        
        // 2. Killer moves (moves that caused beta cutoffs at the same depth)
        bool aKiller = isKillerMove(a, currentDepth);
        bool bKiller = isKillerMove(b, currentDepth);
        
        if (aKiller && !bKiller) return true;
        if (!aKiller && bKiller) return false;
        
        // 3. History heuristic (moves that have been good in the past)
        int aHistory = getHistoryScore(a);
        int bHistory = getHistoryScore(b);
        
        if (aHistory != bHistory) return aHistory > bHistory;
        
        // 4. Positional evaluation (prefer moves to better squares)
        int aPositional = getPositionalValue(board.pieceAt(a.from).type, a.to, board.pieceAt(a.from).color);
        int bPositional = getPositionalValue(board.pieceAt(b.from).type, b.to, board.pieceAt(b.from).color);
        
        return aPositional > bPositional;
    });
}

// Enhanced evaluation functions
int SearchEngine::getMobilityValue(const Board& board, Square sq, const Piece& piece) {
    // Count how many squares this piece can move to
    std::vector<Move> pseudoMoves = MoveGenerator::generatePseudoLegalMoves(board);
    int mobility = 0;
    
    for (const Move& move : pseudoMoves) {
        if (move.from == sq) {
            mobility++;
        }
    }
    
    // Different pieces value mobility differently
    switch (piece.type) {
        case PieceType::QUEEN: return mobility * 2;
        case PieceType::ROOK: return mobility * 3;
        case PieceType::BISHOP: return mobility * 4;
        case PieceType::KNIGHT: return mobility * 3;
        case PieceType::KING: return mobility * 1;
        default: return 0;
    }
}

int SearchEngine::evaluateKingSafety(const Board& board, Color color) {
    Square kingSquare = board.findKing(color);
    if (kingSquare >= 64) return 0;
    
    int safety = 0;
    int file = fileOf(kingSquare);
    int rank = rankOf(kingSquare);
    
    // King in corner is safer in endgame
    if ((file == 0 || file == 7) && (rank == 0 || rank == 7)) {
        safety += 20;
    }
    
    // Count friendly pieces around king
    int friendlyPieces = 0;
    for (int df = -1; df <= 1; df++) {
        for (int dr = -1; dr <= 1; dr++) {
            if (df == 0 && dr == 0) continue;
            int newFile = file + df;
            int newRank = rank + dr;
            if (newFile >= 0 && newFile < 8 && newRank >= 0 && newRank < 8) {
                Square sq = makeSquare(newFile, newRank);
                const Piece& piece = board.pieceAt(sq);
                if (!piece.isEmpty() && piece.color == color) {
                    friendlyPieces++;
                }
            }
        }
    }
    safety += friendlyPieces * 5;
    
    // Penalty for king being in check
    if (board.isInCheck(color)) {
        safety -= 50;
    }
    
    return safety;
}

int SearchEngine::evaluatePawnStructure(const Board& board, Color color) {
    int structure = 0;
    
    // Count pawns on each file
    int pawnsPerFile[8] = {0};
    for (Square sq = 0; sq < 64; sq++) {
        const Piece& piece = board.pieceAt(sq);
        if (!piece.isEmpty() && piece.type == PieceType::PAWN && piece.color == color) {
            pawnsPerFile[fileOf(sq)]++;
        }
    }
    
    // Penalty for doubled pawns
    for (int file = 0; file < 8; file++) {
        if (pawnsPerFile[file] > 1) {
            structure -= (pawnsPerFile[file] - 1) * 20;
        }
    }
    
    // Bonus for passed pawns (simplified)
    for (Square sq = 0; sq < 64; sq++) {
        const Piece& piece = board.pieceAt(sq);
        if (!piece.isEmpty() && piece.type == PieceType::PAWN && piece.color == color) {
            int rank = rankOf(sq);
            int file = fileOf(sq);
            
            // Check if pawn is passed (no enemy pawns in front or on adjacent files)
            bool isPassed = true;
            int direction = (color == Color::WHITE) ? 1 : -1;
            
            for (int r = rank + direction; r >= 0 && r < 8; r += direction) {
                for (int f = file - 1; f <= file + 1; f++) {
                    if (f >= 0 && f < 8) {
                        Square checkSq = makeSquare(f, r);
                        const Piece& checkPiece = board.pieceAt(checkSq);
                        if (!checkPiece.isEmpty() && checkPiece.type == PieceType::PAWN && 
                            checkPiece.color != color) {
                            isPassed = false;
                            break;
                        }
                    }
                }
                if (!isPassed) break;
            }
            
            if (isPassed) {
                structure += 30; // Bonus for passed pawn
            }
        }
    }
    
    return structure;
}

int SearchEngine::evaluateCenterControl(const Board& board, Color color) {
    int control = 0;
    
    // Center squares: d4, d5, e4, e5
    Square centerSquares[] = {makeSquare(3, 3), makeSquare(3, 4), makeSquare(4, 3), makeSquare(4, 4)};
    
    for (Square centerSq : centerSquares) {
        // Check if we control this square
        if (board.isSquareAttacked(centerSq, color)) {
            control += 10;
        }
        
        // Check if we have a piece on this square
        const Piece& piece = board.pieceAt(centerSq);
        if (!piece.isEmpty() && piece.color == color) {
            control += 20;
        }
    }
    
    return control;
}

int SearchEngine::evaluateDevelopment(const Board& board, Color color) {
    int development = 0;
    
    // Check if pieces are off their starting squares
    for (Square sq = 0; sq < 64; sq++) {
        const Piece& piece = board.pieceAt(sq);
        if (!piece.isEmpty() && piece.color == color) {
            int rank = rankOf(sq);
            int file = fileOf(sq);
            
            // Knights should be developed
            if (piece.type == PieceType::KNIGHT) {
                if (color == Color::WHITE && rank > 1) development += 15;
                if (color == Color::BLACK && rank < 6) development += 15;
            }
            
            // Bishops should be developed
            if (piece.type == PieceType::BISHOP) {
                if (color == Color::WHITE && rank > 1) development += 10;
                if (color == Color::BLACK && rank < 6) development += 10;
            }
            
            // Queen shouldn't be out too early
            if (piece.type == PieceType::QUEEN) {
                if (color == Color::WHITE && rank <= 1) development -= 20;
                if (color == Color::BLACK && rank >= 6) development -= 20;
            }
        }
    }
    
    return development;
}

int SearchEngine::evaluateTactics(const Board& board) {
    int tactics = 0;
    
    // Bonus for pieces that are attacking enemy pieces
    for (Square sq = 0; sq < 64; sq++) {
        const Piece& piece = board.pieceAt(sq);
        if (!piece.isEmpty()) {
            // Check if this piece is attacking enemy pieces
            std::vector<Move> pseudoMoves = MoveGenerator::generatePseudoLegalMoves(board);
            for (const Move& move : pseudoMoves) {
                if (move.from == sq && move.isCapture) {
                    const Piece& capturedPiece = board.pieceAt(move.to);
                    if (!capturedPiece.isEmpty() && capturedPiece.color != piece.color) {
                        // Bonus for attacking higher value pieces
                        int attackerValue = getPieceValue(piece.type);
                        int defenderValue = getPieceValue(capturedPiece.type);
                        if (defenderValue > attackerValue) {
                            tactics += (defenderValue - attackerValue) / 10;
                        }
                    }
                }
            }
        }
    }
    
    return tactics;
}

int SearchEngine::evaluateKingAttack(const Board& board, Color color) {
    int attack = 0;
    
    // Find the enemy king
    Color enemyColor = (color == Color::WHITE) ? Color::BLACK : Color::WHITE;
    Square enemyKing = board.findKing(enemyColor);
    
    if (enemyKing >= 64) return 0; // King not found
    
    int kingFile = fileOf(enemyKing);
    int kingRank = rankOf(enemyKing);
    
    // Count pieces attacking the enemy king
    int attackers = 0;
    int defenders = 0;
    
    // Check all squares around the king
    for (int df = -1; df <= 1; df++) {
        for (int dr = -1; dr <= 1; dr++) {
            if (df == 0 && dr == 0) continue;
            
            int newFile = kingFile + df;
            int newRank = kingRank + dr;
            
            if (newFile >= 0 && newFile < 8 && newRank >= 0 && newRank < 8) {
                Square sq = makeSquare(newFile, newRank);
                
                // Count attackers
                if (board.isSquareAttacked(sq, color)) {
                    attackers++;
                }
                
                // Count defenders
                if (board.isSquareAttacked(sq, enemyColor)) {
                    defenders++;
                }
            }
        }
    }
    
    // Bonus for having more attackers than defenders around the king
    if (attackers > defenders) {
        attack += (attackers - defenders) * 25;
    }
    
    // Bonus for pieces close to the enemy king
    for (Square sq = 0; sq < 64; sq++) {
        const Piece& piece = board.pieceAt(sq);
        if (!piece.isEmpty() && piece.color == color) {
            int file = fileOf(sq);
            int rank = rankOf(sq);
            
            // Calculate distance to enemy king
            int distance = std::max(abs(file - kingFile), abs(rank - kingRank));
            
            // Closer pieces get bigger bonuses
            if (distance <= 2) {
                switch (piece.type) {
                    case PieceType::QUEEN: attack += (3 - distance) * 15; break;
                    case PieceType::ROOK: attack += (3 - distance) * 10; break;
                    case PieceType::BISHOP: attack += (3 - distance) * 8; break;
                    case PieceType::KNIGHT: attack += (3 - distance) * 12; break;
                    case PieceType::PAWN: attack += (3 - distance) * 5; break;
                    default: break;
                }
            }
        }
    }
    
    return attack;
}

// Move ordering helper methods
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
    if (depth < 0 || depth >= 32) return;
    
    // Don't record captures as killer moves (they're already ordered first)
    if (move.isCapture) return;
    
    // Shift existing killer moves and add new one
    for (int i = MAX_KILLER_MOVES - 1; i > 0; i--) {
        killerMoves[depth][i] = killerMoves[depth][i-1];
    }
    killerMoves[depth][0] = move;
}

void SearchEngine::recordHistoryMove(const Move& move, int depth) {
    if (move.from >= 64 || move.to >= 64) return;
    
    // Increase history score for this move
    historyTable[move.from][move.to] += depth * depth;
    
    // Prevent overflow
    if (historyTable[move.from][move.to] > 1000000) {
        // Scale down all history scores
        for (int i = 0; i < 64; i++) {
            for (int j = 0; j < 64; j++) {
                historyTable[i][j] /= 2;
            }
        }
    }
}

bool SearchEngine::loadOpeningBook(const std::string& filename) {
    useOpeningBook = openingBook.loadFromFile(filename);
    return useOpeningBook;
}

bool SearchEngine::isMoveInOpeningBook(const Board& board, const Move& move) {
    if (!useOpeningBook) return false;
    
    // Get all possible opening moves for the current position
    std::vector<OpeningMove> bookMoves = openingBook.getMoves(board);
    
    // Check if the given move is in the opening book
    for (const auto& bookMove : bookMoves) {
        if (bookMove.move.from == move.from && 
            bookMove.move.to == move.to &&
            bookMove.move.promotion == move.promotion) {
            return true;
        }
    }
    
    return false;
}

bool SearchEngine::isSafeCapture(const Board& board, const Move& move) {
    if (!move.isCapture) return false;
    
    const Piece& capturedPiece = board.pieceAt(move.to);
    const Piece& attackingPiece = board.pieceAt(move.from);
    
    if (capturedPiece.isEmpty() || capturedPiece.color == attackingPiece.color) {
        return false;
    }
    
    // Check if the captured piece is defended
    bool isDefended = board.isSquareAttacked(move.to, capturedPiece.color);
    
    // Check if the attacking piece is defended
    bool attackerDefended = board.isSquareAttacked(move.from, attackingPiece.color);
    
    // A capture is safe if:
    // 1. The captured piece is not defended (free piece), OR
    // 2. The captured piece is defended but we're trading up in value
    if (!isDefended) {
        return true; // Free piece
    }
    
    // Check if it's a good trade
    int capturedValue = getPieceValue(capturedPiece.type);
    int attackerValue = getPieceValue(attackingPiece.type);
    
    if (capturedValue > attackerValue) {
        return true; // Trading up
    }
    
    if (capturedValue == attackerValue && attackerDefended) {
        return true; // Equal trade with defended attacker
    }
    
    return false; // Bad trade or hanging piece
}

int SearchEngine::evaluateCaptures(const Board& board) {
    int score = 0;
    
    // Generate all pseudo-legal moves to find captures
    std::vector<Move> pseudoMoves = MoveGenerator::generatePseudoLegalMoves(board);
    
    for (const Move& move : pseudoMoves) {
        if (move.isCapture) {
            const Piece& capturedPiece = board.pieceAt(move.to);
            const Piece& attackingPiece = board.pieceAt(move.from);
            
            if (!capturedPiece.isEmpty() && capturedPiece.color != attackingPiece.color) {
                int capturedValue = getPieceValue(capturedPiece.type);
                int attackerValue = getPieceValue(attackingPiece.type);
                
                // Check if the captured piece is defended
                bool isDefended = board.isSquareAttacked(move.to, capturedPiece.color);
                
                // Check if the attacking piece is defended
                bool attackerDefended = board.isSquareAttacked(move.from, attackingPiece.color);
                
                if (isDefended) {
                    // This is a trade - evaluate the exchange
                    int tradeValue = capturedValue - attackerValue;
                    
                    // If we're trading down (losing material), it's bad
                    if (tradeValue < 0) {
                        score -= abs(tradeValue) * 2; // Penalty for bad trades
                    } else if (tradeValue > 0) {
                        score += tradeValue; // Bonus for good trades
                    }
                    // Equal trades (tradeValue == 0) are neutral
                } else {
                    // This is a safe capture - pure gain
                    score += capturedValue * 2; // Bonus for safe captures
                }
                
                // Additional penalty if our attacking piece is undefended
                if (!attackerDefended && isDefended) {
                    score -= attackerValue; // Penalty for hanging our piece
                }
            }
        }
    }
    
    return score;
}

int SearchEngine::evaluateHungPieces(const Board& board) {
    int score = 0;
    
    // Check all pieces on the board
    for (Square sq = 0; sq < 64; sq++) {
        const Piece& piece = board.pieceAt(sq);
        if (!piece.isEmpty()) {
            // Check if this piece is attacked
            bool isAttacked = board.isSquareAttacked(sq, ~piece.color);
            
            if (isAttacked) {
                // Check if this piece is defended
                bool isDefended = board.isSquareAttacked(sq, piece.color);
                
                int pieceValue = getPieceValue(piece.type);
                
                if (!isDefended) {
                    // This piece is hanging (attacked but not defended)
                    if (piece.color == Color::WHITE) {
                        score -= pieceValue * 3; // Big penalty for hanging pieces
                    } else {
                        score += pieceValue * 3; // Big bonus for opponent's hanging pieces
                    }
                } else {
                    // This piece is attacked but defended - still a slight penalty
                    if (piece.color == Color::WHITE) {
                        score -= pieceValue / 2; // Small penalty for being under attack
                    } else {
                        score += pieceValue / 2; // Small bonus for attacking opponent's pieces
                    }
                }
            }
        }
    }
    
    return score;
}
