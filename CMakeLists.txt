cmake_minimum_required(VERSION 3.28)
project(RTS_UAV)
set(CMAKE_CXX_STANDARD 17)

# Build external libraries (e.g., MIRACL Core, GMP, Benchmark)
add_subdirectory(external)
add_subdirectory(RTS-websocket)
add_subdirectory(netSim)

# Include paths for common headers shared across all schemes
include_directories(
        ${CMAKE_SOURCE_DIR}/common/include
)

# Gather all common source files used by the schemes
file(GLOB COMMON_SRC
        ${CMAKE_SOURCE_DIR}/common/src/*.cpp
)

# Function to define a benchmark target for a given signature scheme
function(add_scheme_executable name)
    set(SCHEME_DIR ${CMAKE_SOURCE_DIR}/${name})

    # Collect all source files within the scheme's src/ directory
    file(GLOB SCHEME_SRC
            ${SCHEME_DIR}/src/*.cpp
    )

    # Add include paths for both the scheme-specific and common headers
    include_directories(
            ${SCHEME_DIR}/include
            ${CMAKE_SOURCE_DIR}/common/include
    )

    # Define the executable target with combined common and scheme sources
    add_executable(benchmark_${name}
            ${COMMON_SRC}
            ${SCHEME_SRC}
    )

    # Link the target against required libraries: MIRACL Core, GMP, and Google Benchmark
    target_link_libraries(benchmark_${name}
            PRIVATE
            miracl_core
            gmp::gmp
            gmp::gmpxx
            benchmark::benchmark
    )
endfunction()

# Define benchmark target for the RTS signature scheme
add_scheme_executable(RTS)
add_scheme_executable(FROST)
add_scheme_executable(MuSig2)
add_scheme_executable(MTS)
