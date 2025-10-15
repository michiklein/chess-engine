#include "board.h"
#include "movegen.h"
#include "search.h"
#include "baseline_engine.h"
#include "types.h"
#include <iostream>
#include <string>
#include <vector>
#include <random>
#include <chrono>
#include <fstream>
#include <cmath>
#include <iomanip>
#include <sstream>

class EngineTournament {
private:
    Board board;
    SearchEngine engine1;  // Enhanced engine
    BaselineEngine engine2; // Baseline engine
    std::mt19937 rng;
    
    // Statistics
    int engine1Wins = 0;
    int engine2Wins = 0;
    int draws = 0;
    int totalGames = 0;
    
    // Game results log
    std::vector<std::string> gameResults;
    
public:
    EngineTournament() : rng(std::chrono::steady_clock::now().time_since_epoch().count()) {
        // Load opening book for both engines
        engine1.loadOpeningBook("src/eco.pgn");
        engine2.loadOpeningBook("src/eco.pgn");
        
        // Set different search depths if needed
        engine1.setMaxDepth(4);
        engine2.setMaxDepth(4);
        
        // Enable quiet mode to suppress verbose output
        engine1.setQuietMode(true);
        engine2.setQuietMode(true);
        
        std::cout << "Engine 1: Enhanced evaluation (material + mobility + center + king safety + hanging pieces)" << std::endl;
        std::cout << "Engine 2: Baseline evaluation (material only)" << std::endl;
    }
    
    void runTournament(int numGames) {
        std::cout << "Starting tournament: " << numGames << " games" << std::endl;
        std::cout << "Engine 1 vs Engine 2" << std::endl;
        std::cout << "===================" << std::endl;
        
        std::ofstream pgnFile("tournament_games.pgn");
        if (!pgnFile.is_open()) {
            std::cerr << "Could not create PGN file!" << std::endl;
            return;
        }
        
        for (int game = 1; game <= numGames; game++) {
            std::cout << "Game " << game << "/" << numGames << " - ";
            
            // Alternate who plays white
            bool engine1IsWhite = (game % 2 == 1);
            
            GameResult result = playGame(engine1IsWhite, pgnFile, game);
            
            // Update statistics
            totalGames++;
            switch (result) {
                case GameResult::ENGINE1_WIN:
                    engine1Wins++;
                    std::cout << "Engine 1 wins";
                    break;
                case GameResult::ENGINE2_WIN:
                    engine2Wins++;
                    std::cout << "Engine 2 wins";
                    break;
                case GameResult::DRAW_STALEMATE:
                    draws++;
                    std::cout << "Draw (Stalemate)";
                    break;
                case GameResult::DRAW_INSUFFICIENT_MATERIAL:
                    draws++;
                    std::cout << "Draw (Insufficient Material)";
                    break;
                case GameResult::DRAW_REPETITION:
                    draws++;
                    std::cout << "Draw (Repetition)";
                    break;
                case GameResult::DRAW_50_MOVE_RULE:
                    draws++;
                    std::cout << "Draw (50-Move Rule)";
                    break;
                case GameResult::DRAW_TOO_LONG:
                    draws++;
                    std::cout << "Draw (Too Long)";
                    break;
            }
            
            std::cout << " (Score: " << engine1Wins << "-" << engine2Wins << "-" << draws << ")" << std::endl;
        }
        
        pgnFile.close();
        printFinalResults();
        saveResultsToFile();
    }
    
private:
    enum class GameResult {
        ENGINE1_WIN,
        ENGINE2_WIN,
        DRAW_STALEMATE,
        DRAW_INSUFFICIENT_MATERIAL,
        DRAW_REPETITION,
        DRAW_50_MOVE_RULE,
        DRAW_TOO_LONG
    };
    
    GameResult playGame(bool engine1IsWhite, std::ofstream& pgnFile, int gameNumber) {
        board.setupStartingPosition();
        
        // Write PGN headers
        pgnFile << "[Event \"Engine Tournament\"]\n";
        pgnFile << "[Site \"Local\"]\n";
        pgnFile << "[Date \"" << getCurrentDate() << "\"]\n";
        pgnFile << "[Round \"" << gameNumber << "\"]\n";
        pgnFile << "[White \"" << (engine1IsWhite ? "Engine1" : "Engine2") << "\"]\n";
        pgnFile << "[Black \"" << (engine1IsWhite ? "Engine2" : "Engine1") << "\"]\n";
        
        std::string gamePGN = "";
        int moveCount = 0;
        const int MAX_MOVES = 200; // Prevent infinite games
        
        while (moveCount < MAX_MOVES) {
            // Check for game end conditions
            if (board.isCheckmate()) {
                // The side that just moved won
                bool whiteWon = (board.getSideToMove() == Color::BLACK);
                bool engine1Won = (engine1IsWhite && whiteWon) || (!engine1IsWhite && !whiteWon);
                
                gamePGN += (whiteWon ? "1-0" : "0-1");
                pgnFile << gamePGN << "\n\n";
                return engine1Won ? GameResult::ENGINE1_WIN : GameResult::ENGINE2_WIN;
            }
            
            if (board.isStalemate()) {
                gamePGN += "1/2-1/2";
                pgnFile << gamePGN << "\n\n";
                return GameResult::DRAW_STALEMATE;
            }
            
            // Get the move from the appropriate engine
            SearchEngine& currentEngine = (board.getSideToMove() == Color::WHITE) == engine1IsWhite ? engine1 : engine2;
            SearchResult result = currentEngine.search(board, 4);
            Move move = result.bestMove;
            
            // Add move to PGN BEFORE making the move (so we can get piece info)
            if (board.getSideToMove() == Color::WHITE) {
                gamePGN += std::to_string((moveCount / 2) + 1) + ". ";
            }
            gamePGN += moveToAlgebraic(move) + " ";
            
            // Make the move
            board.makeMove(move);
            moveCount++;
        }
        
        // Game too long - consider it a draw
        gamePGN += "1/2-1/2";
        pgnFile << gamePGN << "\n\n";
        return GameResult::DRAW_TOO_LONG;
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
        
        // Get the piece that's moving (before the move is made)
        Piece movingPiece = board.pieceAt(move.from);
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
        
        // Add capture indicator
        if (move.isCapture) {
            if (movingPieceType == PieceType::PAWN) {
                // For pawn captures, include the file BEFORE the x
                result += static_cast<char>('a' + fileOf(move.from));
            }
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
    
    void printFinalResults() {
        std::cout << "\n===================" << std::endl;
        std::cout << "TOURNAMENT RESULTS" << std::endl;
        std::cout << "===================" << std::endl;
        std::cout << "Total Games: " << totalGames << std::endl;
        std::cout << "Engine 1 Wins: " << engine1Wins << " (" << (100.0 * engine1Wins / totalGames) << "%)" << std::endl;
        std::cout << "Engine 2 Wins: " << engine2Wins << " (" << (100.0 * engine2Wins / totalGames) << "%)" << std::endl;
        std::cout << "Draws: " << draws << " (" << (100.0 * draws / totalGames) << "%)" << std::endl;
        
        if (engine1Wins > engine2Wins) {
            std::cout << "\n🏆 Engine 1 is the winner!" << std::endl;
        } else if (engine2Wins > engine1Wins) {
            std::cout << "\n🏆 Engine 2 is the winner!" << std::endl;
        } else {
            std::cout << "\n🤝 Tournament is a tie!" << std::endl;
        }
        
        // Calculate confidence interval (rough estimate)
        double engine1WinRate = (double)engine1Wins / totalGames;
        double margin = 1.96 * sqrt(engine1WinRate * (1 - engine1WinRate) / totalGames);
        std::cout << "\nEngine 1 win rate: " << (engine1WinRate * 100) << "% ± " << (margin * 100) << "% (95% confidence)" << std::endl;
    }
    
    void saveResultsToFile() {
        std::ofstream file("tournament_results.txt");
        if (file.is_open()) {
            file << "Tournament Results\n";
            file << "==================\n";
            file << "Total Games: " << totalGames << "\n";
            file << "Engine 1 Wins: " << engine1Wins << " (" << (100.0 * engine1Wins / totalGames) << "%)\n";
            file << "Engine 2 Wins: " << engine2Wins << " (" << (100.0 * engine2Wins / totalGames) << "%)\n";
            file << "Draws: " << draws << " (" << (100.0 * draws / totalGames) << "%)\n";
            file.close();
            std::cout << "\nResults saved to tournament_results.txt" << std::endl;
        }
    }
    
    std::string getCurrentDate() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time_t);
        
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y.%m.%d");
        return oss.str();
    }
};

int main() {
    EngineTournament tournament;
    tournament.runTournament(10); // Test improved engine
    return 0;
}
