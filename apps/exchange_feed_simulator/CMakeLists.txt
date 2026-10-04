add_executable(obz_market_exchange_feed_simulator
    src/main.cpp
)

target_compile_features(obz_market_exchange_feed_simulator
    PRIVATE
        cxx_std_20
)

target_link_libraries(obz_market_exchange_feed_simulator
    PRIVATE
        obz_market_lab::market_data_feed
        obz_market_lab::matching_runtime
)
