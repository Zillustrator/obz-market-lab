add_library(obz_market_replay_support
    console_writer.cpp
    jsonl_writer.cpp
    scenario_parser.cpp
)

add_library(obz_market::replay_support ALIAS obz_market_replay_support)

target_include_directories(obz_market_replay_support
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(obz_market_replay_support
    PUBLIC
        cxx_std_20
)

target_link_libraries(obz_market_replay_support
    PUBLIC
        obz_market::pipeline
)

add_executable(obz_market_replay
    main.cpp
)

target_compile_features(obz_market_replay
    PRIVATE
        cxx_std_20
)

target_link_libraries(obz_market_replay
    PRIVATE
        obz_market::replay_support
)
