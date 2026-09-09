add_library(obz_market_engine
    src/matching_engine.cpp
    src/order_book.cpp
)

add_library(obz_market::engine ALIAS obz_market_engine)

target_include_directories(obz_market_engine
    PUBLIC
        include
)

target_compile_features(obz_market_engine
    PUBLIC
        cxx_std_20
)

target_link_libraries(obz_market_engine
    PUBLIC
        obz::endian
)

if (OBZ_MARKET_LAB_ENABLE_TRACY)
    target_link_libraries(obz_market_engine
        PRIVATE
            Tracy::TracyClient
    )

    target_compile_definitions(obz_market_engine
        PRIVATE
            OBZ_MARKET_LAB_ENABLE_TRACY
    )
endif()
