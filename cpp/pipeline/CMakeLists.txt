add_library(obz_market_pipeline
    src/engine_runner.cpp
)

add_library(obz_market::pipeline ALIAS obz_market_pipeline)

target_include_directories(obz_market_pipeline
    PUBLIC
        include
)

target_compile_features(obz_market_pipeline
    PUBLIC
        cxx_std_20
)

target_link_libraries(obz_market_pipeline
    PUBLIC
        obz_market::engine
        obz::bounded_blocking_queue
)

if (OBZ_MARKET_LAB_ENABLE_TRACY)
    target_link_libraries(obz_market_pipeline
        PRIVATE
            Tracy::TracyClient
    )

    target_compile_definitions(obz_market_pipeline
        PRIVATE
            OBZ_MARKET_LAB_ENABLE_TRACY
    )
endif()
