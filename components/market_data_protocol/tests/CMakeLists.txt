add_executable(obz_market_lab_market_data_protocol_tests
    codec_tests.cpp
)

target_link_libraries(obz_market_lab_market_data_protocol_tests
    PRIVATE
        obz_market_lab::market_data_protocol
        Catch2::Catch2WithMain
)

catch_discover_tests(obz_market_lab_market_data_protocol_tests)
