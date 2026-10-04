add_executable(obz_market_data_listener
    src/main.cpp
)

target_compile_features(obz_market_data_listener
    PRIVATE
        cxx_std_20
)

target_link_libraries(obz_market_data_listener
    PRIVATE
        obz_market_lab::market_data_feed
)
