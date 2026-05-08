cmake_minimum_required(VERSION 3.16)
project(ChessEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

include_directories(src)

set(COMMON_SOURCES
    src/board.cpp
    src/movegen.cpp
    src/search.cpp
    src/opening_book.cpp
)

file(GLOB_RECURSE HEADERS "src/*.h")

add_executable(chess_engine src/main.cpp src/uci.cpp ${COMMON_SOURCES} ${HEADERS})

enable_testing()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()
