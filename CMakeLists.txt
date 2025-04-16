cmake_minimum_required(VERSION 3.28)
project(RTS_UAV)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(external)

# Example 1: Main program core_gmp (using GMP + MIRACL)
add_executable(core_gmp main.cpp)
target_link_libraries(core_gmp PRIVATE miracl_core gmp::gmp gmp::gmpxx)

# Example 2: benchmark (using Google Benchmark)
add_executable(benchmark_test benchmark.cpp)
target_link_libraries(benchmark_test PRIVATE benchmark::benchmark benchmark::benchmark_main)









