# ------------------------------------------------------------------------------
# Benchmark dependencies
# ------------------------------------------------------------------------------

include(FetchContent)

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.9.1
)

FetchContent_MakeAvailable(benchmark)

# ------------------------------------------------------------------------------
# Benchmark executable
# ------------------------------------------------------------------------------

add_executable(obz_market_engine_benchmarks
    engine_benchmarks.cpp
    pipeline_benchmarks.cpp
)

target_link_libraries(obz_market_engine_benchmarks
    PRIVATE
        obz_market::pipeline
        benchmark::benchmark_main
)
