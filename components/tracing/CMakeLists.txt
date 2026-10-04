add_library(obz_market_lab_tracing INTERFACE)

add_library(obz_market_lab::tracing ALIAS obz_market_lab_tracing)

target_include_directories(obz_market_lab_tracing
    INTERFACE
        include
)

target_compile_features(obz_market_lab_tracing
    INTERFACE
        cxx_std_20
)

if (OBZ_MARKET_LAB_ENABLE_TRACY)
    target_link_libraries(obz_market_lab_tracing
        INTERFACE
            Tracy::TracyClient
    )

    target_compile_definitions(obz_market_lab_tracing
        INTERFACE
            OBZ_MARKET_LAB_ENABLE_TRACY
    )
endif()
