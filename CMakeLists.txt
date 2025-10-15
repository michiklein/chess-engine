cmake_minimum_required(VERSION 3.16)
project(ChessEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Include directories
include_directories(src)

# Source files for UCI engine
set(UCI_SOURCES
    src/main.cpp
    src/board.cpp
    src/movegen.cpp
    src/search.cpp
    src/uci.cpp
    src/opening_book.cpp
)

# Source files for terminal game
set(TERMINAL_SOURCES
    src/terminal_game.cpp
    src/board.cpp
    src/movegen.cpp
    src/search.cpp
    src/opening_book.cpp
)

# Source files for tournament
set(TOURNAMENT_SOURCES
    src/engine_tournament.cpp
    src/board.cpp
    src/movegen.cpp
    src/search.cpp
    src/opening_book.cpp
)

file(GLOB_RECURSE HEADERS "src/*.h")

# Create the UCI engine executable
add_executable(chess_engine ${UCI_SOURCES} ${HEADERS})

# Create terminal game executable
add_executable(terminal_game ${TERMINAL_SOURCES} ${HEADERS})
target_include_directories(terminal_game PRIVATE src)

# Create tournament executable
add_executable(engine_tournament ${TOURNAMENT_SOURCES} ${HEADERS})
target_include_directories(engine_tournament PRIVATE src)


# Optional: Enable testing
enable_testing()

# Add test directory if it exists and has CMakeLists.txt
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()