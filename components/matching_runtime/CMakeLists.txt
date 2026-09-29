add_library(obz_market_lab_matching_runtime
    src/engine_runner.cpp
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
)

if (OBZ_MARKET_LAB_ENABLE_TRACY)
    target_link_libraries(obz_market_lab_matching_runtime
        PRIVATE
            Tracy::TracyClient
    )

    target_compile_definitions(obz_market_lab_matching_runtime
        PRIVATE
            OBZ_MARKET_LAB_ENABLE_TRACY
    )
endif()

if (OBZ_MARKET_LAB_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if (OBZ_MARKET_LAB_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
