#!/bin/bash

# Chess Engine Build Script
# Builds both the UCI engine and terminal game

echo "🏗️  Building Chess Engine..."
echo "================================"

# Create build directory if it doesn't exist
mkdir -p build

# Change to build directory
cd build

# Run CMake configuration
echo "📋 Configuring build..."
cmake ..

# Build both executables
echo "🔨 Building UCI engine..."
make chess_engine

echo "🎮 Building terminal game..."
make terminal_game

echo "🏆 Building tournament..."
make engine_tournament

echo ""
echo "✅ Build complete!"
echo "================================"
echo "📁 Executables created:"
echo "   • UCI Engine: ./build/chess_engine"
echo "   • Terminal Game: ./build/terminal_game"
echo "   • Tournament: ./build/engine_tournament"
echo ""
echo "🚀 To run:"
echo "   • UCI Engine: ./build/chess_engine"
echo "   • Terminal Game: ./build/terminal_game"
echo "   • Tournament: ./build/engine_tournament"
echo "================================"
