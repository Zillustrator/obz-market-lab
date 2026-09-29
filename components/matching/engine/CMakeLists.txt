add_library(obz_market_lab_matching
    src/engine.cpp
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

target_link_libraries(obz_market_lab_matching
    PRIVATE
        obz_market_lab::tracing
)

if (OBZ_MARKET_LAB_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if (OBZ_MARKET_LAB_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
