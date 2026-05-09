#!/bin/bash

# Chess Engine Build Script
# Builds the UCI engine only and provides a cleanup helper

set -euo pipefail

echo "🏗️  Building Chess Engine..."
echo "================================"

# Create build directory if it doesn't exist
mkdir -p build

# Change to build directory
cd build

# Run CMake configuration
echo "📋 Configuring build..."
cmake ..

# Build the UCI engine target
echo "🔨 Building UCI engine..."
make chess_engine

echo "\n✅ Build complete!"
echo "================================"
echo "📁 Executable created:"
echo "   • UCI Engine: ./build/chess_engine"
echo "\nTo remove build artifacts use: ./cleanup.sh"
echo "================================"
