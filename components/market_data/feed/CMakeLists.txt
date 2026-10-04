add_library(obz_market_lab_market_data_feed
    src/feed_encoder.cpp
    src/multicast_publisher.cpp
    src/multicast_receiver.cpp
)

add_library(obz_market_lab::market_data_feed ALIAS obz_market_lab_market_data_feed)

target_include_directories(obz_market_lab_market_data_feed
    PUBLIC
        include
)

target_compile_features(obz_market_lab_market_data_feed
    PUBLIC
        cxx_std_20
)

target_link_libraries(obz_market_lab_market_data_feed
    PUBLIC
        obz_market_lab::matching
        obz_market_lab::market_data_protocol
        obz::transport
)

if (OBZ_MARKET_LAB_BUILD_TESTS)
    add_subdirectory(tests)
endif()
