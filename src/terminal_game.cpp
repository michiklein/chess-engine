#include "board.h"
#include "movegen.h"
#include "search.h"
#include "types.h"
#include <iostream>
#include <string>
#include <vector>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <iomanip>

class TerminalGame {
private:
    Board board;
    SearchEngine search;
    Color userColor;
    Color engineColor;
    
public:
    TerminalGame() {
        board.setupStartingPosition();
        search.setMaxDepth(8); // Increased depth with bitboard performance
        
        // Load opening book
        if (search.loadOpeningBook("src/eco.pgn")) {
            std::cout << "Opening book loaded successfully" << std::endl;
        } else {
            std::cout << "Warning: Could not load opening book" << std::endl;
        }
    }
    
    void run() {
        std::cout << "Welcome to Chess Engine Terminal Game!" << std::endl;
        std::cout << "=====================================" << std::endl;
        
        // Ask user which color they want to play
        std::cout << "Which color would you like to play? (w for white, b for black): ";
        char colorChoice;
        std::cin >> colorChoice;
        
        if (std::tolower(colorChoice) == 'w') {
            userColor = Color::WHITE;
            engineColor = Color::BLACK;
            std::cout << "You are playing as White. The engine is Black." << std::endl;
        } else {
            userColor = Color::BLACK;
            engineColor = Color::WHITE;
            std::cout << "You are playing as Black. The engine is White." << std::endl;
        }
        
        std::cout << "\nGame started! Enter moves in algebraic notation (e.g., e4, Nf3, O-O)." << std::endl;
        std::cout << "Type 'quit' to exit the game." << std::endl;
        std::cout << "=====================================" << std::endl;
        
        // Main game loop
        while (true) {
            // Check for game end conditions
            if (board.isCheckmate()) {
                if (board.getSideToMove() == userColor) {
                    std::cout << "Checkmate! You lose." << std::endl;
                } else {
                    std::cout << "Checkmate! You win!" << std::endl;
                }
                break;
            }
            
            if (board.isStalemate()) {
                std::cout << "Stalemate! The game is a draw." << std::endl;
                break;
            }
            
            // Display current position info
            std::cout << "\nMove " << board.getFullMoveNumber() << ": ";
            if (board.getSideToMove() == Color::WHITE) {
                std::cout << "White to move";
            } else {
                std::cout << "Black to move";
            }
            
            if (board.isInCheck(board.getSideToMove())) {
                std::cout << " (CHECK!)";
            }
            std::cout << std::endl;
            
            if (board.getSideToMove() == userColor) {
                // User's turn
                if (!handleUserMove()) {
                    break; // User wants to quit
                }
            } else {
                // Engine's turn
                handleEngineMove();
            }
        }
        
        std::cout << "\nThanks for playing!" << std::endl;
    }
    
private:
    bool isValidMove(const Move& move, const std::string& moveStr) {
        // Check if it's a castling move
        if (moveStr == "O-O" || moveStr == "O-O-O" || moveStr == "0-0" || moveStr == "0-0-0") {
            return move.isCastle;
        }
        
        // For other moves, check if both squares are valid (0-63)
        return move.from < 64 && move.to < 64;
    }
    
    bool handleUserMove() {
        std::string moveStr;
        
        while (true) {
            std::cout << "Your move: ";
            std::cin >> moveStr;
            
            if (moveStr == "quit") {
                return false;
            }
            
            Move move = parseMove(moveStr);
            if (!isValidMove(move, moveStr)) {
                std::cout << "Invalid move format. Please use algebraic notation (e.g., e4, Nf3, O-O)." << std::endl;
                continue;
            }
            
            // Check if move is legal
            if (!MoveGenerator::isLegalMove(board, move)) {
                std::cout << "Illegal move. Please try again." << std::endl;
                continue;
            }
            
            // Get the move string BEFORE making the move
            std::string moveWithCheck = moveToAlgebraicWithCheck(move);
            
            // Make the move
            board.makeMove(move);
            std::cout << "You played: " << moveWithCheck << std::endl;
            break;
        }
        
        return true;
    }
    
    void handleEngineMove() {
        std::cout << "Engine is thinking..." << std::endl;
        
        // Use the search engine to find the best move
        SearchResult result = search.search(board, 5);
        Move engineMove = result.bestMove;
        
        // Get the move string before making the move
        std::string moveWithCheck = moveToAlgebraicWithCheck(engineMove);
        
        board.makeMove(engineMove);
        // Show evaluation from engine's perspective (positive = good for engine)
        // Convert to decimal since piece values are now in centipawns (pawn = 1)
        double evalDecimal = result.score / 100.0;
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2);
        if (evalDecimal >= 0) {
            oss << "+" << evalDecimal;
        } else {
            oss << evalDecimal;
        }
        std::cout << "Engine plays: " << moveWithCheck << " (eval: " << oss.str() << ")" << std::endl;
    }
    
    Move parseMove(const std::string& moveStr) {
        Move move;
        
        // Handle castling
        if (moveStr == "O-O" || moveStr == "0-0") {
            // Kingside castling
            if (board.getSideToMove() == Color::WHITE) {
                move.from = E1;
                move.to = G1;
            } else {
                move.from = E8;
                move.to = G8;
            }
            move.isCastle = true;
            return move;
        } else if (moveStr == "O-O-O" || moveStr == "0-0-0") {
            // Queenside castling
            if (board.getSideToMove() == Color::WHITE) {
                move.from = E1;
                move.to = C1;
            } else {
                move.from = E8;
                move.to = C8;
            }
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
        
        // Handle standard algebraic notation (e4, Nf3, Qdd5, etc.)
        if (moveStr.length() >= 2 && moveStr.length() <= 6) {
            return parseAlgebraicMove(moveStr);
        }
        
        return move; // Invalid move
    }
    
    Move parseAlgebraicMove(const std::string& moveStr) {
        // Generate all legal moves and try to match
        std::vector<Move> legalMoves = MoveGenerator::generateLegalMoves(board);
        
        // Deduplicate moves (keep only the first occurrence of each unique move)
        std::vector<Move> uniqueMoves;
        for (const Move& move : legalMoves) {
            bool found = false;
            for (const Move& uniqueMove : uniqueMoves) {
                if (move.from == uniqueMove.from && move.to == uniqueMove.to && 
                    move.isCapture == uniqueMove.isCapture && move.promotion == uniqueMove.promotion) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                uniqueMoves.push_back(move);
            }
        }
        
        // Cache move strings to avoid multiple calls to moveToAlgebraic
        std::vector<std::pair<Move, std::string>> moveStrings;
        for (const Move& legalMove : uniqueMoves) {
            std::string legalMoveStr = moveToAlgebraic(legalMove);
            moveStrings.push_back({legalMove, legalMoveStr});
        }
        
        for (const auto& pair : moveStrings) {
            if (pair.second == moveStr) {
                return pair.first;
            }
        }
        
        // If no exact match, try to parse manually for disambiguation
        return parseDisambiguatedMove(moveStr);
    }
    
    Move parseDisambiguatedMove(const std::string& moveStr) {
        Move move;
        
        // Handle pawn moves (e4, exd5, e8=Q)
        if (moveStr.length() >= 2 && moveStr.length() <= 5) {
            // Check if it's a pawn move (no piece letter at start)
            if (moveStr[0] >= 'a' && moveStr[0] <= 'h') {
                return parsePawnMove(moveStr);
            }
        }
        
        // Handle piece moves (Nf3, Qdd5, Bxc5, etc.)
        if (moveStr.length() >= 3) {
            PieceType pieceType = PieceType::NONE;
            char pieceChar = moveStr[0];
            
            // Determine piece type
            switch (pieceChar) {
                case 'N': pieceType = PieceType::KNIGHT; break;
                case 'B': pieceType = PieceType::BISHOP; break;
                case 'R': pieceType = PieceType::ROOK; break;
                case 'Q': pieceType = PieceType::QUEEN; break;
                case 'K': pieceType = PieceType::KING; break;
                default: return move; // Invalid piece
            }
            
            // Find destination square (last 2 characters)
            std::string destStr = moveStr.substr(moveStr.length() - 2);
            int toFile = destStr[0] - 'a';
            int toRank = destStr[1] - '1';
            
            if (toFile < 0 || toFile >= 8 || toRank < 0 || toRank >= 8) {
                return move; // Invalid destination
            }
            
            Square toSquare = makeSquare(toFile, toRank);
            
            // Find the piece that can move to this square
            std::vector<Move> legalMoves = MoveGenerator::generateLegalMoves(board);
            for (const Move& legalMove : legalMoves) {
                if (legalMove.to == toSquare) {
                    Piece movingPiece = board.pieceAt(legalMove.from);
                    if (movingPiece.type == pieceType) {
                        // Check disambiguation
                        if (isDisambiguationMatch(moveStr, legalMove)) {
                            return legalMove;
                        }
                    }
                }
            }
        }
        
        return move; // Invalid move
    }
    
    Move parsePawnMove(const std::string& moveStr) {
        Move move;
        
        // Simple pawn move (e4)
        if (moveStr.length() == 2) {
            int toFile = moveStr[0] - 'a';
            int toRank = moveStr[1] - '1';
            
            if (toFile >= 0 && toFile < 8 && toRank >= 0 && toRank < 8) {
                Square toSquare = makeSquare(toFile, toRank);
                
                // Find pawn that can move to this square
                std::vector<Move> legalMoves = MoveGenerator::generateLegalMoves(board);
                for (const Move& legalMove : legalMoves) {
                    if (legalMove.to == toSquare) {
                        Piece movingPiece = board.pieceAt(legalMove.from);
                        if (movingPiece.type == PieceType::PAWN) {
                            return legalMove;
                        }
                    }
                }
            }
        }
        
        // Pawn capture (exd5)
        if (moveStr.length() >= 4 && moveStr[1] == 'x') {
            int fromFile = moveStr[0] - 'a';
            int toFile = moveStr[2] - 'a';
            int toRank = moveStr[3] - '1';
            
            if (fromFile >= 0 && fromFile < 8 && toFile >= 0 && toFile < 8 && toRank >= 0 && toRank < 8) {
                Square toSquare = makeSquare(toFile, toRank);
                
                // Find pawn from this file that can capture
                std::vector<Move> legalMoves = MoveGenerator::generateLegalMoves(board);
                for (const Move& legalMove : legalMoves) {
                    if (legalMove.to == toSquare && fileOf(legalMove.from) == fromFile) {
                        Piece movingPiece = board.pieceAt(legalMove.from);
                        if (movingPiece.type == PieceType::PAWN) {
                            return legalMove;
                        }
                    }
                }
            }
        }
        
        return move;
    }
    
    bool isDisambiguationMatch(const std::string& moveStr, const Move& legalMove) {
        // Handle file disambiguation (Qdd5)
        if (moveStr.length() >= 4 && moveStr[1] >= 'a' && moveStr[1] <= 'h') {
            int disambigFile = moveStr[1] - 'a';
            return fileOf(legalMove.from) == disambigFile;
        }
        
        // Handle rank disambiguation (Q1d5)
        if (moveStr.length() >= 4 && moveStr[1] >= '1' && moveStr[1] <= '8') {
            int disambigRank = moveStr[1] - '1';
            return rankOf(legalMove.from) == disambigRank;
        }
        
        // Handle file+rank disambiguation (Qd1d5)
        if (moveStr.length() >= 5 && moveStr[1] >= 'a' && moveStr[1] <= 'h' && 
            moveStr[2] >= '1' && moveStr[2] <= '8') {
            int disambigFile = moveStr[1] - 'a';
            int disambigRank = moveStr[2] - '1';
            return fileOf(legalMove.from) == disambigFile && rankOf(legalMove.from) == disambigRank;
        }
        
        // No disambiguation needed
        return true;
    }
    
    std::string moveToAlgebraic(const Move& move) {
        std::string result;
        
        // Handle castling
        if (move.isCastle) {
            if (move.to == G1 || move.to == G8) {
                return "O-O";
            } else {
                return "O-O-O";
            }
        }
        
        Piece movingPiece = board.pieceAt(move.from);
        Piece capturedPiece = board.pieceAt(move.to);
        
        // Store the piece type to avoid issues with board state changes
        PieceType movingPieceType = movingPiece.type;
        
        
        // Add piece symbol (except for pawns)
        if (movingPieceType != PieceType::PAWN) {
            switch (movingPieceType) {
                case PieceType::KNIGHT: result += "N"; break;
                case PieceType::BISHOP: result += "B"; break;
                case PieceType::ROOK: result += "R"; break;
                case PieceType::QUEEN: result += "Q"; break;
                case PieceType::KING: result += "K"; break;
                default: break;
            }
        }
        
        // Add capture indicator (only for actual captures, not en passant)
        if (move.isCapture && !move.isEnPassant) {
            if (movingPieceType == PieceType::PAWN) {
                // For pawn captures, include the file BEFORE the x
                result += static_cast<char>('a' + fileOf(move.from));
            }
            result += "x";
        } else if (move.isEnPassant) {
            // For en passant, include the file and use x
            result += static_cast<char>('a' + fileOf(move.from));
            result += "x";
        }
        
        
        
        
        // Add destination square
        result += static_cast<char>('a' + fileOf(move.to));
        result += static_cast<char>('1' + rankOf(move.to));
        
        
        // Add promotion
        if (move.promotion != PieceType::NONE) {
            result += "=";
            switch (move.promotion) {
                case PieceType::QUEEN:  result += "Q"; break;
                case PieceType::ROOK:   result += "R"; break;
                case PieceType::BISHOP: result += "B"; break;
                case PieceType::KNIGHT: result += "N"; break;
                default: break;
            }
        }
        
        return result;
    }
    
    std::string moveToAlgebraicWithCheck(const Move& move) {
        std::string result = moveToAlgebraic(move);
        
        // Check if the move gives check (board already has the move made)
        Color opponentColor = (board.getSideToMove() == Color::WHITE) ? Color::BLACK : Color::WHITE;
        if (board.isInCheck(opponentColor)) {
            if (board.isCheckmate()) {
                result += "#"; // Checkmate
            } else {
                result += "+"; // Check
            }
        }
        
        return result;
    }
    
    std::string moveToString(const Move& move) {
        std::string result;
        
        // From square
        result += static_cast<char>('a' + fileOf(move.from));
        result += static_cast<char>('1' + rankOf(move.from));
        
        // To square
        result += static_cast<char>('a' + fileOf(move.to));
        result += static_cast<char>('1' + rankOf(move.to));
        
        // Promotion
        if (move.promotion != PieceType::NONE) {
            switch (move.promotion) {
                case PieceType::QUEEN:  result += 'q'; break;
                case PieceType::ROOK:   result += 'r'; break;
                case PieceType::BISHOP: result += 'b'; break;
                case PieceType::KNIGHT: result += 'n'; break;
                default: break;
            }
        }
        
        return result;
    }
};

int main() {
    TerminalGame game;
    game.run();
    return 0;
}
