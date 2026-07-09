add_executable(perft_test perft.cpp ${CMAKE_SOURCE_DIR}/src/board.cpp ${CMAKE_SOURCE_DIR}/src/movegen.cpp)
target_include_directories(perft_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME perft COMMAND perft_test)
