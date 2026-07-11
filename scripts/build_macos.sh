#!/bin/bash
# Build a shippable macOS executable (universal: Apple Silicon + Intel).
#
# Output: dist/macos/chess_engine — fully self-contained (opening book
# embedded), runs on any Mac.

set -euo pipefail
cd "$(dirname "$0")/.."

OUT=dist/macos
mkdir -p "$OUT"

echo "Building universal macOS binary (arm64 + x86_64)..."
c++ -std=c++17 -O3 -DNDEBUG -Isrc \
    -arch arm64 -arch x86_64 \
    src/main.cpp src/uci.cpp src/board.cpp src/movegen.cpp \
    src/search.cpp src/opening_book.cpp src/eco_book.cpp \
    -o "$OUT/chess_engine"
strip "$OUT/chess_engine"

cat > "$OUT/README.txt" <<'EOF'
ChessEngine (UCI) for macOS (Apple Silicon and Intel)

chess_engine is fully self-contained (opening book included) — add it
as a UCI engine in your chess program (En Croissant, Cute Chess,
Banksia, ...).

If macOS blocks it ("cannot verify the developer"), remove the
quarantine flag once:
    xattr -d com.apple.quarantine chess_engine
EOF

echo "Done:"
ls -la "$OUT"
file "$OUT/chess_engine"
