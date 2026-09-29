add_library(obz_market_lab_market_data_protocol
    src/codec.cpp
)

add_library(
    obz_market_lab::market_data_protocol
    ALIAS obz_market_lab_market_data_protocol
)

target_include_directories(obz_market_lab_market_data_protocol
    PUBLIC
        include
)

target_compile_features(obz_market_lab_market_data_protocol
    PUBLIC
        cxx_std_20
)

target_link_libraries(obz_market_lab_market_data_protocol
    PUBLIC
        obz_market_lab::matching
    PRIVATE
        obz::endian
)

if (OBZ_MARKET_LAB_BUILD_TESTS)
    add_subdirectory(tests)
endif()
