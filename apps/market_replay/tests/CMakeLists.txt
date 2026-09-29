add_executable(obz_market_lab_replay_tests
    jsonl_writer_tests.cpp
    scenario_parser_tests.cpp
)

target_link_libraries(obz_market_lab_replay_tests
    PRIVATE
        obz_market_lab::replay_support
        Catch2::Catch2WithMain
)

catch_discover_tests(obz_market_lab_replay_tests)
