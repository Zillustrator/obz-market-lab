add_executable(obz_market_lab_matching_tests
    engine_tests.cpp
)

target_link_libraries(obz_market_lab_matching_tests
    PRIVATE
        obz_market_lab::matching
        Catch2::Catch2WithMain
)

catch_discover_tests(obz_market_lab_matching_tests)
