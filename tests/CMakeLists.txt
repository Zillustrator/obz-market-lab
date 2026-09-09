# ------------------------------------------------------------------------------
# Test dependencies
# ------------------------------------------------------------------------------

include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
)

FetchContent_MakeAvailable(Catch2)

# ------------------------------------------------------------------------------
# Test executable
# ------------------------------------------------------------------------------

add_executable(obz_market_engine_tests
    engine_tests.cpp
    jsonl_writer_tests.cpp
    pipeline_tests.cpp
    scenario_parser_tests.cpp
)

target_link_libraries(obz_market_engine_tests
    PRIVATE
        obz_market::replay_support
        Catch2::Catch2WithMain
)

# ------------------------------------------------------------------------------
# CTest integration
# ------------------------------------------------------------------------------

include(Catch)
catch_discover_tests(obz_market_engine_tests)
