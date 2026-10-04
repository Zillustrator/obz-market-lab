add_executable(obz_market_lab_market_data_feed_tests
    feed_encoder_tests.cpp
    processor_tests.cpp
    sequenced_buffer_tests.cpp
    top_of_book_view_tests.cpp
)

target_link_libraries(obz_market_lab_market_data_feed_tests
    PRIVATE
        obz_market_lab::market_data_feed
        Catch2::Catch2WithMain
)

catch_discover_tests(obz_market_lab_market_data_feed_tests)
