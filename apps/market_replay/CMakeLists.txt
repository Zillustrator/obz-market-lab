add_library(obz_market_replay_support
    src/console_writer.cpp
    src/jsonl_writer.cpp
    src/scenario_parser.cpp
)

add_library(obz_market_lab::replay_support ALIAS obz_market_replay_support)

target_include_directories(obz_market_replay_support
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(obz_market_replay_support
    PUBLIC
        cxx_std_20
)

target_link_libraries(obz_market_replay_support
    PUBLIC
        obz_market_lab::matching_runtime
)

add_executable(obz_market_replay
    src/main.cpp
)

target_compile_features(obz_market_replay
    PRIVATE
        cxx_std_20
)

target_link_libraries(obz_market_replay
    PRIVATE
        obz_market_lab::replay_support
)

if (OBZ_MARKET_LAB_BUILD_TESTS)
    add_subdirectory(tests)
endif()
