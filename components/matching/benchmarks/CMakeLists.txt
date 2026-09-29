add_executable(obz_market_lab_matching_benchmarks
    matching_benchmarks.cpp
)

target_link_libraries(obz_market_lab_matching_benchmarks
    PRIVATE
        obz_market_lab::matching
        benchmark::benchmark_main
)
