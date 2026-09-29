add_library(obz_market_lab_matching_runtime
    src/runner.cpp
)

add_library(obz_market_lab::matching_runtime ALIAS obz_market_lab_matching_runtime)

target_include_directories(obz_market_lab_matching_runtime
    PUBLIC
        include
)

target_compile_features(obz_market_lab_matching_runtime
    PUBLIC
        cxx_std_20
)

target_link_libraries(obz_market_lab_matching_runtime
    PUBLIC
        obz_market_lab::matching
    PRIVATE
        obz::bounded_blocking_queue
        obz_market_lab::tracing
)

if (OBZ_MARKET_LAB_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if (OBZ_MARKET_LAB_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
