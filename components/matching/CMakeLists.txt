add_library(obz_market_lab_matching
    src/matching_engine.cpp
    src/order_book.cpp
)

add_library(obz_market_lab::matching ALIAS obz_market_lab_matching)

target_include_directories(obz_market_lab_matching
    PUBLIC
        include
)

target_compile_features(obz_market_lab_matching
    PUBLIC
        cxx_std_20
)

if (OBZ_MARKET_LAB_ENABLE_TRACY)
    target_link_libraries(obz_market_lab_matching
        PRIVATE
            Tracy::TracyClient
    )

    target_compile_definitions(obz_market_lab_matching
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
