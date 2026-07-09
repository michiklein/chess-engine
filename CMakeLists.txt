cmake_minimum_required(VERSION 3.16)
project(ChessEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default to an optimized build; without this the engine is built at -O0
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

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

# Keep the opening book next to the binary so it is found regardless of the
# working directory the engine is launched from
add_custom_command(TARGET chess_engine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/src/eco.pgn
            $<TARGET_FILE_DIR:chess_engine>/eco.pgn)

enable_testing()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()
