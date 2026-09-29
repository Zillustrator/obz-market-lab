add_executable(obz_market_lab_matching_runtime_benchmarks
    matching_runtime_benchmarks.cpp
)

target_link_libraries(obz_market_lab_matching_runtime_benchmarks
    PRIVATE
        obz_market_lab::matching_runtime
        benchmark::benchmark_main
)
