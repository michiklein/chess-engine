#!/bin/bash
# Cross-compile a self-contained Windows executable using mingw-w64.
#
#   brew install mingw-w64     (macOS)
#   apt install g++-mingw-w64  (Linux)
#
# Output: dist/windows/chess_engine.exe (+ eco.pgn opening book)

set -euo pipefail
cd "$(dirname "$0")/.."

CXX=x86_64-w64-mingw32-g++
if ! command -v "$CXX" >/dev/null; then
    echo "error: $CXX not found — install mingw-w64 first" >&2
    exit 1
fi

OUT=dist/windows
mkdir -p "$OUT"

echo "Cross-compiling for Windows (x86_64)..."
"$CXX" -std=c++17 -O3 -DNDEBUG -Isrc \
    src/main.cpp src/uci.cpp src/board.cpp src/movegen.cpp \
    src/search.cpp src/opening_book.cpp src/eco_book.cpp \
    -static -s \
    -o "$OUT/chess_engine.exe"

cat > "$OUT/README.txt" <<'EOF'
ChessEngine (UCI) for Windows 64-bit

chess_engine.exe is fully self-contained (opening book included) —
just add it as a UCI engine in your chess program (Arena,
En Croissant, Cute Chess, ...).
EOF

echo "Done:"
ls -la "$OUT"
