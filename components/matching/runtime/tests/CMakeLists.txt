add_executable(obz_market_lab_matching_runtime_tests
    runner_tests.cpp
)

target_link_libraries(obz_market_lab_matching_runtime_tests
    PRIVATE
        obz_market_lab::matching_runtime
        Catch2::Catch2WithMain
)

catch_discover_tests(obz_market_lab_matching_runtime_tests)
