add_executable(obz_market_lab_market_data_feed_benchmarks
    feed_benchmarks.cpp
)

target_link_libraries(obz_market_lab_market_data_feed_benchmarks
    PRIVATE
        obz_market_lab::market_data_feed
        benchmark::benchmark_main
)
