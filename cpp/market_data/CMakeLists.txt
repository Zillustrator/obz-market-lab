add_library(obz_market_market_data
    src/codec.cpp
)

add_library(obz_market::market_data ALIAS obz_market_market_data)

target_include_directories(obz_market_market_data
    PUBLIC
        include
)

target_compile_features(obz_market_market_data
    PUBLIC
        cxx_std_20
)

target_link_libraries(obz_market_market_data
    PUBLIC
        obz_market::engine
    PRIVATE
        obz::endian
)
